# ctrace TODO

## Cleanup

- [ ] Add explicit parentheses to compound payload-validation expressions where they improve readability.
- [ ] Replace the CSV payload-type `if`/`else` chain with a backend-local `std::visit` visitor.
- [ ] Replace the CTF payload-type `if`/`else` chain with a backend-local `std::visit` visitor.

## DWT

- [ ] Preserve logical DWT reference and setup identities when expanding comparator source arrays.
- [ ] Complete Armv7-M linked-comparator, range, and value-match decoding.
- [ ] Resolve programmable PMU event-counter names from trace-run configuration.

## Inputs and multiple streams

[Implementation plan](multicore-multisource-plan.md)

- [ ] Define raw-input format and framing metadata for `ctrace-run.yml`.
- [ ] Move the unformatted SWO/ITM path to an OpenCSD `DecodeTree`.
- [ ] Decode formatted CoreSight frames and route them by Trace Bus ID.
- [ ] Preserve normalized source-route-to-processor bindings and use them in outputs.
- [ ] Support separate trace clock domains in CTF.
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
