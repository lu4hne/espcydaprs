"""
Herramienta de verificación de decodificación AFSK 1200 bps Bell 202 / AX.25
Replica con exactitud la lógica DSP del firmware de ESP32 CYD APRS.
"""
import wave
import struct
import math
import os
import sys

def calc_crc(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0x8408
            else:
                crc >>= 1
    return (~crc) & 0xFFFF

def parse_ax25(frame):
    if len(frame) < 14:
        return "Demasiado corto"
    try:
        def decode_call(chunk):
            call = "".join(chr((b >> 1) & 0x7F) for b in chunk[:6]).strip()
            ssid = (chunk[6] >> 1) & 0x0F
            return f"{call}-{ssid}" if ssid else call
        
        dest = decode_call(frame[0:7])
        src = decode_call(frame[7:14])
        idx = 14
        digis = []
        while idx + 7 <= len(frame) and (frame[idx-1] & 0x01) == 0:
            d_call = "".join(chr((b >> 1) & 0x7F) for b in frame[idx:idx+6]).strip()
            d_ssid = (frame[idx+6] >> 1) & 0x0F
            has_rep = (frame[idx+6] & 0x80) != 0
            rep = "*" if has_rep else ""
            digis.append(f"{d_call}-{d_ssid}{rep}" if d_ssid else f"{d_call}{rep}")
            idx += 7
        if idx + 2 > len(frame):
            return f"{src}>{dest}: (sin control/pid)"
        ctrl = frame[idx]
        pid = frame[idx+1]
        info = frame[idx+2:]
        info_str = bytes(info).decode('latin1', errors='replace')
        digi_str = (" via " + ",".join(digis)) if digis else " DIRECTO"
        return f"{src} > {dest}{digi_str}: {info_str} (Ctrl=0x{ctrl:02X}, PID=0x{pid:02X})"
    except Exception as e:
        return f"Error al parsear: {e}"

def verify_wav(wav_path):
    if not os.path.exists(wav_path):
        print(f"Error: No se encuentra {wav_path}")
        return False

    with wave.open(wav_path, "rb") as w:
        nchannels = w.getnchannels()
        framerate = w.getframerate()
        nframes = w.getnframes()
        raw = w.readframes(nframes)

    orig_samples = struct.unpack(f"<{nframes * nchannels}h", raw)
    if nchannels == 2:
        orig_samples = orig_samples[::2]

    # Decimación a 9600 Hz si la tasa es 48000 Hz
    step = int(framerate / 9600)
    samples = orig_samples[::step]

    # Parámetros exactos del firmware ESP32 CYD APRS
    SAMPLES_PER_BIT = 8
    RATE = 9600
    ALPHA = 0.30

    markCos = [int(round(256.0 * math.cos(2.0 * math.pi * 1200 * k / RATE))) for k in range(SAMPLES_PER_BIT)]
    markSin = [int(round(256.0 * math.sin(2.0 * math.pi * 1200 * k / RATE))) for k in range(SAMPLES_PER_BIT)]
    spaceCos = [int(round(256.0 * math.cos(2.0 * math.pi * 2200 * k / RATE))) for k in range(SAMPLES_PER_BIT)]
    spaceSin = [int(round(256.0 * math.sin(2.0 * math.pi * 2200 * k / RATE))) for k in range(SAMPLES_PER_BIT)]

    sampleBuf = [0] * SAMPLES_PER_BIT
    sampleIdx = 0
    lpfCorr = 0.0
    lastToneMark = False
    pllCounter = 0
    nrziLastTone = False

    rxOnesCount = 0
    rxInFrame = False
    rxCurrentByte = 0
    rxBitIndex = 0
    rxFrameBuf = []

    decoded_frames = []
    dcEst = float(samples[0])

    for s_in in samples:
        dcEst = 0.995 * dcEst + 0.005 * float(s_in)
        centered = int(float(s_in) - dcEst)

        sampleBuf[sampleIdx] = centered
        sampleIdx = (sampleIdx + 1) % SAMPLES_PER_BIT

        iMark = 0
        qMark = 0
        iSpace = 0
        qSpace = 0
        for k in range(SAMPLES_PER_BIT):
            idx = (sampleIdx + k) % SAMPLES_PER_BIT
            s = sampleBuf[idx]
            iMark += (s * markCos[k]) >> 8
            qMark += (s * markSin[k]) >> 8
            iSpace += (s * spaceCos[k]) >> 8
            qSpace += (s * spaceSin[k]) >> 8

        energyMark = float(iMark * iMark + qMark * qMark)
        energySpace = float(iSpace * iSpace + qSpace * qSpace)
        diff = energyMark - energySpace

        lpfCorr = (1.0 - ALPHA) * lpfCorr + ALPHA * diff
        currentToneMark = (lpfCorr >= 0.0)

        if currentToneMark != lastToneMark:
            lastToneMark = currentToneMark
            if pllCounter < (SAMPLES_PER_BIT // 2):
                pllCounter += 1
            else:
                pllCounter -= 1

        pllCounter += 1
        if pllCounter >= SAMPLES_PER_BIT:
            pllCounter -= SAMPLES_PER_BIT

            nrziBit = (currentToneMark == nrziLastTone)
            nrziLastTone = currentToneMark

            if nrziBit:
                rxOnesCount += 1
                if rxOnesCount >= 7:
                    rxInFrame = False
                    rxFrameBuf = []
                    rxBitIndex = 0
                elif rxInFrame:
                    rxCurrentByte = (rxCurrentByte >> 1) | 0x80
                    rxBitIndex += 1
                    if rxBitIndex == 8:
                        rxFrameBuf.append(rxCurrentByte)
                        rxCurrentByte = 0
                        rxBitIndex = 0
            else:
                if rxOnesCount == 6: # Flag 0x7E
                    if rxInFrame and len(rxFrameBuf) >= 16:
                        payload = rxFrameBuf[:-2]
                        crc_rx = rxFrameBuf[-2] | (rxFrameBuf[-1] << 8)
                        crc_exp = calc_crc(payload)
                        if crc_exp == crc_rx:
                            decoded_frames.append(payload)
                    rxInFrame = True
                    rxFrameBuf = []
                    rxBitIndex = 0
                    rxCurrentByte = 0
                    rxOnesCount = 0
                elif rxOnesCount == 5:
                    rxOnesCount = 0
                else:
                    rxOnesCount = 0
                    if rxInFrame:
                        rxCurrentByte = (rxCurrentByte >> 1)
                        rxBitIndex += 1
                        if rxBitIndex == 8:
                            rxFrameBuf.append(rxCurrentByte)
                            rxCurrentByte = 0
                            rxBitIndex = 0

    print(f"==================================================")
    print(f" Verificación de Audio: {wav_path}")
    print(f" Muestras: {len(orig_samples)} ({len(orig_samples)/framerate:.2f}s) @ {framerate} Hz")
    print(f" Tramas AX.25 decodificadas con CRC OK: {len(decoded_frames)}")
    print(f"==================================================")
    for i, frame in enumerate(decoded_frames, 1):
        print(f"[{i}] {parse_ax25(frame)}")
    return len(decoded_frames) >= 2

if __name__ == "__main__":
    wav = sys.argv[1] if len(sys.argv) > 1 else "baliza.wav"
    ok = verify_wav(wav)
    sys.exit(0 if ok else 1)
