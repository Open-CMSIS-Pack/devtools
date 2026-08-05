# ctrace

`ctrace` converts CMSIS Cortex-M trace captures into outputs that can be inspected or processed by other tools. It
combines a `<solution-set>.ctrace-run.yml` description with matching raw trace files from one trace directory.

The current implementation decodes SWO/ITM and DWT data. Trace Bus (`*.TB.raw`) files are discovered and reported,
but deliberately skipped until that decoder is implemented.
ITM payload output supports stimulus ports `1` through `31`. Port `0` remains part of stream decoding but is
deliberately excluded from CSV and CTF event output. Trace Compass observes the same filtered CTF stream.

![ctrace architecture](docs/architecture.svg)

## Usage

```text
ctrace <trace-dir> [options]

  -t, --target <name>       Process one solution set; otherwise process all
      --csv                 Generate CSV output
      --ctf                 Generate CTF and Trace Compass XML output
  -a, --all                 Generate all output formats
      --type <type ...>     Select event types
      --stream <id ...>     Select CoreSight Trace Bus IDs (0 to 111)
  -h, --help                Print command-line help
  -V, --version             Print the version
```

Values for `--type` and `--stream` are space-separated, so place the trace directory before these multi-value options.
If the directory follows them, terminate option parsing explicitly, for example
`ctrace --type itm dwt -- .trace`. With no output option, `ctrace` validates and decodes the capture without writing
output files.

The `--type` option accepts the specification-defined selectors `itm`, `dwt`, `event`, `pmu`, `exception`,
`pcsample`, `global_ts`, `overflow`, and `error`. Decoded DWT event counters, PMU packets, and PC samples remain
disabled until their output semantics are implemented, so their selectors currently produce no rows.

A trace directory uses solution-set-based file names:

```text
.trace/
  Board.ctrace-run.yml
  Board.SWO.raw
  Board.TB.raw              # discovered, currently not decoded
```

For `ctrace .trace --target Board --all`, the supported input produces:

```text
.trace/
  Board.SWO.csv
  Board.ctf/
    metadata
    stream_0
  Board.SWO.traceanalysis.xml
```

The YAML reader validates fields consumed by `ctrace` and ignores unrelated or unknown fields. A malformed consumed
field is an error; an unneeded extension to the trace-run format does not break the tool.

## Project structure

| Path | Responsibility |
|---|---|
| `src/CtraceMain.h` | Platform-independent entry point used by the executable trampoline |
| `src/cli` | Command-line parsing and validation |
| `src/control` | Trace-directory orchestration, raw-file access, and per-file decode jobs |
| `src/tracerun` | Trace-run discovery, YAML parsing, and normalized metadata |
| `src/decode` | OpenCSD adapter, recovery, and Cortex-M semantic decoding |
| `src/model` | Backend-independent trace events and selections |
| `src/output/csv` | CSV schema and writer |
| `src/output/ctf` | CTF bundle and Trace Compass XML writers |
| `src/diagnostics` | Structured diagnostics and trace-issue reporting |
| `test/unit` | GoogleTest cases, arranged like the production modules |
| `test/integration` | File-oriented executable tests run through CTest |
| `test/data` | Stable fixtures and expected output |

The module boundaries, dependency direction, runtime flow, and extension points are described in the
[architecture documentation](docs/architecture.md). See also the [verified constraints](docs/constraints.md) and the
compact [remaining TODO list](docs/todo.md).

## Build and test

Initialize all dependencies and configure the repository from its root:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target ctrace CtraceUnitTests
```

Run the independently discoverable GoogleTest cases and the executable-level integration tests:

```bash
ctest --test-dir build -C Debug -R '^(CtraceUnitTests|ctrace-)'
```

Editors using `clangd` should open the devtools repository root and configure into `build`. The tool-local
`.clangd` file points clangd at that compilation database.

## Dependencies and releases

`ctrace` uses the devtools copies of `cxxopts`, `yaml-cpp`, GoogleTest, and the `external/OpenCSD` submodule. It does
not carry private copies below `tools/ctrace`.
Known defects in the pinned OpenCSD revision that can affect ctrace are recorded
in the [OpenCSD issue notes](docs/opencsd-issues.md).

Publishing a GitHub Release for a tag named `tools/ctrace/<version>`, for example `tools/ctrace/0.0.1`, runs the ctrace
workflow. It builds Windows AMD64/Arm64, Linux AMD64/Arm64, and macOS Arm64 variants and attaches `ctrace.zip` to the
release. Unit and integration tests run on Windows AMD64 and Linux AMD64; ARM64 targets are compiled but not executed,
matching the other devtools workflows. Coverage runs on Linux AMD64. The executable version is derived from the same
tool-specific tag.

The project is licensed under Apache-2.0. Product dependencies and their licenses are recorded in the repository's
top-level `LICENSE.md`. The release archive contains the project license, third-party notices, OpenCSD copyright
notices, the application-dependency license texts, and checksums for all included files. Compiler and operating-system
runtime contents must still be inspected for each production build; the application dependency list does not claim to
cover them. Release artifacts are currently unsigned; signing, archive checksums, macOS notarization, and a formal
SBOM remain explicit release-hardening decisions.

The code, build, test, and packaging structure is ready for release-candidate validation. The Blinky SWO and TB
captures are approved redistributable test assets, as documented in the [fixture README](test/data/README.md).
Publishing the current statically linked Linux artifacts additionally requires the runtime-license and relinking work
tracked in the [release TODO](docs/todo.md).
