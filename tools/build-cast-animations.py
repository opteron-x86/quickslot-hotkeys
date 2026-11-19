#!/usr/bin/env python3
"""Builds Quickslot Casting Animations, the Open Animation Replacer pack that spell casting needs.

Quickslot casts in the shout behavior: it sends "ShoutStart", and while its Quickslot_Casting condition matches the
running cast, this pack's submods swap the shout clips for Skyrim's own casting clips. The clips are copied out of
the game's Skyrim - Animations.bsa, so the pack is built on a machine with the game installed:

    python3 tools/build-cast-animations.py "<Skyrim>/Data/Skyrim - Animations.bsa" <output folder> [--zip <archive>]

The output folder holds meshes/actors/character/OpenAnimationReplacer/Quickslot, ready to install as a mod.
The submod numbers are the condition's Animation values (CastAnimation in src/CastModel.h); Hand is 0 any, 1 right,
2 left; Phase is 1 charging or channeling, 2 the release clip plays, 3 the release clip is about to end.
"""
import argparse
import json
import re
import struct
import sys
import zipfile
from pathlib import Path

PACK = Path("meshes/actors/character/OpenAnimationReplacer/Quickslot")
CONDITION = "Quickslot_Casting"
PRIORITY = 2130000000

# Shout clips the pack replaces: charge ("inhale") and release ("exhale"), standing, armed and sneaking.
INHALE = ["mt_shout_inhale.hkx", "1hm_shout_inhale.hkx", "sneak1hm_shout_inhale.hkx"]
EXHALE = ["mt_shout_exhale.hkx", "1hm_shout_exhale.hkx", "sneak1hm_shout_exhale.hkx"]
VIEWS = {"animations": "meshes\\actors\\character\\animations\\",
         "_1stperson/animations": "meshes\\actors\\character\\_1stperson\\animations\\"}

ANY, RIGHT, LEFT = 0, 1, 2
# (folder, description, animation, hand, charge clip, release clip)
SUBMODS = [
    ("1_aimed", "one hand, aimed", 1, ANY, "mrh_chargeloop.hkx", "mrh_release.hkx"),
    ("1_aimed_left", "left hand, aimed", 1, LEFT, "mlh_chargeloop.hkx", "mlh_release.hkx"),
    ("2_self", "one hand, on self", 2, ANY, "mrh_selfchargeloop.hkx", "mrh_selfrelease.hkx"),
    ("2_self_left", "left hand, on self", 2, LEFT, "mlh_selfchargeloop.hkx", "mlh_selfrelease.hkx"),
    ("3_aimed_concentration", "one hand concentration, aimed", 3, ANY, "mrh_aimedconcentration.hkx", "mrh_release.hkx"),
    ("3_aimed_concentration_left", "left hand concentration, aimed", 3, LEFT, "mlh_aimedconcentration.hkx", "mlh_release.hkx"),
    ("4_self_concentration", "one hand concentration, on self", 4, ANY, "mrh_selfconcentration.hkx", "mrh_selfrelease.hkx"),
    ("4_self_concentration_left", "left hand concentration, on self", 4, LEFT, "mlh_selfconcentration.hkx", "mlh_selfrelease.hkx"),
    ("5_dual_aimed", "dual cast, aimed", 5, ANY, "dmagaimconcharge.hkx", "dmagaimrelease.hkx"),
    ("6_dual_self", "dual cast, on self", 6, ANY, "dmagselfconcharge.hkx", "dmagselfrelease.hkx"),
    ("7_dual_concentration", "two hands concentration, aimed", 7, ANY, "dmagaimconloop.hkx", "dmagaimrelease.hkx"),
    ("8_ritual", "two-handed spell", 8, ANY, "ritualspell_charge.hkx", "ritualspell_release.hkx"),
    ("9_dual_self_concentration", "two hands concentration, on self", 9, ANY, "dmagselfconloop.hkx", "dmagselfrelease.hkx"),
]

# First person: moving in the shout state blends the shout clip with the unarmed walk and run, which pulls the hands out
# of view. While a cast holds the shout state those walk and run clips play the cast's clips as well.
FIRST_PERSON_MOVES = [f"mt_{gait}{direction}.hkx" for gait in ("walk", "run") for direction in (
    "forward", "forwardleft", "forwardright", "left", "right", "backward", "backwardleft", "backwardright")]
# Third person: the walk and run become the magic casting locomotion, with the arms held for casting.
LOCOMOTION = [
    ("mt_walkforward.hkx", "magcast_walkforward.hkx"), ("mt_walkforwardright.hkx", "magcast_walkfrwrdright.hkx"),
    ("mt_walkright.hkx", "magcast_walkright.hkx"), ("mt_walkbackwardright.hkx", "magcast_walkbckwrdrht.hkx"),
    ("mt_walkbackward.hkx", "magcast_walkbackward.hkx"), ("mt_walkbackwardleft.hkx", "magcast_walkbckwrdleft.hkx"),
    ("mt_walkleft.hkx", "magcast_walkleft.hkx"), ("mt_walkforwardleft.hkx", "magcast_walkforwrdleft.hkx"),
    ("mt_runforward.hkx", "magcast_runforward.hkx"), ("mt_runforwardright.hkx", "magcast_runfrwrdright.hkx"),
    ("mt_runright.hkx", "magcast_runright.hkx"), ("mt_runbackwardright.hkx", "magcast_runbckwrdright.hkx"),
    ("mt_runbackward.hkx", "magcast_runbackward.hkx"), ("mt_runbackwardleft.hkx", "magcast_runbackwrdleft.hkx"),
    ("mt_runleft.hkx", "magcast_runleft.hkx"), ("mt_runforwardleft.hkx", "magcast_runforwardleft.hkx"),
]


def ready_clip(animation, hand):
    """The first person pose a release ends in."""
    if animation in (5, 7):
        return "dmagaimconcharge.hkx"
    if animation in (6, 9):
        return "dmagselfconcharge.hkx"
    if animation == 8:
        return "ritualspell_ready.hkx"
    prefix = "mlh" if hand == LEFT else "mrh"
    return f"{prefix}_selfreadyloop.hkx" if animation in (2, 4) else f"{prefix}_readyloop.hkx"


def lz4_block(data, out):
    """Decodes one LZ4 block onto the end of out, which also holds earlier blocks for back references."""
    i, end = 0, len(data)
    while i < end:
        token = data[i]
        i += 1
        length = token >> 4
        if length == 15:
            while True:
                extra = data[i]
                i += 1
                length += extra
                if extra != 255:
                    break
        out += data[i:i + length]
        i += length
        if i >= end:
            break
        offset = data[i] | data[i + 1] << 8
        i += 2
        if offset == 0 or offset > len(out):
            raise ValueError("corrupt LZ4 block")
        length = token & 15
        if length == 15:
            while True:
                extra = data[i]
                i += 1
                length += extra
                if extra != 255:
                    break
        length += 4
        start = len(out) - offset
        for k in range(length):
            out.append(out[start + k])


def lz4_frame(data):
    if data[:4] != b"\x04\x22\x4d\x18":
        raise ValueError("not an LZ4 frame")
    flags = data[4]
    i = 7 + (8 if flags & 0x08 else 0) + (4 if flags & 0x01 else 0)
    out = bytearray()
    while True:
        size = struct.unpack_from("<I", data, i)[0]
        i += 4
        if size == 0:
            break
        raw, size = size & 0x80000000, size & 0x7FFFFFFF
        block = data[i:i + size]
        i += size + (4 if flags & 0x10 else 0)
        if raw:
            out += block
        else:
            lz4_block(block, out)
    return bytes(out)


class Archive:
    """Reads files out of a Skyrim Special Edition BSA (version 105)."""

    def __init__(self, path):
        self.data = Path(path).read_bytes()
        magic, version, offset, self.flags, folders, files, _, names_length, _, _ = struct.unpack_from("<4sIIIIIIIHH", self.data, 0)
        if magic != b"BSA\x00" or version != 105:
            raise ValueError(f"{path} is not a Skyrim Special Edition archive")
        if not self.flags & 1 or not self.flags & 2:
            raise ValueError(f"{path} lacks folder or file names")
        counts, position = [], offset
        for _ in range(folders):
            _, count, _, _ = struct.unpack_from("<QIIQ", self.data, position)
            counts.append(count)
            position += 24
        records = []
        for count in counts:
            length = self.data[position]
            folder = self.data[position + 1:position + length].rstrip(b"\x00").decode("cp1252").lower()
            position += 1 + length
            for _ in range(count):
                _, size, at = struct.unpack_from("<QII", self.data, position)
                records.append((folder, size, at))
                position += 16
        names = self.data[position:position + names_length].split(b"\x00")
        self.files = {}
        for (folder, size, at), name in zip(records, names):
            self.files[f"{folder}\\{name.decode('cp1252').lower()}"] = (size, at)

    def read(self, path):
        key = path.lower().replace("/", "\\")
        if key not in self.files:
            raise KeyError(f"{path} is not in the archive")
        size, at = self.files[key]
        compressed = bool(self.flags & 4) != bool(size & 0x40000000)
        size &= 0x3FFFFFFF
        if self.flags & 0x100:
            length = self.data[at]
            at += 1 + length
            size -= 1 + length
        blob = self.data[at:at + size]
        if not compressed:
            return blob
        original = struct.unpack_from("<I", blob, 0)[0]
        result = lz4_frame(blob[4:])
        if len(result) != original:
            raise ValueError(f"{path} decompressed to {len(result)} bytes instead of {original}")
        return result


def plugin_version(root):
    text = (root / "src/Version.h").read_text(encoding="utf-8")
    found = re.search(r"major=(\d+),minor=(\d+),patch=(\d+)", text)
    if not found:
        raise ValueError("version not found in src/Version.h")
    return ".".join(found.groups())


def condition(version, animation, hand, shout_state=False, phase=0):
    entry = {"condition": CONDITION, "requiredPlugin": "Quickslot", "requiredVersion": version, "Animation": {"value": float(animation)}}
    if hand != ANY:
        entry["Hand"] = {"value": float(hand)}
    if shout_state:
        entry["ShoutState"] = {"value": 1.0}
    if phase:
        entry["Phase"] = {"value": float(phase)}
    return entry


def submod(name, description, priority, conditions, interruptible=False):
    config = {"name": name, "description": f"Quickslot casting animation: {description}", "priority": priority,
              "interruptible": interruptible, "conditions": conditions}
    if interruptible:
        config["hasCustomBlendTimeOnInterrupt"] = True
        config["blendTimeOnInterrupt"] = 0.1
    return config


def build(archive, output, version):
    root = output / PACK
    files = {}

    def put(relative, data):
        files[str(PACK / relative).replace("\\", "/")] = data

    put("config.json", json.dumps({"name": "Quickslot Casting Animations", "author": "Quickslot",
                                   "description": "Skyrim's casting animations for spells Quickslot casts without equipping them. "
                                                  "Active only while Quickslot casts."}, indent=4).encode())
    phases = {1: ("_move", "moving, charging"), 2: ("_move_release", "moving, releasing"), 3: ("_move_ready", "moving, after the release")}
    for folder, description, animation, hand, charge, release in SUBMODS:
        # Each submod gets its own priority: hand-specific above any hand, the first person moving phases above both.
        priority = PRIORITY + animation * 100 + (hand != ANY) * 10
        put(f"{folder}/config.json", json.dumps(submod(folder, description, priority, [condition(version, animation, hand)]), indent=4).encode())
        for view, source in VIEWS.items():
            for clip, targets in ((charge, INHALE), (release, EXHALE)):
                data = archive.read(source + clip)
                for target in targets:
                    put(f"{folder}/{view}/{target}", data)
        for phase, (suffix, label) in phases.items():
            clip = {1: charge, 2: release, 3: ready_clip(animation, hand)}[phase]
            name = folder + suffix
            config = submod(name, f"{description} (first person, {label})", priority + phase,
                            [condition(version, animation, hand, True, phase)], True)
            put(f"{name}/config.json", json.dumps(config, indent=4).encode())
            data = archive.read(VIEWS["_1stperson/animations"] + clip)
            for target in FIRST_PERSON_MOVES:
                put(f"{name}/_1stperson/animations/{target}", data)
    put("0_locomotion/config.json", json.dumps(submod("0_locomotion", "walking and running with the arms held for casting (third person)",
                                                      PRIORITY + 1, [condition(version, 0, ANY, True)]), indent=4).encode())
    for target, clip in LOCOMOTION:
        data = archive.read(VIEWS["animations"] + clip)
        for body in ("male", "female"):
            put(f"0_locomotion/animations/{body}/{target}", data)
    for relative, data in files.items():
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("archive", type=Path, help="Skyrim - Animations.bsa")
    parser.add_argument("output", type=Path, help="folder that receives the pack's meshes folder")
    parser.add_argument("--zip", type=Path, help="also write the pack to this archive")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    version = plugin_version(root)
    files = build(Archive(args.archive), args.output, version)
    if args.zip:
        args.zip.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.zip, "w", zipfile.ZIP_DEFLATED) as archive:
            for relative in sorted(files):
                archive.writestr(relative, files[relative])
    print(f"Wrote {len(files)} files for Quickslot {version} to {args.output}" + (f" and {args.zip}" if args.zip else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
