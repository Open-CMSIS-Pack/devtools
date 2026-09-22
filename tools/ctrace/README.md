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
  Board.SWO.ctf/
    metadata
    stream_0
  Board.SWO.traceanalysis.xml  # only with graphical data and one retained clock domain
```

Discovery processes every existing `Board.SWO.raw`, `Board.TB.raw`, and `Board.TB_<name>.raw` input independently,
one after another. `--target Board` selects the solution set and all its supported inputs. Each input gets its own
CSV, CTF bundle, and optional XML companion, using `<solution-set>.<channel>` as their common base name. CTF uses
this channel-qualified name even for a single input; older `Board.ctf` bundles are neither reused nor removed.
The [CTF profile](docs/ctf-format.md#files-and-common-structure) records the required specification alignment.

Without a format declaration, each SWO input defaults to unformatted ITM and each TB or named-TB input defaults to
formatted CoreSight. Missing or null `trace-format` uses these channel-based defaults; an explicit value overrides
the format for every supported input in that trace-run:

```yaml
ctrace-run:
  trace-format: formatted  # formatted or unformatted
  # ctrace-setup and ctrace-refs follow here
```

This optional, ctrace-private provisional field does not select a file. The channel-based default is a heuristic, not
byte-content detection. Formatted input currently requires complete 16-byte
memory-aligned CoreSight frames; there is no public `trace-framing` field yet. See the
[constraints](docs/constraints.md) for the full discovery, routing, and
compatibility contract.

Event Recorder (`Board.ER.raw`) remains unsupported and is skipped with a warning. A solution set without a supported
input reports an error. A failure in one input leaves the remaining inputs and solution sets available for processing;
the command returns non-zero if any input fails. This also applies when validating without output options.

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
from healthy routes. Continuing past an unassigned prefix therefore does not guarantee decodable payload.

A fatal OpenCSD error, including a framing error, aborts decoding and returns a non-zero status. A CSV output that
has already started retains the previously committed rows and ends with a global `type=error` row. Its `note` is
`decode aborted after processing N input bytes; trace is incomplete: reason`; all other fields are empty. This final
record describes the entire input, so it bypasses both `--type` and `--stream`. Partial CTF and Trace Compass XML
artifacts are still removed. A failure before CSV starts creates no CSV, and a CSV write or close failure still
removes the unreliable file. Preserving partial CSV with this global marker is an explicit ctrace contract; the
published CSV specification does not define fatal-abort handling.

## Packet diagnostics

CLI diagnostics are always unfiltered. Ordinary route-bound error and warning rows in CSV follow `--stream` and
`--type`: their output type is `error`, so `--type dwt error` retains both DWT data and selected-stream diagnostics,
whereas `--type dwt` omits those diagnostic rows. The global fatal-abort record described above is the exception.
The [published CSV specification](https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/#csv-format)
defines `error` and its free-text `note`, but no `warning` type or severity column; ctrace adds neither.

CLI errors and CSV `note` fields retain the native OpenCSD error code and message.
CLI trace issues also carry a structured `raw_offset` and, for formatted input,
the source `stream` when available.
When the raw-packet callback identifies the failing packet, the diagnostic also
includes its original ITM packet type, total byte count, and up to 16 hexadecimal
bytes. Longer packets have an explicitly truncated preview. Incomplete packets
at end of input receive the same context even when OpenCSD reports them only
through the packet monitor, without a logger error.

For formatted input, the reported raw index can identify a deformatter output
group rather than the exact physical position of the failing byte. A recovery
message distinguishes a later hardware SYNC (with its raw index) from reaching
end of input without resynchronization. The affected raw interval includes
formatter control and potentially other routes: its length is not a count of
zero bytes or discarded ITM payload bytes. Errors still make the invocation
fail even when decoding resumes and completed outputs are retained.

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
- [Multi-source design](docs/multi-source-design.md): rationale and migration from single-source SWO to routed
  CoreSight input, with later contract changes identified separately.
- [TODO](docs/todo.md): planned work and pull-request boundaries.
- [OpenCSD issues](docs/opencsd-issues.md): known issues in the pinned decoder revision.
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md): dependency versions, licenses, and build configuration.
- [Test data](test/data/README.md) and [integration tests](test/integration/README.md): fixture provenance and test
  scope.
