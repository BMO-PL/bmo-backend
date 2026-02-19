#!/usr/bin/env python3
import json
import socket
import time
from pathlib import Path

import numpy as np
import sounddevice as sd

from openwakeword.model import Model
import openwakeword

HOST, PORT = "127.0.0.1", 3939

SAMPLE_RATE = 16000
FRAME_MS = 80
FRAME_SAMPLES = int(SAMPLE_RATE * FRAME_MS / 1000)

THRESH = 0.60 # Higher is more precise, too high might not recognize
COOLDOWN_S = 0.9

INPUT_DEVICE = 0

def find_model_files():
    pkg_dir = Path(openwakeword.__file__).resolve().parent

    candidates = [
        pkg_dir / "resources" / "models",
        pkg_dir / "models",
    ]

    for d in candidates:
        if d.is_dir():
            onnx = sorted(d.glob("*.onnx"))
            tflite = sorted(d.glob("*.tflite"))
            files = onnx + tflite
            if files:
                return files

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
            "If you're in Docker: run with --device /dev/snd --group-add audio.\n"
            "On host: check /dev/snd exists and your user is in the audio group."
        )

    print(f"Using input device #{dev_idx}: {dev_info['name']} "
      f"(inputs={dev_info['max_input_channels']})")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (HOST, PORT)

    m = Model(wakeword_model_paths=[str(p) for p in model_files])

    print("Loaded models:", ", ".join(m.models.keys()))
    print("openWakeWord: listening...")

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
            except OSError:
                # backend not up yet, etc.
                pass

    def callback(indata, frames, time_info, status):
        nonlocal last
        if status:
            return

        now = time.time()
        if now - last < COOLDOWN_S:
            return

        pcm = indata[:, 0]
        pcm16 = np.clip(pcm * 32768.0, -32768, 32767).astype(np.int16, copy=False)

        preds = m.predict(pcm16)

        best_name, best_score = None, 0.0
        for name, score in preds.items():
            s = float(score)
            if s > best_score:
                best_name, best_score = name, s

        if best_name and best_score >= THRESH:
            print(f"WAKE {best_name} {best_score:.3f}")
            notify(best_name, best_score)
            last = now

    print("Using input device:", INPUT_DEVICE, sd.query_devices(INPUT_DEVICE)["name"])
    
    print(f"openWakeWord: listening… UDP -> {HOST}:{PORT}")
    with sd.InputStream(
        device=INPUT_DEVICE,
        channels=1,
        samplerate=SAMPLE_RATE,
        dtype="float32",
        blocksize=FRAME_SAMPLES,
        callback=callback,
    ):
        while True:
            time.sleep(1)


if __name__ == "__main__":
    main()
