# ctrace TODO

This list contains only work that is currently open in ctrace. Completed work and duplicated implementation details
are intentionally omitted.

- [ ] **Input schema:** select and document the supported CMSIS-Toolbox trace revision and define the architecture,
  comparator-role, grouped-source, and symbol metadata required for complete decoding.
- [ ] **Metadata model:** preserve logical references, bindings, grouped resources, processor identity, and exact route
  context instead of flattening DWT source arrays.
- [ ] **DWT architecture:** complete Armv7-M comparator semantics and add Armv8-M match, compressed address/PC,
  linked-pair reconstruction, recovery boundaries, and architecture-specific diagnostics.
- [ ] **Formatted trace:** add the OpenCSD frame deformatter and per-Trace-Bus-ID decoders for `*.TB.raw` input.
- [ ] **Outputs:** add multiple CTF clock classes; define and enable `pcsample`, `event`, and `pmu`; extend CSV, CTF,
  Trace Compass, multicore, and Armv8-M golden coverage.
- [ ] **OpenCSD boundary:** remove the remaining dependency on OpenCSD `common/` and `interfaces/` private headers and
  adopt an upstream-supported ITM-only target if one becomes available. OpenCSD 1.8.3's minimal static target includes
  all protocol decoders; maintaining a private source list is deliberately avoided.
- [ ] **Release verification:** inspect the first generated multi-platform ZIP, checksums, license texts, OpenCSD
  notices, and runtime dependencies. The hosted Windows, Linux, and macOS matrix and local AddressSanitizer/
  UndefinedBehaviorSanitizer suite are green; the final production-artifact inspection remains to be recorded.
- [ ] **Static runtime compliance:** inspect the GNU and Microsoft runtime code incorporated by the pinned release
  toolchains. For each Linux architecture, record the exact glibc, libstdc++, and libgcc provenance; provide the
  applicable notices and license texts, corresponding sources, and a tested LGPL 2.1 section 6 relinking mechanism.
  Complete any additional toolchain-specific obligations before publication.
- [ ] **Release provenance:** decide whether production releases require artifact signing, macOS notarization, and a
  machine-readable SBOM or a separately published archive checksum. Until then, release artifacts are explicitly
  unsigned; the archive contains per-file SHA-256 checksums and the documented application-dependency notices and
  license texts.
- [ ] **Release coverage:** add an Armv8-M end-to-end fixture when the corresponding DWT semantics are implemented;
  this extends the supported profile and does not block the initial ITM/DWT-focused release.
