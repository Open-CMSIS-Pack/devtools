# ctrace Constraints

This document records contracts that implementation changes must preserve. It intentionally does not describe the
runtime flow, module inventory, supported feature profile, build, or release process; those belong in the
[architecture description](architecture.md), [README](../README.md), and [TODO list](todo.md). The CMSIS-Toolbox
[trace specification](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/Experimental-Features.md#file-structure-of-ctrace-runyml)
remains authoritative for the external `*.ctrace-run.yml` format.

## Boundaries

- OpenCSD types remain inside the decode layer. Other modules and output backends consume semantic `TraceEvent`
  values.
- An OpenCSD API migration must retain access to typed ITM configuration and packet data and must preserve every
  structured decoder error from each data-path operation; falling back to only the last error or formatted log text
  would change recovery behavior.
- YAML types remain inside the trace-run reader. The rest of ctrace consumes normalized configuration and metadata.
- The YAML reader validates fields consumed by ctrace; unrelated fields are outside its validation scope. Malformed
  consumed fields remain errors. An ITM reference without `source` values is valid and contributes no source events.
- Backend-specific requirements and failures remain independent; requesting CTF must not disable otherwise valid CSV
  output, or vice versa.

## Decode invariants

- Raw trace bytes are passed to the decoder unchanged; ctrace never injects synthetic synchronization. Recovery
  resumes only at synchronization present in the input.
- File-read chunks are not packet boundaries. Decoder state must survive arbitrary read boundaries.
- Incomplete input and unrecoverable decoder responses remain visible errors. Discontinuities flush or clear pending
  DWT state and invalidate timestamp quality before decoding continues.
- Unformatted input uses Trace Bus ID `0`; routed IDs are restricted to `1` through `111`.
- ITM stimulus ports are restricted to `0` through `31`. Timestamp prescalers are stream-specific, default to `1`,
  and accept only `1`, `4`, `16`, or `64`.

## Observable behavior and output safety

- ITM port `0` is decoded for stream integrity but excluded from payload output. Decoder warnings and errors remain
  observable regardless of payload filtering.
- Structured diagnostic impact determines command failure; formatted stderr text does not.
- CTF timestamps never regress, and a global timestamp does not by itself establish local timestamp quality.
- Validation-only mode creates no output. Unsupported trace channels are diagnosed and skipped.
- Cleanup of incomplete output artifacts is attempted after failure, and cleanup failures are reported. Incompatible
  target types and overlapping CTF/XML paths are rejected before replacement.

Changes to these contracts require corresponding unit or integration coverage. Update the architecture document only
when the implementation structure or data flow changes.
