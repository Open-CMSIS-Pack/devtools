# Reconstructed multi-source Trace Bus fixture

`Blinky+Arm.TB.raw` is a 4096-byte, memory-aligned CoreSight formatter capture reconstructed from the real hardware
capture in `../Blinky+Arm/Blinky+Arm.TB.raw`. It is intended for development and integration testing of formatted
multi-source input. It does not contain instruction trace.

Canonical SHA-256 values:

- source capture: `b0fccabe1a326ffe9fadf12d5c3a205d87628985e5e75a99da23c97d7f33d13b`;
- reconstructed capture: `aab49e56a07783b984fa7c6faeea101a51141423e66ba043dbd8d30702012639`;
- reconstruction tool: `8ce6ca54cedc216c04a03587b8388003a8ab0563e6c79c39ebf436d9bfcd0050`;
- analysis helper: `0ce65b99a2c51b2978cf0b790c653f5172715a1fa86521da8088fbd851ef9347`.

The reconstruction preserves the order of all usable hardware payload bytes and their original formatter
interleaving. It makes the following deliberate changes:

- drops four bytes that preceded the first formatter ID in the source capture;
- maps CM4 to Trace Bus ID 1 and CM7 to Trace Bus ID 2;
- prepends one valid ITM hardware synchronization packet to each source;
- removes the redundant terminal CM7 synchronization packet from the source capture;
- regenerates memory-aligned formatter frames and fills the remaining capacity with ID 0 NULL data.

The resulting formatter stream has these properties:

| Trace Bus ID | Processor | Payload bytes | Initial ITM sync |
| :--- | :--- | ---: | :--- |
| 0 | NULL | 7 | n/a |
| 1 | CM4 | 1488 | offset 0 |
| 2 | CM7 | 2093 | offset 0 |

There are 256 formatter frames and 252 ID changes. No payload byte precedes the first formatter ID.

The recorded independent-deformatter and current single-stream-decoder countercheck produced the following semantic
CSV rows without decoder errors:

| Trace Bus ID | PC samples | Exception rows | DWT data rows | Total |
| :--- | ---: | ---: | ---: | ---: |
| 1 / CM4 | 129 | 84 | 0 | 213 |
| 2 / CM7 | 121 | 165 | 26 | 312 |

`Blinky+Arm.ctrace-run.yml` follows the current per-processor setup and generated-reference structure. The
ctrace-private provisional `ctrace-run.trace-format: formatted` field explicitly selects CoreSight frame decoding.
If this field is absent or null, the selected `.TB.raw` filename also selects formatted decoding; `.TB_<suffix>.raw`
uses the same fallback. Ctrace internally defaults formatted input to 16-byte memory-aligned framing; no public
`trace-framing` field is assumed or emitted. The declared processor clocks are fixture metadata and are not encoded
in the raw trace. These format and filename rules are ctrace compatibility behavior; normative format, framing, and
explicit file-association metadata remain to be specified by CMSIS-Toolbox and emitted by the producer that knows the
effective capture configuration.

`regenerate_tb_trace.py` performs the documented reconstruction without reading the canonical output. It validates the
source hash and structure, removes the terminal CM7 synchronization bytes even though formatter interleaving separates
them, applies the source-ID mapping, prepends the CM4 and CM7 synchronization packets in that order, deterministically
packs memory-aligned frames, and adds exactly seven ID 0 bytes to retain the 4096-byte capture size. The tool validates
the output hash and a deformat/reformat round trip before writing it.

From the repository root, regenerate and compare the canonical capture as follows:

```sh
work_dir="$(mktemp -d)"
python3 tools/ctrace/test/data/TB-Trace/regenerate_tb_trace.py \
  --output "$work_dir/Blinky+Arm.TB.raw"
cmp "$work_dir/Blinky+Arm.TB.raw" \
  tools/ctrace/test/data/TB-Trace/Blinky+Arm.TB.raw
sha256sum "$work_dir/Blinky+Arm.TB.raw"
```

The command reports 4096 bytes, 256 frames, 252 formatter ID changes, payload lengths 7/1488/2093 for IDs 0/1/2,
and reconstructed SHA-256 `aab49e56a07783b984fa7c6faeea101a51141423e66ba043dbd8d30702012639`.

`split_tb_trace.py` is the independent analysis helper. It deformats the fixture into one raw ITM file per valid Trace
Bus ID, allowing the unformatted decoder path to remain an implementation-independent semantic countercheck for the
combined formatted path:

```sh
python3 tools/ctrace/test/data/TB-Trace/split_tb_trace.py \
  "$work_dir/Blinky+Arm.TB.raw" --output-dir "$work_dir/split"
```

The helper reports 4096 input bytes, 256 frames, 252 ID changes, no unassigned bytes, ID 0 with 7 bytes, ID 1 with
1488 bytes, and ID 2 with 2093 bytes. Both valid streams report their first ITM synchronization at offset 0. It
deliberately does not copy `Blinky+Arm.ctrace-run.yml`: that configuration describes the combined formatted input and
would be incorrect beside a demultiplexed unformatted stream.

For a semantic countercheck, set `CTRACE` to a freshly built ctrace executable and give each demultiplexed stream a
minimal unformatted trace-run configuration:

```sh
CTRACE=build/tools/ctrace/<platform>/Debug/ctrace
for stream_id in 01 02; do
  stream_dir="$work_dir/split/stream-$stream_id"
  printf 'ctrace-run:\n  ctrace-refs: []\n' \
    > "$stream_dir/Blinky+Arm.ctrace-run.yml"
  "$CTRACE" "$stream_dir" --csv
  awk -v stream="$stream_id" \
    'END { print "stream " stream ": " NR - 1 " semantic rows" }' \
    "$stream_dir/Blinky+Arm.SWO.csv"
done
```

The expected row counts, excluding the CSV header, are `stream 01: 213 semantic rows` and
`stream 02: 312 semantic rows`. These counterchecks inspect the generated output; the checked-in reconstructed capture
remains the canonical test artifact.

The executable integration test also decodes the canonical combined capture directly. It requires 213 payload CSV
rows on Trace Bus ID 1 and 312 on ID 2. ID-0 padding is recorded as skipped-byte CSV Info without creating a decoded
route or CTF file. The output has one CTF stream per active ID and two independent clock declarations for the 240 MHz
CM4 and 480 MHz CM7 routes. Because those clock domains have no specified common origin, the valid CTF bundle
deliberately has no companion Trace Compass XML and reports that limitation once.
