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

THRESH = 0.73
COOLDOWN_S = .9

SESSION_MAX_WAIT_S = 300.0

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
            "[OpenWakeWord] [ERROR] No model files found.\n"
        )

    dev_idx, dev_info = pick_input_device()
    if dev_idx is None:
        raise SystemExit(
            "[PortAudio] [ERROR] No audio input devices found.\n"
        )

    # print(f"Using input device #{dev_idx}: {dev_info['name']} (inputs={dev_info['max_input_channels']})")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    sock.settimeout(0.25)
    dest = (HOST, PORT)

    m = Model(
        wakeword_models=[str(p) for p in model_files],
        inference_framework="onnx",
        vad_threshold=0.0,
    )
    model_names = list(m.models.keys())

    last = 0.0

    def send_wake(label: str, score: float):
        payload = {"type": "wake", "label": label, "score": float(score), "ts": time.time()}
        try:
            sock.sendto(json.dumps(payload).encode("utf-8"), dest)
        except OSError as e:
            if getattr(e, "winerror", None) in (10054, 10061, 10065):
                return
            if e.errno in (errno.ECONNREFUSED, errno.ENETUNREACH, errno.EHOSTUNREACH):
                return

    def wait_for_session_done():
        deadline = time.time() + SESSION_MAX_WAIT_S
        while time.time() < deadline:
            try:
                data, _addr = sock.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                continue

            try:
                msg = json.loads(data.decode("utf-8", errors="ignore"))
            except Exception:
                continue

            if isinstance(msg, dict) and msg.get("type") == "session_done":
                print("wake: backend done; resuming wake listen")
                return True

        print("wake: timed out waiting for backend; resuming wake listen anyway")
        return False

    while True:
        audio_buf = np.zeros((0,), dtype=np.int16)
        wake_hit = {"hit": False}

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

                preds = m.predict(frame)
                if not isinstance(preds, dict):
                    return

                best_name, best_score = None, 0.0
                for name in model_names:
                    s = float(preds.get(name, 0.0))
                    if s > best_score:
                        best_name, best_score = name, s

                now = time.time()
                if (now - last) >= COOLDOWN_S and best_name and best_score >= THRESH:
                    print(f"WAKE {best_name} {best_score:.3f}")
                    send_wake(best_name, best_score)
                    last = now
                    wake_hit["hit"] = True
                    raise sd.CallbackStop()

        print(f"openWakeWord: listening… UDP -> {HOST}:{PORT}")
        try:
            with sd.InputStream(
                device=dev_idx,
                channels=1,
                samplerate=SAMPLE_RATE,
                dtype="float32",
                blocksize=FRAME_SAMPLES,
                callback=callback,
            ):
                while not wake_hit["hit"]:
                    time.sleep(0.1)
        except sd.CallbackStop:
            pass

        if hasattr(m, "reset"):
            m.reset()

        wait_for_session_done()
        

if __name__ == "__main__":
    main()
