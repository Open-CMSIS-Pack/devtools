# ctrace TODO

This list contains only work that is currently open in ctrace. Completed work and duplicated implementation details
are intentionally omitted.

## SWO, ITM, and DWT profile

- [ ] **Input schema:** select and document the supported CMSIS-Toolbox trace revision and define the architecture,
  comparator-role, grouped-source, and symbol metadata required for complete decoding.
- [ ] **Metadata model:** preserve logical reference and grouping identities plus the exact setup-to-run binding context
  when expanding DWT source arrays.
- [ ] **Armv7-M DWT:** complete comparator-role semantics, including linked comparators, address ranges, value matches,
  grouped sources, and diagnostics for configurations that cannot be reconstructed safely.
- [ ] **Armv8-M and Armv8.1-M DWT:** add architecture-aware `match`, `PC+offset`, compressed address/PC, linked-pair,
  range, and value-match decoding.
- [ ] **DWT event counters and PMU:** map the already retained packets to the existing `event` and `pmu` selectors and
  implement CSV, CTF, and Trace Compass output.
- [ ] **DWT PC samples:** add a backend-independent PC-sample event, stop suppressing periodic PC-sample packets in
  the decoder, map it to the existing `pcsample` selector, and implement CSV, CTF, and Trace Compass output.
- [ ] **Multicore:** carry processor identity and trace-route bindings through decoding, event selection, and outputs.
- [ ] **Multiple trace clocks:** promote the preserved per-route clock values to explicit clock-domain identities, add
  the required CTF clock classes, and implement cross-stream synchronization without assuming one shared trace clock.

## Decoder roadmap

- [ ] **Formatted trace:** add the OpenCSD frame deformatter and per-Trace-Bus-ID decoders for `*.TB.raw` input.
- [ ] **ETM instruction trace:** add target-image access, instruction-flow decoding, backend-independent instruction
  events, and corresponding outputs.
- [ ] **MTB instruction trace:** add MTB input and instruction-flow decoding, backend-independent instruction events,
  and corresponding outputs.

## Dependencies and release

- [ ] **OpenCSD boundary:** remove the remaining dependency on OpenCSD `common/` and `interfaces/` private headers and
  use supported public APIs instead.
- [ ] **OpenCSD empty-buffer safety:** track the upstream resolution of the reachable empty-buffer access described in
  the [OpenCSD issue notes](opencsd-issues.md) and update the pinned revision when a fix is available. The submodule is
  deliberately used without a downstream source patch.
- [ ] **Release verification:** inspect the first generated multi-platform ZIP, checksums, license texts, OpenCSD
  notices, and runtime dependencies after a fresh hosted Windows, Linux, and macOS matrix and local AddressSanitizer/
  UndefinedBehaviorSanitizer run over the final changes.
- [ ] **Static runtime compliance:** inspect the GNU and Microsoft runtime code incorporated by the pinned release
  toolchains. For each Linux architecture, record the exact glibc, libstdc++, and libgcc provenance; provide the
  applicable notices and license texts, corresponding sources, and a tested LGPL 2.1 section 6 relinking mechanism.
  Complete any additional toolchain-specific obligations before publication.
- [ ] **Release provenance:** decide whether production releases require artifact signing, macOS notarization, and a
  machine-readable SBOM or a separately published archive checksum. Until then, release artifacts are explicitly
  unsigned; the archive contains per-file SHA-256 checksums and the documented application-dependency notices and
  license texts.
