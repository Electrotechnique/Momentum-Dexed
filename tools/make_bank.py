"""Makes a Momentum-Dexed bank folder from voices in DX7 .syx banks, as the firmware saves them.

Each voice becomes a patch file named by its UID (the RokkitHash of its JSON, as savePatch()
makes it), with Momentum's default settings, and the bank's Patchindex lists them in order.

    python tools/make_bank.py SDCard/Bank1 SDCard/SYX DX01.syx:22 RICH01.syx:10 ...

FILE:N is voice N (1-32) of a 32 voice bank in the .syx folder. Python 3 only, no packages.
"""
import json
import os
import struct
import sys


def rokkit(data: bytes) -> int:
    """RokkitHash (Paul Hsieh's SuperFastHash), as the firmware's getHash()."""
    M = 0xFFFFFFFF
    n = len(data)
    if n == 0:
        return 0
    h = n & M
    rem = n & 3
    i = 0
    for _ in range(n >> 2):
        h = (h + struct.unpack_from("<H", data, i)[0]) & M
        tmp = ((struct.unpack_from("<H", data, i + 2)[0] << 11) ^ h) & M
        h = ((h << 16) ^ tmp) & M
        i += 4
        h = (h + (h >> 11)) & M
    signed = lambda b: b - 256 if b > 127 else b
    if rem == 3:
        h = (h + struct.unpack_from("<H", data, i)[0]) & M
        h ^= (h << 16) & M
        h ^= (signed(data[i + 2]) << 18) & M
        h = (h + (h >> 11)) & M
    elif rem == 2:
        h = (h + struct.unpack_from("<H", data, i)[0]) & M
        h ^= (h << 11) & M
        h = (h + (h >> 17)) & M
    elif rem == 1:
        h = (h + signed(data[i])) & M
        h ^= (h << 10) & M
        h = (h + (h >> 1)) & M
    h ^= (h << 3) & M
    h = (h + (h >> 5)) & M
    h ^= (h << 4) & M
    h = (h + (h >> 17)) & M
    h ^= (h << 25) & M
    h = (h + (h >> 6)) & M
    return h


def unpack_voice(p: bytes) -> bytearray:
    """A 128 byte bank voice to the 155 byte voice, as fmUnpackVoice() (3 bit pitch mod sens)."""
    v = bytearray(155)
    for op in range(6):
        q, o = p[op * 17:op * 17 + 17], op * 21
        v[o:o + 11] = q[0:11]
        v[o + 11] = q[11] & 3
        v[o + 12] = (q[11] >> 2) & 3
        v[o + 13] = q[12] & 7
        v[o + 20] = (q[12] >> 3) & 15
        v[o + 14] = q[13] & 3
        v[o + 15] = (q[13] >> 2) & 7
        v[o + 16] = q[14] & 127
        v[o + 17] = q[15] & 1
        v[o + 18] = (q[15] >> 1) & 31
        v[o + 19] = q[16] & 127
    v[126:134] = p[102:110]
    v[134] = p[110] & 31
    v[135] = p[111] & 7
    v[136] = (p[111] >> 3) & 1
    v[137:141] = p[112:116]
    v[141] = p[116] & 1
    v[142] = (p[116] >> 1) & 7
    v[143] = (p[116] >> 4) & 7
    v[144] = p[117] & 127
    v[145:155] = p[118:128]
    return v


def voice_name(v: bytes) -> str:
    name = "".join(chr(c) if 32 <= c < 127 else " " for c in v[145:155]).rstrip()
    return name or "FM Voice"


def patch_json(name: str, voice: bytes) -> dict:
    """The keys and defaults of savePatch() and PatchStruct, in the same order."""
    v = bytearray(voice)
    v[145:155] = name[:10].ljust(10).encode("ascii")  # savePatch(): the patch name as voice name
    return {
        "PatchName": name, "Engine": "FM", "Voice": v.hex().upper(), "OpEnabled": 63,
        "EngineType": 1, "EnvMacro": [50, 50, 50, 50], "PitchBend": 2, "ModWheelRange": 50,
        "ModWheelTarget": 1, "AftertouchRange": 0, "AftertouchTarget": 0, "MonophonicMode": 0,
        "PortamentoTime": 0, "ToneCutoff": 127, "ToneResonance": 0, "EnsembleEffectAmt": 8,
        "EnsembleEffectMix": 0, "ReverbEffectTime": 8, "ReverbEffectMix": 0,
        "EnsembleEffectDepth": 64, "EnsembleEffectDelay": 64, "ReverbEffectDamping": 64,
        "ReverbEffectBassCut": 26,
    }


def main(bank_dir, syx_dir, entries):
    if len(entries) > 128:
        sys.exit("A bank holds up to 128 patches")
    os.makedirs(bank_dir, exist_ok=True)
    index = {}
    for n, entry in enumerate(entries, 1):
        file, num = entry.rsplit(":", 1)
        b = open(os.path.join(syx_dir, file), "rb").read()
        if len(b) != 4104 or b[:2] != b"\xF0\x43" or b[3] != 9:
            sys.exit(f"{file}: not a 32 voice DX7 bank")
        k = int(num) - 1
        voice = unpack_voice(b[6 + k * 128:6 + k * 128 + 128])
        name = voice_name(voice)
        doc = patch_json(name, voice)
        compact = json.dumps(doc, separators=(",", ":"), ensure_ascii=False)
        uid = rokkit(compact.encode("utf-8"))
        doc["UID"] = uid
        with open(os.path.join(bank_dir, str(uid)), "w", newline="") as f:
            f.write(json.dumps(doc, separators=(",", ":"), ensure_ascii=False))
        index[str(n)] = uid
        print(f"{n:3} {name:<10} {file}:{num} -> {uid}")
    with open(os.path.join(bank_dir, "Patchindex"), "w", newline="") as f:
        f.write(json.dumps(index, separators=(",", ":")))


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
