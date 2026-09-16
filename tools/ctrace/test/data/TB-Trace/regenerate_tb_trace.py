#!/usr/bin/env python3
# Copyright (c) 2026 Arm Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

"""Regenerate the canonical multi-source formatter capture from its source."""

from __future__ import annotations

import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path


FRAME_SIZE = 16
FRAME_PAYLOAD_SLOTS = 15
CAPTURE_SIZE = 4096
ITM_HARDWARE_SYNC = b"\x00\x00\x00\x00\x00\x80"
SOURCE_SHA256 = "b0fccabe1a326ffe9fadf12d5c3a205d87628985e5e75a99da23c97d7f33d13b"
OUTPUT_SHA256 = "aab49e56a07783b984fa7c6faeea101a51141423e66ba043dbd8d30702012639"

SOURCE_CM7_ID = 0x01
SOURCE_CM4_ID = 0x02
OUTPUT_CM4_ID = 0x01
OUTPUT_CM7_ID = 0x02
NULL_ID = 0x00


@dataclass(frozen=True)
class RoutedByte:
    """One deformatted payload byte and its formatter source ID."""

    source_id: int | None
    value: int


@dataclass(frozen=True)
class DecodedCapture:
    """Ordered formatter payload and structural counters."""

    payload: list[RoutedByte]
    id_changes: int


def sha256(data: bytes) -> str:
    """Return the lowercase SHA-256 digest of data."""
    return hashlib.sha256(data).hexdigest()


def decode_frames(data: bytes) -> DecodedCapture:
    """Decode memory-aligned CoreSight frames while retaining payload order."""
    if len(data) % FRAME_SIZE:
        raise ValueError(
            f"input size {len(data)} is not a multiple of {FRAME_SIZE} bytes"
        )

    payload: list[RoutedByte] = []
    current_id: int | None = None
    id_changes = 0

    def emit(value: int) -> None:
        payload.append(RoutedByte(current_id, value))

    for frame_offset in range(0, len(data), FRAME_SIZE):
        frame = data[frame_offset : frame_offset + FRAME_SIZE]
        flags = frame[15]

        for index in range(0, 14, 2):
            first = frame[index]
            second = frame[index + 1]
            flag = bool(flags & (1 << (index // 2)))

            if first & 1:
                new_id = first >> 1
                if new_id != current_id:
                    if flag:
                        emit(second)
                    current_id = new_id
                    id_changes += 1
                    if flag:
                        continue
                emit(second)
                continue

            emit(first | int(flag))
            emit(second)

        last = frame[14]
        if last & 1:
            new_id = last >> 1
            if new_id != current_id:
                current_id = new_id
                id_changes += 1
        else:
            emit(last | ((flags >> 7) & 1))

    return DecodedCapture(payload, id_changes)


def payload_for(records: list[RoutedByte], source_id: int | None) -> bytes:
    """Collect payload bytes belonging to one source."""
    return bytes(record.value for record in records if record.source_id == source_id)


def run_count(records: list[RoutedByte]) -> int:
    """Count source-ID runs in an ordered payload."""
    if not records:
        return 0
    return 1 + sum(
        left.source_id != right.source_id
        for left, right in zip(records, records[1:])
    )


def validate_source(data: bytes, decoded: DecodedCapture) -> set[int]:
    """Validate every source property on which reconstruction depends."""
    if len(data) != CAPTURE_SIZE:
        raise ValueError(f"source capture must contain {CAPTURE_SIZE} bytes")
    if sha256(data) != SOURCE_SHA256:
        raise ValueError("source capture SHA-256 does not match the canonical input")
    if decoded.id_changes != 252:
        raise ValueError("source capture must contain 252 formatter ID changes")

    observed_ids = {record.source_id for record in decoded.payload}
    if observed_ids != {None, NULL_ID, SOURCE_CM7_ID, SOURCE_CM4_ID}:
        raise ValueError(f"unexpected source IDs: {sorted(repr(value) for value in observed_ids)}")
    if len(payload_for(decoded.payload, None)) != 4:
        raise ValueError("source capture must contain four bytes before its first formatter ID")
    if len(payload_for(decoded.payload, NULL_ID)) != 9:
        raise ValueError("source capture must contain nine NULL-source bytes")
    if len(payload_for(decoded.payload, SOURCE_CM4_ID)) != 1482:
        raise ValueError("source CM4 payload must contain 1482 bytes")

    cm7_indices = {
        index
        for index, record in enumerate(decoded.payload)
        if record.source_id == SOURCE_CM7_ID
    }
    cm7_payload = payload_for(decoded.payload, SOURCE_CM7_ID)
    if len(cm7_payload) != 2093 or not cm7_payload.endswith(ITM_HARDWARE_SYNC):
        raise ValueError("source CM7 payload must end in its redundant ITM synchronization packet")
    return set(sorted(cm7_indices)[-len(ITM_HARDWARE_SYNC) :])


def transform_payload(decoded: DecodedCapture, removed_cm7_indices: set[int]) -> list[RoutedByte]:
    """Apply the documented source-ID, synchronization, and padding changes."""
    transformed = [
        *(RoutedByte(OUTPUT_CM4_ID, value) for value in ITM_HARDWARE_SYNC),
        *(RoutedByte(OUTPUT_CM7_ID, value) for value in ITM_HARDWARE_SYNC),
    ]
    id_map = {
        SOURCE_CM4_ID: OUTPUT_CM4_ID,
        SOURCE_CM7_ID: OUTPUT_CM7_ID,
    }
    transformed.extend(
        RoutedByte(id_map[record.source_id], record.value)
        for index, record in enumerate(decoded.payload)
        if record.source_id in id_map and index not in removed_cm7_indices
    )

    available_slots = (CAPTURE_SIZE // FRAME_SIZE) * FRAME_PAYLOAD_SLOTS
    # Each source run needs one formatter-ID marker. Appending NULL padding adds
    # one more run and therefore one more marker.
    null_bytes = available_slots - len(transformed) - run_count(transformed) - 1
    if null_bytes <= 0:
        raise ValueError("transformed payload leaves no room for canonical NULL padding")
    transformed.extend(RoutedByte(NULL_ID, 0) for _ in range(null_bytes))
    if len(transformed) + run_count(transformed) != available_slots:
        raise ValueError("transformed payload does not fill complete formatter frames")
    return transformed


def source_marker(source_id: int) -> int:
    """Encode one formatter source-ID marker."""
    if not 0 <= source_id <= 0x7F:
        raise ValueError(f"formatter source ID is out of range: {source_id}")
    return (source_id << 1) | 1


def encode_frames(records: list[RoutedByte]) -> bytes:
    """Encode an ordered payload into deterministic memory-aligned frames."""
    output = bytearray()
    position = 0
    current_id: int | None = None

    while position < len(records):
        frame = bytearray(FRAME_SIZE)
        flags = 0

        for index in range(0, 14, 2):
            if position >= len(records):
                raise ValueError("payload ended before a formatter pair was complete")
            record = records[position]

            if record.source_id != current_id:
                if record.source_id is None:
                    raise ValueError("cannot encode payload without a formatter source ID")
                frame[index] = source_marker(record.source_id)
                current_id = record.source_id
                frame[index + 1] = record.value
                position += 1
                continue

            if position + 1 < len(records) and records[position + 1].source_id != current_id:
                next_id = records[position + 1].source_id
                if next_id is None:
                    raise ValueError("cannot encode payload without a formatter source ID")
                frame[index] = source_marker(next_id)
                frame[index + 1] = record.value
                flags |= 1 << (index // 2)
                current_id = next_id
                position += 1
                continue

            frame[index] = record.value & 0xFE
            flags |= (record.value & 1) << (index // 2)
            position += 1
            if position >= len(records) or records[position].source_id != current_id:
                raise ValueError("payload cannot fill a complete formatter pair")
            frame[index + 1] = records[position].value
            position += 1

        if position >= len(records):
            raise ValueError("payload ended before the final formatter slot")
        record = records[position]
        if record.source_id != current_id:
            if record.source_id is None:
                raise ValueError("cannot encode payload without a formatter source ID")
            frame[14] = source_marker(record.source_id)
            current_id = record.source_id
        else:
            frame[14] = record.value & 0xFE
            flags |= (record.value & 1) << 7
            position += 1

        frame[15] = flags
        output.extend(frame)

    return bytes(output)


def regenerate(source: bytes) -> bytes:
    """Return the canonical reconstructed capture for source."""
    decoded = decode_frames(source)
    removed_cm7_indices = validate_source(source, decoded)
    records = transform_payload(decoded, removed_cm7_indices)
    output = encode_frames(records)

    if len(output) != CAPTURE_SIZE:
        raise ValueError(f"reconstructed capture has unexpected size {len(output)}")
    if sha256(output) != OUTPUT_SHA256:
        raise ValueError("reconstructed capture SHA-256 does not match the canonical output")
    if decode_frames(output).payload != records:
        raise ValueError("reconstructed formatter stream does not round-trip")
    return output


def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments."""
    fixture_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        default=fixture_dir.parent / "Blinky+Arm" / "Blinky+Arm.TB.raw",
        help="source hardware capture (default: adjacent Blinky+Arm fixture)",
    )
    parser.add_argument("--output", type=Path, required=True, help="reconstructed capture to write")
    return parser.parse_args()


def main() -> int:
    """Regenerate, validate, and write the canonical capture."""
    args = parse_arguments()
    output = regenerate(args.source.read_bytes())
    args.output.write_bytes(output)
    decoded = decode_frames(output)

    print(f"wrote {len(output)} bytes ({len(output) // FRAME_SIZE} frames) to {args.output}")
    print(f"SHA-256: {sha256(output)}")
    print(f"formatter ID changes: {decoded.id_changes}")
    for source_id in (NULL_ID, OUTPUT_CM4_ID, OUTPUT_CM7_ID):
        payload = payload_for(decoded.payload, source_id)
        print(f"formatter ID 0x{source_id:02x}: {len(payload)} payload bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
