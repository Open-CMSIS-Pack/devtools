# ctrace TODO

## Pull-request cleanup

- [ ] Split commit `88c3f4dc`; it mixes PC Sampling and Exception-Return handling.
- [ ] Submit Exception-Return preservation as an independent bug-fix PR.

## DWT

- [ ] Preserve DWT reference, group, and setup-binding identities when expanding source arrays.
- [ ] Complete Armv7-M linked-comparator, range, and value-match decoding.
- [ ] Add Armv8-M and Armv8.1-M DWT decoding.
- [ ] Add `event` output in its own PR.
- [ ] Add `pmu` output in its own PR.
- [ ] Add PC Sampling in its own PR after agreeing the sleep/`PC_SAMPLE` CTF contract.

## Multiple streams

- [ ] Propagate processor identity into decoded events and outputs.
- [ ] Support separate trace clock domains and cross-stream synchronization in CTF.

## Additional decoders

- [ ] Add named trace-buffer discovery (`TB_<name>`) in its own PR.
- [ ] Decode formatted `*.TB.raw` input by Trace Bus ID in its own PR.
- [ ] Add ETM instruction trace decoding and output in its own PR.
- [ ] Add MTB instruction trace decoding and output in its own PR.

## Dependencies and release

- [ ] Replace private OpenCSD `common/` and `interfaces/` headers with supported public APIs.
- [ ] Update OpenCSD after the [empty-buffer issue](opencsd-issues.md) is fixed upstream.
- [ ] Verify the first release archive and hosted Windows, Linux, and macOS build/test matrix.
- [ ] Complete runtime-license, source-provenance, and relinking requirements for statically linked releases.
- [ ] Decide the signing, macOS notarization, SBOM, and archive-checksum requirements for production releases.
