# ctrace TODO

## DWT

- [ ] Preserve logical DWT reference and setup identities when expanding comparator source arrays.
- [ ] Complete Armv7-M linked-comparator, range, and value-match decoding.
- [ ] Resolve programmable PMU event-counter names from trace-run configuration.
- [ ] Decode the Armv8-M one-byte PC-sampling marker `0xFF` as `Trace prohibited`, as defined by the
      [CSV specification](https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/#pc-sampling-markers).
      The current decoder accepts a four-byte PC or the one-byte `0x00` sleep marker and reports other forms as errors.

## Inputs and time correlation

- [ ] Align caller-facing raw-input/channel selection and transport format/framing with the outcome of
      [cmsis-toolbox #699](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/pull/699), also tracked by
      [vscode-cmsis-debugger #1150](https://github.com/Open-CMSIS-Pack/vscode-cmsis-debugger/issues/1150) and
      [devtools #2573](https://github.com/Open-CMSIS-Pack/devtools/issues/2573). Keep trace communication separate from
      trace-source setup and decide how to replace the temporary `trace-format` override without assuming its
      standardization.
- [ ] Support FSYNC and FSYNC+HSYNC formatted input after a public framing contract is specified.
- [ ] Define cross-stream clock correlation and offsets once a common producer time reference is available.
- [ ] Add per-clock-domain Trace Compass bundles/experiments for uncorrelated streams when required.

## Additional decoders

- [ ] Support multiple simultaneous named trace-buffer inputs after explicit file association is specified.
- [ ] Add ETM/ETE/PTM instruction trace decoding and output in its own PR.
- [ ] Add MTB instruction trace decoding and output in its own PR.
- [ ] Add Event Recorder decoding and output when it enters the implementation scope.

## Dependencies and release

- [ ] Replace private OpenCSD `common/` and `interfaces/` headers with supported public APIs.
- [ ] Update OpenCSD after the [empty-buffer issue](opencsd-issues.md) is fixed upstream.
- [ ] Decide the signing, macOS notarization, and SBOM requirements for production releases. The release archive
      already includes `SHA256SUMS` for its binaries and license material.
