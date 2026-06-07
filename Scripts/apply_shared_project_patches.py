#!/usr/bin/env python3
"""Patch FirstPerson BPs so Nova voice PC + F-key interact work with bp_npc."""
from __future__ import annotations

import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

OLD_PC_PARENT = "/Script/CoreUObject.Class'/Script/Engine.PlayerController'"
NEW_PC_PARENT = "/Script/CoreUObject.Class'/Script/NovaUproject.NovaClickMovePlayerController'"


def replace_fstring(data: bytearray, old: str, new: str) -> int:
    """Replace UE FString entries (length prefix includes trailing null)."""
    old_b = old.encode("ascii")
    new_b = new.encode("ascii")
    count = 0
    start = 0
    while True:
        pos = data.find(old_b, start)
        if pos == -1:
            break

        len_pos = pos - 4
        if len_pos < 0:
            start = pos + 1
            continue

        (declared_len,) = struct.unpack_from("<i", data, len_pos)
        expected_len = len(old_b) + 1
        if declared_len != expected_len:
            start = pos + 1
            continue

        if data[pos + len(old_b)] != 0:
            start = pos + 1
            continue

        end = pos + len(old_b) + 1
        new_chunk = new_b + b"\x00"
        data[len_pos : len_pos + 4] = struct.pack("<i", len(new_chunk))
        data[pos:end] = new_chunk

        count += 1
        start = pos + len(new_chunk)
    return count


def patch_file(rel_path: str, mutator, *, restore_from_backup: bool = False) -> None:
    path = ROOT / rel_path
    if not path.exists():
        raise FileNotFoundError(path)
    backup = path.with_suffix(path.suffix + ".bak")
    if not backup.exists():
        shutil.copy2(path, backup)
    if restore_from_backup:
        shutil.copy2(backup, path)

    data = bytearray(path.read_bytes())
    changes = mutator(data)
    path.write_bytes(data)
    print(f"Patched {rel_path}: {changes}")


def patch_fp_pc_parent(data: bytearray) -> str:
    # uasset 바이너리 reparent는 직렬화 오프셋을 깨뜨림 → 사용 금지
    return "skipped (CoreRedirects + BP_NovaPlayerController)"


def patch_fp_character_interact_key(data: bytearray) -> str:
    old = b"InpActEvt_E_K2Node_InputKeyEvent"
    new = b"InpActEvt_F_K2Node_InputKeyEvent"
    if old not in data:
        return "E input event not found (skipped)"
    data[:] = bytes(data).replace(old, new)
    return "Interact key event E -> F"


def patch_interact_prompt_text(data: bytearray) -> str:
    old = "E를 눌러 대화".encode("utf-16-le")
    new = "F를 눌러 대화".encode("utf-16-le")
    if old not in data:
        return "Interact prompt text not found (skipped)"
    count = data.count(old)
    data[:] = bytes(data).replace(old, new)
    return f"Interact prompt E -> F ({count} occurrence(s))"


def main() -> None:
    patch_file(
        "Content/FirstPerson/Blueprints/BP_FirstPersonCharacter.uasset",
        patch_fp_character_interact_key,
    )
    patch_file(
        "Content/Fantastic_Dungeon_Pack/maps/WBP_InteractPrompt.uasset",
        patch_interact_prompt_text,
    )

    archive_imc = ROOT / "nova 게임 압축본" / "nova 게임 압축본" / "Content" / "Input" / "IMC_Default.uasset"
    target_imc = ROOT / "Content" / "Input" / "IMC_Default.uasset"
    if archive_imc.exists():
        target_imc.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(archive_imc, target_imc)
        print(f"Copied archive IMC_Default -> {target_imc}")


if __name__ == "__main__":
    main()
