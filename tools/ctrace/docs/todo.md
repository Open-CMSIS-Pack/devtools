# ctrace TODO

## DWT

- [ ] Preserve logical DWT reference and setup identities when expanding comparator source arrays.
- [ ] Complete Armv7-M linked-comparator, range, and value-match decoding.
- [ ] Resolve programmable PMU event-counter names from trace-run configuration.

## Inputs and time correlation

- [ ] Align caller-facing raw-input/channel selection and transport format/framing with the outcome of
      [cmsis-toolbox #699](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/pull/699), also tracked by
      [vscode-cmsis-debugger #1150](https://github.com/Open-CMSIS-Pack/vscode-cmsis-debugger/issues/1150).
      TB input/CSV naming support tracked by
      [devtools #2573](https://github.com/Open-CMSIS-Pack/devtools/issues/2573) is complete and that issue is closed;
      public channel selection and format/framing remain separate follow-up work. Keep trace communication separate
      from trace-source setup and decide how to replace the temporary `trace-format` override without assuming its
      standardization.
- [ ] Align the published CTF bundle path with ctrace's `<solution-set>.<channel>.ctf` naming, including single-input runs.
- [ ] Support FSYNC and FSYNC+HSYNC formatted input after a public framing contract is specified.
- [ ] Define cross-stream clock correlation and offsets once a common producer time reference is available.
- [ ] Add per-clock-domain Trace Compass bundles/experiments for uncorrelated streams when required.

## Additional decoders

- [ ] Add ETM/ETE/PTM instruction trace decoding and output in its own PR.
- [ ] Add MTB instruction trace decoding and output in its own PR.
- [ ] Add Event Recorder decoding and output when it enters the implementation scope.

## AI trace analysis skill

The [skill evaluation kit](../test/skill/README.md) covers recorded trace questions, capture selection,
bounded inspection, and diagnostics requested by the user. Fixture and artifact checks complement manual
semantic review; they do not prove automatic skill discovery or full model behavior.

- [ ] Verify implicit skill selection in the actual CMSIS Extension host, including registration and distinction
      from live debugging, project configuration, and documentation skills. Metadata-only discovery tests are
      preliminary evidence.
- [ ] Integrate deterministic evaluation-kit checks into CI without expanding the ctrace workflow path filters.
      Keep optional model evaluations separate and record the model, host, transcript, and semantic rubric results.
- [ ] Add host-observed evaluation of file reads and shell activity to check inspection budgets, direct RAW reads,
      and bypasses of the recorded CLI wrapper. The current artifact checks cannot observe these actions.
- [ ] Define native selectors and aggregations for questions about complete, large captures, including the need
      for time, index, variable, or address selection. Retain the current inspection limits until the CLI supports
      the required operation; do not infer a complete-capture answer from a bounded sample.

## Dependencies and release

- [ ] Replace private OpenCSD `common/` and `interfaces/` headers with supported public APIs.
- [ ] Update OpenCSD after the [empty-buffer issue](opencsd-issues.md) is fixed upstream.
- [ ] Decide the signing, macOS notarization, and SBOM requirements for production releases. The release archive
      already includes `SHA256SUMS` for its binaries and license material.
