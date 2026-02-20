#!/usr/bin/env python3
import json
import socket
import time
import errno
from pathlib import Path

import numpy as np
import sounddevice as sd

from openwakeword.model import Model
import openwakeword

HOST, PORT = "127.0.0.1", 3939

SAMPLE_RATE = 16000
FRAME_MS = 50
FRAME_SAMPLES = int(SAMPLE_RATE * FRAME_MS / 1000)

VAD_FRAME = 512

THRESH = 0.73 # Higher is more precise, too high might not recognize
COOLDOWN_S = .9

HANDOFF_S = 12.5

def find_model_files():
    pkg_dir = Path(openwakeword.__file__).resolve().parent

    candidates = [
        pkg_dir / "resources" / "models",
        pkg_dir / "models",
        Path.cwd() / "python" / "wake" / "models",
    ]

    exclude_prefixes = ("embedding_model", "melspectrogram", "silero_vad")

    for d in candidates:
        if not d.is_dir():
            continue

        models = []
        for p in sorted(d.glob("*.onnx")):
            if p.stem.startswith(exclude_prefixes):
                continue
            models.append(p)

        if models:
            return models

    return []

def pick_input_device():
    devices = sd.query_devices()
    try:
        default_in = sd.default.device[0]
        if default_in is not None and default_in >= 0:
            info = sd.query_devices(default_in)
            if info.get("max_input_channels", 0) > 0:
                return int(default_in), info
    except Exception:
        pass

    for i, info in enumerate(devices):
        if info.get("max_input_channels", 0) > 0:
            return i, info

    return None, None

def main():

    openwakeword.utils.download_models()

    model_files = find_model_files()
    if not model_files:
        raise SystemExit(
            "openwakeword: no bundled model files found in this install.\n"
            "Fix: (1) upgrade openwakeword so it includes models, OR\n"
            "      (2) download models and point to them explicitly."
        )
    
    print("Loading models:")
    for p in model_files:
        print(" -", p.name)

    dev_idx, dev_info = pick_input_device()
    if dev_idx is None:
        raise SystemExit(
            "No input audio devices found (PortAudio sees none).\n"
            "Docker: run with --device /dev/snd --group-add audio.\n"
            "Host: check /dev/snd exists and your user is in the audio group."
        )

    print(f"Using input device #{dev_idx}: {dev_info['name']} "
      f"(inputs={dev_info['max_input_channels']})")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (HOST, PORT)

    m = Model(
        wakeword_models=[str(p) for p in model_files],
        inference_framework="onnx",
        vad_threshold=0.0,
    )
    model_names = list(m.models.keys())
    print("Loaded models:", ", ".join(model_names))

    last = 0.0

    def notify(label: str, score: float):
            payload = {
                "type": "wake",
                "label": label,
                "score": float(score),
                "ts": time.time(),
            }
            try:
                sock.sendto(json.dumps(payload).encode("utf-8"), dest)
            except OSError as e:
                if getattr(e, "winerror", None) in (10054, 10061, 10065):
                    return
                if e.errno in (errno.ECONNREFUSED, errno.ENETUNREACH, errno.EHOSTUNREACH):
                    return

    audio_buf = np.zeros((0,), dtype=np.int16)

    def callback(indata, frames, time_info, status):
        nonlocal last, audio_buf
        if status:
            return

        pcm = indata[:, 0].astype(np.float32, copy=False)
        rms = float(np.sqrt(np.mean(pcm * pcm)))
        if rms < 0.0015:
            pcm16 = np.zeros((frames,), dtype=np.int16)
        else:
            pcm16 = np.clip(pcm * 32767.0, -32768, 32767).astype(np.int16, copy=False)

        audio_buf = np.concatenate([audio_buf, pcm16])

        while audio_buf.size >= VAD_FRAME:
            frame = audio_buf[:VAD_FRAME]
            audio_buf = audio_buf[VAD_FRAME:]

            now = time.time()
            preds = m.predict(frame)

            if not isinstance(preds, dict):
                return
            
            best_name, best_score = None, 0.0
            for name in model_names:
                s = float(preds.get(name, 0.0))
                if s > best_score:
                    best_name, best_score = name, s

            if (now - last) >= COOLDOWN_S and best_name and best_score >= THRESH:
                print(f"WAKE {best_name} {best_score:.3f}")
                notify(best_name, best_score)
                last = now

                raise sd.CallbackStop()

                if hasattr(m, "reset"):
                    m.reset()

                audio_buf = np.zeros((0,), dtype=np.int16)
                break



    print(f"openWakeWord: listening… UDP -> {HOST}:{PORT}")
    with sd.InputStream(
        device=dev_idx,
        channels=1,
        samplerate=SAMPLE_RATE,
        dtype="float32",
        blocksize=FRAME_SAMPLES,
        callback=callback,
    ):
        while True:
            try:
                with sd.InputStream(...):
                    while True:
                        time.sleep(1)
            except sd.CallbackStop:
                time.sleep(HANDOFF_S)
                continue
        

if __name__ == "__main__":
    main()
