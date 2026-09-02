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
      --stream <id ...>     Select CoreSight Trace Bus IDs (0 to 111)
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
  Board.TB.raw              # optional Trace Bus input
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

## Build and test

Initialize all dependencies and configure the repository from its root:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target ctrace CtraceUnitTests CtraceIntegTests
```

Run the GoogleTest unit and integration suites plus the executable smoke tests:

```bash
ctest --test-dir build -C Debug -R '^(CtraceUnitTests|CtraceIntegTests|ctrace-)'
```

Editors using `clangd` should open the devtools repository root and configure into `build`. The tool-local
`.clangd` file points clangd at that compilation database.

## Further documentation

- [Architecture](docs/architecture.md): supported features, runtime flow, module boundaries, dependencies, tests,
  and CI.
- [Constraints](docs/constraints.md): contracts that implementation changes must preserve.
- [TODO](docs/todo.md): planned work and pull-request boundaries.
- [OpenCSD issues](docs/opencsd-issues.md): known issues in the pinned decoder revision.
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md): dependency versions, licenses, and build configuration.
- [Test data](test/data/README.md) and [integration tests](test/integration/README.md): fixture provenance and test
  scope.
