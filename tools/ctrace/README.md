# ctrace

`ctrace` converts CMSIS Cortex-M trace captures into outputs that can be inspected or processed by other tools. It
combines a `<solution-set>.ctrace-run.yml` description with matching raw trace files from one trace directory.
The [architecture description](docs/architecture.md) documents the supported feature profile and internal design.

## Usage

```text
ctrace <trace-dir> [options]

  -t, --target <name>       Process one solution set; otherwise process all
      --csv                 Generate CSV output
      --ctf                 Generate CTF and Trace Compass XML output
  -a, --all                 Generate all output formats
      --type <type ...>     Select event types
      --stream <id ...>     Select streams (0 for unformatted; ATB IDs 1 to 111)
  -h, --help                Print command-line help
  -V, --version             Print the version
```

Values for `--type` and `--stream` are space-separated, so place the trace directory before these multi-value options.
If the directory follows them, terminate option parsing explicitly, for example
`ctrace --type itm dwt -- .trace`. With no output option, `ctrace` validates and decodes the capture without writing
output files. Run `ctrace --help` for the current option details.

## Trace directory

Input and output files share a solution-set base name:

```text
.trace/
  Board.ctrace-run.yml
  Board.SWO.raw
```

For `ctrace .trace --target Board --all`, the supported input produces:

```text
.trace/
  Board.SWO.csv
  Board.ctf/
    metadata
    stream_0
  Board.SWO.traceanalysis.xml  # when retained streams use one clock domain; views are data-driven
```

Discovery requires exactly one `Board.SWO.raw`, `Board.TB.raw`, or
`Board.TB_<name>.raw` input. Without a format declaration, SWO defaults to
unformatted ITM and TB or named-TB defaults to formatted CoreSight input.
Missing or null `trace-format` uses this channel-based default; an explicit
value overrides it for the selected input:

```yaml
ctrace-run:
  trace-format: formatted  # formatted or unformatted
  # ctrace-setup and ctrace-refs follow here
```

This optional, ctrace-private provisional field does not select a file or
resolve multiple candidates. The channel-based default is a heuristic, not
byte-content detection. Formatted input currently requires complete 16-byte
memory-aligned CoreSight frames; there is no public `trace-framing` field yet. See the
[constraints](docs/constraints.md) for the full discovery, routing, and
compatibility contract.

## Incomplete captures

Formatted input accounts for payload skipped because its source ID is missing, NULL, reserved, or unconfigured,
and for initial protocol bytes skipped while seeking hardware synchronization. Each accounting record is CLI Info and a
CSV `info` row, retained regardless of type or stream filters. `info` is an input annotation, not a new `--type`
selector. The row has no time; `stream` contains the observed formatter ID, including `0` or `127`, or is empty when
no ID is known. These observations do not create decoded routes.

The note counts **deformatted payload bytes**, not formatter control bytes or differences between raw offsets. Its
offset identifies the first formatter output group. No raw bytes are rewritten and no synchronization is invented.
Before synchronization, the skipped bytes cannot be classified as ITM software packets or DWT hardware packets
(including exception trace), so the note uses the neutral wording `bytes skipped due to missing SYNC`.
This accounting covers formatter skips and initial ITM synchronization, not every possible decoder-recovery loss.

If a configured formatted route receives bytes but never reaches a real ITM hardware synchronization, ctrace reports
an Error at end of input and exits non-zero. Completed diagnostic and decoded outputs are retained, including data
from healthy routes. Continuing past an unassigned prefix therefore does not guarantee decodable payload. Genuine
framing errors still abort the input and remove incomplete outputs.

## Build and test

Initialize all dependencies and configure the repository from its root:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target ctrace CtraceUnitTests CtraceIntegTests
```

Run the GoogleTest unit and integration suites plus the executable smoke tests:

```bash
ctest --test-dir build -C Debug -R '^(CtraceUnitTests|CtraceIntegTests|CtraceFixtureIntegrity|ctrace-)'
```

On native Linux, installing exactly Babeltrace `2.0.5` before configuration
also registers the external consumer gate:

```bash
ctest --test-dir build -C Debug --no-tests=error -L '^linux-consumer$'
```

CI installs the pinned consumer and treats a missing labelled test as a
failure. The separate versioned Trace Compass acceptance record is documented
with the [integration tests](test/integration/README.md).

Editors using `clangd` should open the devtools repository root and configure into `build`. The tool-local
`.clangd` file points clangd at that compilation database.

## Further documentation

- [Architecture](docs/architecture.md): supported features, runtime flow, module boundaries, dependencies, tests,
  and CI.
- [CTF profile](docs/ctf-format.md): generated CTF structure, event groups, field semantics, and Trace Compass
  representation.
- [Constraints](docs/constraints.md): contracts that implementation changes must preserve.
- [TODO](docs/todo.md): planned work and pull-request boundaries.
- [OpenCSD issues](docs/opencsd-issues.md): known issues in the pinned decoder revision.
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md): dependency versions, licenses, and build configuration.
- [Test data](test/data/README.md) and [integration tests](test/integration/README.md): fixture provenance and test
  scope.
