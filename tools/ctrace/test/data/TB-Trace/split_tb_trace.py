#!/usr/bin/env python3
# Copyright (c) 2026 Arm Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

"""Split memory-aligned CoreSight formatter frames into per-ID ITM streams.

The default output preserves the demultiplexed payload bytes exactly.  The
optional analysis sync is synthetic and exists only to let an ITM decoder start
at byte zero when the captured excerpt does not begin with a hardware sync.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


FRAME_SIZE = 16
ITM_HARDWARE_SYNC = b"\x00\x00\x00\x00\x00\x80"
MIN_SOURCE_ID = 0x01
MAX_SOURCE_ID = 0x6F


@dataclass
class SplitResult:
    """Payload bytes collected for each formatter ID."""

    streams: dict[int, bytearray]
    unassigned: bytearray
    id_changes: int


def split_frames(data: bytes) -> SplitResult:
    """Demultiplex memory-aligned 16-byte CoreSight formatter frames."""
    if len(data) % FRAME_SIZE:
        raise ValueError(
            f"input size {len(data)} is not a multiple of {FRAME_SIZE} bytes"
        )

    streams: dict[int, bytearray] = defaultdict(bytearray)
    unassigned = bytearray()
    current_id: int | None = None
    id_changes = 0

    def emit(value: int) -> None:
        if current_id is None:
            unassigned.append(value)
        else:
            streams[current_id].append(value)

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

    return SplitResult(dict(streams), unassigned, id_changes)


def sync_offsets(data: bytes | bytearray) -> list[int]:
    """Return every hardware ITM synchronization offset in a byte stream."""
    offsets: list[int] = []
    offset = 0
    while (offset := data.find(ITM_HARDWARE_SYNC, offset)) >= 0:
        offsets.append(offset)
        offset += len(ITM_HARDWARE_SYNC)
    return offsets


def write_outputs(
    result: SplitResult,
    output_dir: Path,
    solution_set: str,
    prepend_analysis_sync: bool,
) -> None:
    """Write valid sources as independent raw ITM streams."""
    output_dir.mkdir(parents=True, exist_ok=True)

    if result.unassigned:
        (output_dir / "unassigned.bin").write_bytes(result.unassigned)

    for source_id, payload in sorted(result.streams.items()):
        if not MIN_SOURCE_ID <= source_id <= MAX_SOURCE_ID:
            (output_dir / f"non-source-id-{source_id:02x}.bin").write_bytes(payload)
            continue

        stream_dir = output_dir / f"stream-{source_id:02x}"
        stream_dir.mkdir(parents=True, exist_ok=True)
        output = bytes(payload)
        if prepend_analysis_sync and not output.startswith(ITM_HARDWARE_SYNC):
            output = ITM_HARDWARE_SYNC + output
        (stream_dir / f"{solution_set}.SWO.raw").write_bytes(output)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="memory-aligned *.TB.raw file")
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="directory for stream-<id>/<solution-set>.SWO.raw files",
    )
    parser.add_argument(
        "--prepend-analysis-sync",
        action="store_true",
        help="prepend a synthetic ITM sync where the stream does not start with one",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    data = args.input.read_bytes()
    result = split_frames(data)
    solution_set = args.input.name.removesuffix(".TB.raw")
    write_outputs(
        result,
        args.output_dir,
        solution_set,
        args.prepend_analysis_sync,
    )

    print(f"input: {len(data)} bytes, {len(data) // FRAME_SIZE} frames")
    print(f"formatter ID changes: {result.id_changes}")
    print(f"unassigned before first ID: {len(result.unassigned)} bytes")
    for source_id, payload in sorted(result.streams.items()):
        kind = "source" if MIN_SOURCE_ID <= source_id <= MAX_SOURCE_ID else "non-source"
        offsets = ", ".join(str(value) for value in sync_offsets(payload)) or "none"
        print(
            f"{kind} 0x{source_id:02x}: {len(payload)} bytes, "
            f"ITM sync offsets: {offsets}"
        )
    if args.prepend_analysis_sync:
        print("analysis outputs have a synthetic leading ITM sync where required")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
