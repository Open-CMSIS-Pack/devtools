# ctrace Threat Model

Status: Draft. Open mitigation actions and residual risks require review.

## Scope

ctrace is a native command-line utility that decodes local CoreSight RAW captures using YAML trace configuration
and produces CSV, CTF and Trace Compass XML output.

The scope includes input discovery, configuration parsing, trace decoding, diagnostics and output generation.
OpenCSD, yaml-cpp and cxxopts are incorporated into the executable. Capture producers, including pyTS, and downstream
analysis tools are separate components.

ctrace does not intentionally execute input as code, invoke shell commands, load plugins or communicate with network
services or debug hardware. It runs with the invoking user's permissions and does not provide an application sandbox.

Assessment baseline: `devtools` commit `ab99915b`, 14 October 2026.

## Assets and trust boundaries

Protected assets are:

- Availability of the host and invoking application.
- Integrity of files accessible to the invoking user.
- Confidentiality of captured trace data and generated artifacts.
- Integrity of decoded results and diagnostic messages.
- Integrity and authenticity of distributed ctrace binaries.

RAW files, YAML configuration, filenames and producer-supplied text may originate outside the user's trust boundary.
Local storage does not establish their trustworthiness.

The principal dataflow is:

```text
RAW capture + YAML configuration
    → native parsing and decoding
    → event processing and timestamp resolution
    → CSV / CTF / XML / CLI diagnostics
```

Generated artifacts cross another boundary when opened by downstream tools or shared with other users.

## Operational assumptions

The intended operating environment is a local process running without elevated privileges. The invoking user controls
the input/output directory, and input files and directory entries remain stable during conversion.

These are deployment assumptions, not restrictions enforced by ctrace. Automated integrations and shared,
concurrently writable directories require additional assessment. Input content remains potentially untrusted even
when these assumptions hold.

## Threats

### CTRACE-T1: Pending-event memory consumption

**STRIDE:** Denial of Service.

Captures with absent or widely separated local timestamps accumulate pending events and can exhaust memory.

**Current assessment:** Memory amplification reproduced. Pending-event storage has no explicit budget.

### CTRACE-T2: YAML memory consumption

**STRIDE:** Denial of Service.

Large YAML structures or repeated aliases cause excessive parsing and metadata allocations.

**Current assessment:** Memory amplification reproduced with a small YAML file. Input validation does not bound
expanded metadata.

### CTRACE-T3: Diagnostic text manipulation

**STRIDE:** Spoofing / Tampering.

Producer-supplied diagnostic text contains line breaks or terminal controls, allowing misleading log output or
manipulation of terminal presentation.

**Current assessment:** Reproduced in normal and verbose modes. Message text is emitted without control-character
escaping.

### CTRACE-T4: Invalid XML output

**STRIDE:** Tampering / Denial of Service.

Metadata containing characters prohibited by XML produces unusable analysis files and disrupts downstream processing.

**Current assessment:** Reproduced: conversion succeeds while generated XML is invalid.

### CTRACE-T5: Native parser and decoder defects

**STRIDE:** Tampering / Information Disclosure / Denial of Service.

Defects in native parsing or decoding could cause crashes, memory corruption or execution with the invoking user's
permissions.

**Current assessment:** Architectural exposure. No new code-execution exploit demonstrated by this assessment.

### CTRACE-T6: Concurrent filesystem modification

**STRIDE:** Tampering / Denial of Service.

Concurrent replacement of filesystem paths or links could redirect output operations or affect unintended files.

**Current assessment:** Conditional on directory control. Static output-link handling is tested; concurrent
replacement remains unverified.

### CTRACE-T7: Trace data disclosure

**STRIDE:** Information Disclosure.

Generated artifacts or logs expose trace payloads, addresses, metadata or filesystem paths to unintended readers.

**Current assessment:** Output permissions can be broader than input permissions. Exposure depends on directory
permissions and subsequent sharing.

## Existing mitigations

- Validation of consumed YAML structures, numeric ranges and duplicate keys; yaml-cpp also limits nesting depth.
- RAW regular-file preflight and retention of the opened input handle.
- Bounded decoder feeding, progress checks and controlled handling of decoding errors.
- Separate decoding state for each input and reporting of incomplete output.
- Output-name validation and removal of existing output links before replacement.
- Unit and integration tests, source analysis and coverage checks.

These controls provide partial mitigation. Bounded input reads do not bound accumulated events, and source-line
coverage does not establish security against arbitrary input.

## Open mitigation actions

- **CTRACE-T1:** Bound pending-event memory and define resource-exhaustion behavior while preserving timestamp semantics.
  **Status:** Open.
- **CTRACE-T2:** Bound input bytes, collection sizes, strings and total expanded metadata before large allocations.
  **Status:** Open.
- **CTRACE-T3:** Safely render untrusted diagnostic messages and context values in both CLI modes.
  **Status:** Open.
- **CTRACE-T4:** Enforce valid XML characters and verify generated documents using an XML parser.
  **Status:** Open.
- **CTRACE-T5:** Add resource-bounded fuzzing and sanitizer testing covering ctrace and its native dependencies.
  **Status:** Open.
- **CTRACE-T6:** Confirm directory-control assumptions; harden and test concurrent path replacement if shared directories
  are supported. **Status:** Decision required.
- **CTRACE-T7:** Define output-permission, diagnostic-sharing and retention requirements.
  **Status:** Decision required.

## Shared controls and residual risks

Applicable Toolbox threats concerning source, dependencies, CI and release artifacts also apply to ctrace.

Dependency assurance requires explicit evidence: published CodeQL results exclude primary findings under `external/`,
and the current Dependabot configuration covers GitHub Actions. Dedicated vulnerability monitoring for shipped C++
dependencies remains to be established. Release signing, provenance and SBOM requirements remain pending decisions.

The identified residual risks remain open. Risk ratings, ownership and acceptance require review by the responsible
product and security owners. This section does not record acceptance of those risks.
