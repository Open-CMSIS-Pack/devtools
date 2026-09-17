# ctrace TODO

## Cleanup

- [ ] Clarify the compound DWT/PMU payload-validation expressions in `DwtPacketDecoder` with explicit grouping.

## DWT

- [ ] Preserve logical DWT reference and setup identities when expanding comparator source arrays.
- [ ] Complete Armv7-M linked-comparator, range, and value-match decoding.
- [ ] Resolve programmable PMU event-counter names from trace-run configuration.

## Inputs and time correlation

- [ ] Resolve the cross-tool raw-input contract tracked by
      [vscode-cmsis-debugger #1150](https://github.com/Open-CMSIS-Pack/vscode-cmsis-debugger/issues/1150) and
      [devtools #2573](https://github.com/Open-CMSIS-Pack/devtools/issues/2573): standardize selected raw-input identity
      plus effective byte format/framing in the CMSIS-Toolbox trace specification, then migrate the provisional global
      `trace-format` reader contract, channel-based format defaults, and producer output deliberately.
- [ ] Correct the CMSIS-Toolbox CSV schema from the obsolete seventh-column name `offset` to the implemented
      `address` name.
- [ ] Support FSYNC and FSYNC+HSYNC formatted input after a public framing field is specified.
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
- [ ] Decide the signing, macOS notarization, SBOM, and archive-checksum requirements for production releases.
