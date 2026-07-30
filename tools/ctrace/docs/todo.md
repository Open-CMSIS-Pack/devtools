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
- [ ] **OpenCSD boundary:** remove the remaining dependency on OpenCSD `common/` and `interfaces/` private headers.
- [ ] **Release verification:** obtain a green ctrace GitHub Actions run for the Windows, Linux, and macOS matrix and
  inspect the generated multi-platform ZIP, checksums, and included license texts. Local Debug and Release tests are
  green; sanitizer and hosted platform results remain to be recorded.
- [ ] **Release provenance:** decide whether production releases require artifact signing, macOS notarization, and a
  machine-readable SBOM. Until then, release artifacts are explicitly unsigned and protected by published SHA-256
  checksums plus complete third-party notices and license texts.
- [ ] **Release coverage:** add an Armv8-M end-to-end fixture when the corresponding DWT semantics are implemented;
  this extends the supported profile and does not block the initial ITM/DWT-focused release.
