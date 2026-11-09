# Instruction Trace Implementation Plan

Updated: 2026-11-06. Branch: `feat/ctrace-instruction-trace`.
OpenCSD: **1.8.3**, commit `59f15320637ce6e510604e7bed550d7ad109d15d`.

## Experimental specification

Basis: [CMSIS-Toolbox Experimental Features][experimental-spec], `ctrace` and `*.ctrace-run.yml`.

### Trace

#### Directory and File Structure

| Directory or File | Created by | Description |
| :---|:---|:--- |
| `.trace/<solution-set>.ctrace-run.yml` | pyTS / capture tooling | Source configuration and image inventory. |
| `.trace/<solution-set>.<channel>.raw` | Capture backend | ETM input: formatted `TB` or `TB_<name>`. |
| `.trace/<solution-set>.<channel>.<mode>.csv` | ctrace | CSV including instruction trace |

- One trace-buffer RAW file contains multiple trace sources, identified by their ATB `stream` IDs.
- Decode each source with its own configuration and state; combine all sources in one CSV, retaining `stream`.

#### File Structure of `*.ctrace-run.yml`

| `ctrace-run:` | | Content |
| :---|:---|:--- |
| &nbsp;&nbsp;&nbsp; `ctrace-images:` | Optional | Processor image inventory for instruction reconstruction. |

| `ctrace-images:` | | Content |
| :---|:---|:--- |
| `- pname:` | **Required** | Unique processor instance name, including single-core captures. |
| &nbsp;&nbsp;&nbsp; `elf:` | Optional | List of `.elf`/`.axf` paths, relative to `*.ctrace-run.yml`. |
| &nbsp;&nbsp;&nbsp; `hex:` | Optional | List of Intel HEX paths, relative to `*.ctrace-run.yml`. |

- Declared image entries need a nonempty file list; processors without images may be omitted.
- Select one format per processor; selection when both lists exist: TBD. HEX production remains open.
- Map matching code to execution addresses; reject invalid files and conflicting overlaps.

> **Possible extension:** Mixed ELF/HEX files per `pname`.

ETM source entries:

| `ctrace-refs:` | | Content |
| :---|:---|:--- |
| `- ref:` | **Required** | Trace-setting reference, for example `CM7/instructions`. |
| &nbsp;&nbsp;&nbsp; `type:` | **Required** | New trace source type: `etm`. |
| &nbsp;&nbsp;&nbsp; `pname:` | Conditional | Required with image inventory; agrees with any processor prefix in `ref`. |
| &nbsp;&nbsp;&nbsp; `stream:` | **Required** | Capture-time CoreSight ATB ID; decoder Trace ID. |
| &nbsp;&nbsp;&nbsp; `decoder:` | **Required** | Capture-specific ETM decoder configuration. |

| `decoder:` | | Content |
| :---|:---|:--- |
| &nbsp;&nbsp;&nbsp; `core:` | **Required** | `Cortex-M4` (ETMv3.5) or `Cortex-M7` (ETMv4.0). |
| &nbsp;&nbsp;&nbsp; `regs:` | **Required** | Fixed qualified Cortex-M implementation values, written to YAML by pyTS. |
| &nbsp;&nbsp;&nbsp; `regsToRead:` | **Required** | Variable configuration registers read by pyOCD. |

- ETM references need no `index`; reject ambiguous processor/source bindings.
- A source's `pname` may have no image entry; this selects image-free fallback, not another processor's images.
- Retain unambiguous legacy processor inference without an inventory and existing DWT `symbol-file` provenance.

##### Register Accesses

| Cortex-M profile | `regs` (pyTS) | `regsToRead` (pyOCD) |
| :---|:---|:--- |
| Cortex-M4 / ETMv3.5 | `ETMIDR`, `ETMCCER` | `ETMCR` |
| Cortex-M7 / ETMv4.0 | `TRCIDR0`, `TRCIDR1`, `TRCIDR2`, `TRCIDR8` | `TRCCONFIGR` |

| `regs:`, `regsToRead:` | | Content |
| :---|:---|:--- |
| `- name:` | **Required** | Symbolic register name. |
| &nbsp;&nbsp;&nbsp; `value:` | Conditional | Required in `regs`; filled by pyOCD in `regsToRead`. |

- `ETMCR` / `TRCCONFIGR`: complete effective configuration.

##### Example

M7 reference values; existing setup/disable entries omitted. ELF and HEX lists illustrate the pending format selection:

```yml
ctrace-run:
  ctrace-images:
    - pname: CM7
      elf: [Bootloader.axf, Firmware.elf]
      hex: [Bootloader.hex, Firmware.hex]
  ctrace-refs:
    - ref: CM7/instructions
      pname: CM7
      type: etm
      stream: 0x12
      decoder:
        core: Cortex-M7
        regs:
          - name: TRCIDR0
            value: 0x080006e1
          - name: TRCIDR1
            value: 0x4100f401
          - name: TRCIDR2
            value: 0x00000004
          - name: TRCIDR8
            value: 0x00000001
        regsToRead:
          - name: TRCCONFIGR
            value: 0x00000001  # Filled by pyOCD
```

#### Initial Implementation

- ETMv3 and ETMv4: default one PC per line (`instructions`), using ELF or HEX; optional image-free `packets`.

#### Remaining Design Questions

- Whether to offer `--etm-output decoded`: generic OpenCSD output elements, using ELF or HEX.
- ELF/HEX selection when both lists are present; ownership of image-inventory and HEX production.
- Producer/CMSIS schema alignment for these extensions.

#### Out-of-Scope

- Disassembly and ETM CTF.

### `pyTS` Utility

- Emit consistent `pname` values on DWT/ETM references and any image entries.
- Populate `decoder.regs` from the qualified Cortex-M implementation; list variable registers in `decoder.regsToRead`.

### `ctrace` Utility

```txt
Usage:
  ctrace <trace-dir> [options]

Options:
      --csv                        Export CSV (ETM default: instructions)
      --type sel [...]              Filter output types; additional ETM selectors: etmv3, etmv4
      --etm-output mode             Select ETM mode for --csv (default: instructions)
        instructions               One PC per line (ELF or HEX)
        packets                    ETM protocol packets (no image)
        decoded                    OpenCSD generic elements (ELF or HEX; TBD)
```

- `--csv`: ETM output uses `instructions`.
- Optionally add `--etm-output mode` to select another ETM output mode.

#### `--type` option

- Selectors `etmv3`, `etmv4` select CSV types `ETMv3`, `ETMv4`.

#### `--etm-output` option

| Mode | Required images | Output records |
| :---|:---|:--- |
| `instructions` (default) | ELF or HEX; fallback if absent | PCs, events/status and fallback packets. |
| `packets` | None; no image opened | ETM protocol packets, parser status and diagnostics. |
| `decoded` (TBD) | ELF or HEX | Generic OpenCSD ranges, events and status. |

- No matching image inventory: warn once per source/input and use packet fallback in `instructions`.
- Image-backed modes: missing/unreadable declared files, invalid images and ambiguous format selection are errors.
  `packets` neither validates image files nor selects a format. Unmapped code uses local packet fallback.
- `decoded` remains unavailable until offering it is decided; if offered, absent images are an error.
- `--etm-output` requires CSV output (`--csv` or `--all`); it does not change ITM/DWT decoding.
- Without an output option, validate/decode using the default mode and fallback; write no output file.
- Unknown/unimplemented modes are errors. Fallback retains the selected filename and header.

**Example:**

```bash
ctrace .trace --csv
ctrace .trace --csv --channel TB --etm-output packets
```

#### CSV Format

**Common fields:** existing column meanings and relative order, with `packet` added.

```csv
cycles,stream,type,packet,index,value,pc,address,note
```

| Mode | Additional fields before `note` | Total columns |
| :---|:---|:--- |
| `instructions` | `execution` | 10 |
| `packets` | `packet_size`, `packet_bytes` | 11 |
| `decoded` (TBD) | `instruction_count` | 10 |

| Column | ETM meaning |
| :---|:--- |
| `cycles` | Attributable CPU-cycle position; otherwise empty. |
| `stream` | CoreSight ATB ID; a processor may own several sources. |
| `type` | `ETMv3` or `ETMv4`; distinct from YAML `type: etm`. |
| `packet` | `INSTRUCTION`, protocol class, generic record kind or parser status. |
| `index` | Architectural exception/vector number when known; otherwise empty. |
| `value` | Instruction opcode, exception transition or valid timing value, in hexadecimal. |
| `pc` | Instruction/range start; empty for protocol-only address reports. |
| `address` | Range end, inaccessible code address, protocol address or preferred exception-return address. |
| `execution` | Predication: `executed`, `not_executed`, `unknown`; not retirement/completion. |
| `instruction_count` | Decoded range count; unknown counts stay empty. |
| `packet_size` | Captured ETM source-byte count, including packet headers. |
| `packet_bytes` | Space-separated hex bytes in trace order, after CoreSight deformatting. |
| `note` | Structured execution reasons, packet/event fields and diagnostics. |

- One CSV per RAW input. Configured ETM sources select the mode filename/header before row filtering,
  even when a file contains no ETM bytes. ITM/DWT rows retain their payload fields;
  `packet` and ETM-only fields stay empty. Inputs without configured ETM retain the eight-column CSV.
- Unavailable/inapplicable values stay empty. Preserve per-source order; no cross-source execution-time claim.
- ETM `note`: `key=value` pairs separated by `;`; escape backslash, semicolon and line endings as
  `\\`, `\;`, `\r`, `\n`, then apply normal CSV quoting. Free diagnostic text uses `message`.
  Absent keys mean unavailable, not zero. Existing ITM/DWT notes keep their format.

**`instructions`:**

- Expand complete ranges once, one PC per line; `packet=INSTRUCTION`, opcode in `value`, `index`/`address` empty.
  Retain not-executed/unknown PCs and all other decoded events/status; never fabricate missing PCs.
- Explain execution state with `execution_basis` in `note`. CBZ/CBNZ can execute without taking the branch.
- Preserve opcode width in `value`; Thumb-32 uses first then second little-endian halfword:
  bytes `2d e9 f0 41` become `value=0xe92d41f0`.
- Uncovered code: preserve valid PC prefixes and gaps, then emit the corresponding complete protocol packet once.
  Continue packets through the gap; resume PCs only when code and decoder state permit reconstruction.
  A packet may cover several ranges and overlap retained PCs: mark this with `partial=1`.
- Fallback notes contain `record=packet`, `reason=no_image|image_gap`, `packet_size`, `packet_bytes` and packet fields.
  Instruction-specific columns stay empty. At a shared origin, decoded rows precede the complete fallback packet.
  Fallback preserves observed packets; it is not an independently decodable subtrace.

```csv
cycles,stream,type,packet,index,value,pc,address,execution,note
,18,ETMv4,INSTRUCTION,,0x2902,0x08009a1c,,unknown,execution_basis=IT_state_unavailable
,18,ETMv4,INSTRUCTION,,0xd127,0x08009a1e,,not_executed,execution_basis=conditional_branch_predicate
,18,ETMv4,INSTRUCTION,,0x6a74,0x08009a20,,executed,execution_basis=unpredicated_path
```

**`packets`:** one row per protocol packet or parser status; no instruction reconstruction.

```csv
cycles,stream,type,packet,index,value,pc,address,packet_size,packet_bytes,note
,18,ETMv4,atom_f1,,,,,1,f7,atoms=E
```

**`decoded` (TBD):** one row per generic OpenCSD element; ranges remain ranges.

```csv
cycles,stream,type,packet,index,value,pc,address,instruction_count,note
,18,ETMv4,instr_range,,,0x08000100,0x08000108,3,last_executed=1
,20,ETMv4,instr_range,,,0x08000100,0x08000104,2,last_executed=1
```

**Record fields:** apply only when valid for the record; other payload columns stay empty. [O3], [O17]

| Record | CSV mapping |
| :---|:--- |
| `instr_range` | `pc` = start, `address` = exclusive end; `instruction_count`; final flags in `note`. |
| `i_range_nopath` | `pc` = start, `address` = continuation; valid count; no PC expansion. |
| Generic `exception` | Normalized `index`; `value=0x1` only for architectural entry; valid return `address`. |
| `exception_ret` | Return marker in `note`; no inferred destination exception or ITM exit/return transition. |
| `pe_context` | Context/security fields and validity in `note`. |
| `addr_nacc` | `address` = inaccessible code address. |
| `timestamp`, `cycle_count` | Valid complete value in `value`; domain/association and partial fields in `note`. |
| `event`, `sync_marker` | Event/marker kind and value in `note`; an event mask is not an exception index. |
| `trace_on`, `no_sync`, `eo_trace`, `addr_unknown`, `unknown` | Reason/state in `note`. |
| Protocol address | Complete valid address in `address`; partial address bits/mask in `note`. |
| Protocol exception fields | Normalized `index` when known; raw code, event kind and flags in `note`. |
| Other protocol fields | Structured atoms, context, counts, speculation and validity in `note`. |
| Parser status | `record=status`, status-specific reason and available source bytes; no invented packet. |

- Normalize Cortex-M exceptions using the protocol-specific tables; OpenCSD's generic number is still protocol-raw.
  Preserve `exception_raw` and `exception_kind` in `note`. Debug halt, lazy FP push and lockup leave `index`/`value` empty.
- ETMv4 reset has no known return address despite the generic API flag: leave `address` empty.
  Preserve the transmitted address only as a protocol field with `address_role=unknown`. [A10, p. 270][A10]
- Preserve valid generic final-instruction flags as `last_executed`, `last_size`, `last_conditional`, `last_type`,
  `last_subtype` in `decoded` notes; in `instructions` they are classifier inputs, not range-wide execution claims.
- Copy remaining semantic fields using OpenCSD member names under `packet.` or `element.` in `note`;
  omit the constant ISA, duplicate mappings, inactive union members and internal bookkeeping.
  Do not parse display strings.
- Preserve ETMv3 cancellation information (`cancel_previous=1`) even when images cover the entire trace.
- `cycles` needs qualified association; raw timestamps/counts do not imply per-PC timing or cross-core synchronization.
  Preserve their domain, validity and association in `note`; do not distribute cycles across PCs.

Packet names and parser status: Appendix B. Exception normalization: section 3.3.

##### Loopback Requests

- `ETMv3` / `ETMv4` resolve to `type: etm` references using `solution-set` and `stream`.
- `packet` and ETM exception `index` do not select separate trace settings.
- Recognize the mode suffix in `<solution-set>.<channel>.<mode>.csv`; input-wide diagnostics have no source reference.

#### CTF Format

- Evaluate CTF eligibility for configured sources after source/type selection.
  Selected ETM sources make that input's CTF unsupported, even if no ETM bytes were observed.
- With `--all`, report the CTF error and return nonzero; still generate CSV and process other inputs.
  Excluding ETM permits existing ITM/DWT CTF output.

[experimental-spec]: https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/Experimental-Features.md

## 1. Status and scope

| Item | State |
| --- | --- |
| Production | ITM/DWT supported; ctrace ETM integration pending. Experimental specification above is the contract. |
| Required profiles | Cortex-M4/ETMv3.5 and Cortex-M7/ETMv4.0. |
| Capture evidence | Both profiles decoded with the host helper; PC/classifier validation currently M7 only. |
| Decoder blockers | Pinned OpenCSD ETMv3 cancellation and truncation defects; section 5. |
| Images | ELF/HEX import and parity with archived BIN references remain to be implemented/qualified. |

## 2. Code-image provider

- Merge selected regions per `pname`; different processors may map different bytes at the same address.
- Static instruction bytes at execution addresses, including RAM code; no relocation, gap padding or dynamic overlays.
- `.bin` is historical reference data, not an input format. Inventory and HEX production ownership remain open.

| Reader | Import contract |
| --- | --- |
| Intel HEX | Validate record syntax/counts/checksums, EOF and bounds; `00`, `01`, `02`, `04` address/data records. |
| HEX start records | Validate `03`/`05`; do not use them as image bases or trace starting PCs. |
| ELF/AXF | ELF32, little-endian `EM_ARM`, static `ET_EXEC`, usable section headers. |
| ELF code | `SHT_PROGBITS`, `SHF_ALLOC`, `SHF_EXECINSTR`; nonempty bytes at `sh_addr`, any section name. |

Validate bounds, address overflow, available data and conflicting overlaps before copying into owned regions.
Reject unsupported compression, relocation and sectionless ELF; leave BSS and holes unmapped. [E2], [E4], [H1], [H2]
For scatter-loaded RAM code, `sh_addr` is the execution address; a segment load address is insufficient. [E1], [E6]

Direct ELF import candidate: ELFIO, header-only C++, MIT; pin the dependency and retain its notices. [E3], [L1]
Copy bytes before releasing ELFIO storage. HEX and ELF feed the same provider; no HEX conversion is required. [E5]

## 3. Integration boundaries

### 3.1 Source configuration

| YAML | OpenCSD mapping |
| --- | --- |
| `core: Cortex-M4` | `OCSD_BUILTIN_DCD_ETMV3`, `ARCH_V7`, `profile_CortexM`. |
| `core: Cortex-M7` | `OCSD_BUILTIN_DCD_ETMV4I`, `ARCH_V7`, `profile_CortexM`. |
| `stream` | ETMv3 `reg_trc_id` / ETMv4 `reg_traceidr`. |
| `ETMIDR`, `ETMCCER`, `ETMCR` | `reg_idr`, `reg_ccer`, `reg_ctrl`. |
| `TRCIDR0`, `TRCIDR1`, `TRCIDR2`, `TRCIDR8` | `reg_idr0`, `reg_idr1`, `reg_idr2`, `reg_idr8`. |
| `TRCCONFIGR` | `reg_configr`. |

- Make `core_prof` configurable; default to `Cortex-M` (`profile_CortexM`).
  Its configuration interface is not part of the Experimental specification yet.
- Pending specification update: obtain `TRCCONFIGR` from the pyOCD "write registers" block,
  not the "ID regs" block. Keep the Experimental specification unchanged for now.
- `decoder.regs` is metadata; existing reference/disable `regs` retain hardware-write semantics.
- Fixed values belong to the qualified ETM implementation/revision, not every instance of a Cortex-M name.
  Offline decoding needs resolved capture-time configuration; pyOCD supplies no static replacement.
- Initialize unused OpenCSD ETMv4 IDR9–13 fields deterministically; they are not measured registers. [O10], [O14]
- Bind `stream -> reference -> pname -> code regions`; preserve DWT symbol-file provenance separately. [C4], [C13]

M4 source example; the corresponding image entry, when present, uses `pname: CM4`:

```yaml
- ref: CM4/instructions
  pname: CM4
  type: etm
  stream: 0x22
  decoder:
    core: Cortex-M4
    regs:
      - name: ETMIDR
        value: 0x4114f250
      - name: ETMCCER
        value: 0x18541800
    regsToRead:
      - name: ETMCR
        value: 0x00000810  # Filled by pyOCD
```

### 3.2 Decoder and packet monitoring

| Boundary | Responsibility |
| --- | --- |
| CLI / output planning | ETM selectors/mode, fixed schema/name per input; independent CSV/CTF eligibility. |
| YAML / source routing | Neutral configuration fields and source/image binding; no OpenCSD types in YAML models. |
| `FileDecodeJob` / image readers | Owned code regions and Trace-ID-aware memory access. |
| `DecodePipeline` / ETM session | One tree per file, independent ETMv3/ETMv4/ITM sources and diagnostics. |
| ETM adapters / CSV | Structured packets, generic events, range expansion and deterministic fallback merge. |

- Use `OCSD_TRC_SRC_FRAME_FORMATTED` / `OCSD_DFRMTR_FRAME_MEM_ALIGN` for TB input. [O2]
- `packets` and full-source fallback: `OCSD_CREATE_FLG_PACKET_PROC` plus `OCSD_C_API_CB_PKT_MON`.
  MON provides packet bytes and length; `OCSD_C_API_CB_PKT_SINK` alone does not. Add SINK only for flow control. [O30]
- Image-backed `instructions` / proposed `decoded`: `OCSD_CREATE_FLG_FULL_DECODER`, memory accessor before data,
  `ocsd_dt_set_gen_elem_outfn` and MON. Copy callback-owned fields/bytes immediately. [O26]
- Use `ocsd_dt_add_callback_trcid_mem_acc` for processor-local mappings; return actual available bytes.
  A missing accessor is fatal; an unmapped read returns short/zero and may emit `ADDR_NACC`. [O26], [O27]
- Provider/storage outlive the tree. The cache keys Trace ID + address; dynamic code/security mappings need separate
  qualification and invalidation, not just a different callback response. [O19], [O23], [O24], [O25]
- Route identity includes source/ATB ID; `pname` alone is not a route key. [C16]

**Mixed-output merge:**

- Retain packets and generic elements per source, with packet order, protocol position and local element order.
  Association is many-to-many; decoder positions are not physical RAW offsets.
- Finalize after decoder EOT; spool if necessary to bound memory. Preserve generic-element order and complete
  valid PC prefixes. Let the full decoder resolve commit, cancel, mispredict and discard before PC export.
- `ADDR_NACC` opens an image-gap interval at its originating packet. Keep that complete packet and subsequent
  packets through the interval; stop before the next successfully reconstructed packet origin.
  If that origin also contains a gap, retain its valid PCs and its packet once.
- For one origin: generic rows/expanded PCs first, then its fallback packet. Set `partial=1` when it also produced PCs.
  Missing generic output alone does not establish an image gap; canceled/unresolved atoms are not PC rows.
- Only generic output drives instruction/context classification. MON additionally preserves source fields omitted
  by generic events, including ETMv3 cancellation; unresolved EOT state remains explicit.

### 3.3 Cortex-M exception normalization

`index` uses architectural exception/vector numbers, not protocol encodings or an IPSR sample.
Reset is vector 1; exception-return/tail-chain markers alone do not identify the destination exception.
Read raw structured fields; OpenCSD passes them unchanged into generic `exception_number`. [O25], [O31]

| Protocol | Raw code | Architectural number |
| --- | --- | --- |
| ETMv3 | `0x01..0x07` | `raw + 16` (IRQ1..7). |
| ETMv3 | `0x08` | 16 (IRQ0). |
| ETMv3 | `0x09`, `0x0a`, `0x0d` | 6 (UsageFault), 2 (NMI), 4 (MemManage). |
| ETMv3 | `0x0b`, `0x0c`, `0x0e`, `0x0f` | Unchanged. |
| ETMv3 | `0x11`, `0x13`, `0x15` | 1 (reset), 3 (HardFault), 5 (BusFault). |
| ETMv3 | `0x18..0x1ff` | Unchanged (IRQ8..495). |
| ETMv4 M | `0x01..0x06`, `0x0b`, `0x0c`, `0x0e`, `0x0f` | Unchanged. |
| ETMv4 M | `0x10..0x17` | Unchanged (IRQ0..7). |
| ETMv4 M | `0x208..0x3ef` | `raw - 0x1f0` (IRQ8..495). |
| ETMv4 M | `0x18`, `0x19`, `0x1a` | Empty: debug halt, lazy FP push, lockup. |

ETMv3 code zero means no exception. Other reserved/implementation-defined codes remain unnormalized;
SecureFault is outside M4/M7. Preserve raw code and event kind. Sources: IHI0014Q Table 7-11, pp. 319–320;
IHI0064H.a Table 6-13, pp. 268–269; DDI0403E.e pp. 517, 525. [A11], [A10], [A12]

## 4. Instruction execution classification

- Verify range instruction count, exclusive end and final size against code bytes. M4/M7 use mixed-width Thumb;
  neither PC bit zero nor range byte length establishes instruction count.
- `execution` describes the predicate, not completion. Canceled/interrupted instruction facts remain separate events;
  no invented completed PC for a partially resumed instruction.
- Track IT state: ordinary taken branches leave an IT block; Armv7-M exception entry starts with ITSTATE=0.
  Exception return restores unknown IT state until evidence establishes it. Repeated unchanged context preserves state.
  Debug halt, lazy FP push and lockup are not handler-entry indications and do not by themselves clear IT state.
- Omitted Context-ID/VMID fields retain knowledge, including unknown. Gaps/unexplained context changes invalidate IT state.
- ETMv3 E/N normally reports passed/failed predicates; apply instruction-specific exceptions such as CBZ/CBNZ first.
  Use resolved endpoints after cancellation. ETMv4 interpretation also depends on enabled features. [A11], [A10]
- `last_executed` describes only the range endpoint. An N atom may mean branch not taken; it does not always mean an
  unexecuted instruction. A non-branch endpoint with `last_executed=1` alone does not prove an IT predicate. [O3], [A12]
- Use structured context and protocol fields. The prototype's display-string parser is not the production interface.

| `execution_basis` | Meaning |
| --- | --- |
| `unpredicated_path` | Known outside IT predication. |
| `conditional_branch_predicate` | Conditional branch result establishes its predicate. |
| `etmv3_atom_predicate` | Resolved E/N predicate after instruction-specific exceptions, including CBZ/CBNZ. |
| `IT_state_unavailable` | IT state cannot be reconstructed. |
| `IT_predicate_not_traced` | IT block known, predicate result absent from trace. |
| `CBZ_CBNZ_comparison` | Comparison executes even when the branch is not taken. |
| `IT_instruction` | IT instruction establishes the following block. |
| `branch_taken` | Observed taken branch establishes that instruction's predicate. |
| `IT_branch_predicate_failed` | Untaken branch in a known IT block has a failed predicate. |
| `missing_branch_outcome` | Branch outcome is unavailable. |
| `branch_predicate_context_unavailable` | Untaken branch without sufficient predicate context. |
| `nonbranch_last_flag_unqualified` | Endpoint flag alone cannot establish a non-branch predicate. |
| `predicate_analysis_profile_unqualified` | No qualified classifier for this profile/ISA. |

Do not infer missing predicate results from later register snapshots.
ETMv3 PC expansion/classification still needs independent qualification.

## 5. OpenCSD restrictions and qualification risks

Details and reproducible defects: [Known OpenCSD issues](opencsd-issues.md).

| Area | Pinned-version result / consequence |
| --- | --- |
| ETMv3 cancellation | Packed EE followed by cancellation can retain a canceled PC; decoder correction required. |
| ETMv3 IRQ8 field | `cm_irq_n` is 24 instead of 8; normalize from the raw exception code. |
| ETMv3 truncated branch at EOT | Reported as a valid branch with stale fields; truncation handling needs correction. |
| ETMv3 exception metadata | Generic `EXCEPTION` omits the raw Cancel bit; preserve it through packet monitoring. |
| ETMv4 reset address | Generic return-address flag does not establish architectural validity for reset. |
| ETMv4 conditional non-branch trace | Parser recognizes it; full decoder rejects configuration with `HW_CFG_UNSUPP`. |
| Armv8.1-M branch-forward | Not qualified here; fixtures do not establish complete support. |
| Cortex-M23 / ETMv3 | Decoder checks `ARCH_V7`, core map selects `ARCH_V8`; qualification required. |

- ETMv3 support remains required; correct cancellation and EOT handling before qualifying its outputs.
  The CM4 hardware capture contains no cancellation packet and does not cover that defect.
- The M7 reference has conditional non-branch tracing disabled; absent IT predicate results remain `unknown`.
  Enabling it requires a decoder that supports the configuration; changing archived metadata cannot recover results.
- Additional cores, security/dynamic mappings and protocol revisions need separate qualification. Unsupported features
  must remain explicit; packet-parser support is not full instruction-decoder support. [O1], [O8], [O13], [O25], [O28]

## 6. Reference evidence

Companion workspace: `/Users/thode01/work/github/TraceBufferExamples`; captures retain their original schemas.

| Reference | Evidence | Remaining limit |
| --- | --- | --- |
| [STM32H7B3I-DK / M7][ref-m7] | 4-KiB ETF, ATB `0x12`; packets, ranges and PC validation. | ELF/HEX parity. |
| [STM32H747I-DISCO / M4][ref-m4] | 4-KiB ETF, ATB `0x22`; packets and ranges. | PC/classifier qualification. |
| [ETMv3 cancellation][ref-cancel] | Synthetic packed/split atom comparison. | Host reproduction. |
| [ETMv3 packet fields][ref-fields] | IRQ8 field and truncated-branch cases. | Host reproduction. |

- Hardware bundles contain matching RAW, images, decoder metadata and validation reports; both capture windows wrap.
- M7 PC evidence: `validation/pc-expansion/README.md`; hardware/project configuration: each board's `TRACE_SETUP.md`.
- Shared host helpers: `STM32H7B3I-DK/tools/`. They establish reference evidence, not production ctrace ETM support.

## Appendix A. Trace representations

| Representation | Meaning |
| --- | --- |
| Architectural elements | ETMv4 P0: atoms, exceptions, Q, M-profile returns; other elements: state/resolution. |
| Protocol packets | Encodings of elements, potentially several per packet; Appendix B. |
| OpenCSD generic elements | Decoded ranges, events and status; mapped in the CSV specification. |
| Instruction rows | Image-backed expansion of complete generic ranges, with qualified predication. |

ETMv4 element groups: IHI0064H.a Figure 2-3, p. 34. [A10]
One protocol packet is not one range or one instruction; packet names do not imply instruction execution.

## Appendix B. CSV types and packet names

### B.1 Trace types

| Family or existing output | CSV `type` | Status |
| --- | --- | --- |
| ITM / DWT | `itm`, `dwt` | Existing software/data trace. |
| DWT events and PMU | `event`, `pmu` | Existing event categories. |
| Exceptions and PC samples | `exception`, `pcsample` | Existing records; not ETM instruction reconstruction. |
| Timing and diagnostics | `global_ts`, `overflow`, `error`, `info` | Existing output categories. |
| ETMv3 | `ETMv3` | Required; Cortex-M4/ETMv3.5 hardware reference. |
| ETMv4 | `ETMv4` | Required; Cortex-M7/ETMv4.0 hardware reference. |
| PTM / ETE | `ptm`, `ete` | Possible later instruction protocols; OpenCSD decoders exist. |
| STM | `stm` | Possible later software trace; OpenCSD decoder exists. |

Existing source/event categories retain their meanings. `info` marks skipped input bytes and is not selectable.
[C10], [C11] DWT travels through ITM, not a separate protocol decoder. Minor ETM revisions remain configuration,
not distinct `type` values. Profile qualification and decoder restrictions are in section 5. [O1], [O6]

### B.2 Packet inventory

Packet classes below include formats beyond the required M4/M7 profiles.
Names use lowercase enum suffixes, normalizing `A_SYNC` to `async`, `CTXT` to `context`, `EXCEPT` to `exception`,
and `EXCEPT_RTN` to `exception_return`. Format/address variants stay distinct.
Availability depends on revision, capabilities and capture configuration; not every core emits every listed packet.

Sources: IHI0014Q Table 7-1, pp. 301–302 [A11], and IHI0064H.a Table 6-1, pp. 239–241 [A10], checked against
OpenCSD enums/processors. [O10], [O12], [O14], [O28] Appendix A separates architectural and decoded elements.

#### Shared ETMv3 / ETMv4 packet names

| `packet` | Meaning / qualification |
| --- | --- |
| `async` | Alignment synchronization (A-Sync). |
| `timestamp` | Transmitted timestamp; ETMv3.5+ and configured timestamp support. |
| `ignore` | Ignored padding packet; ETMv4.3+ for the ETMv4 instruction stream. |

Shared names describe roles; encoding and validity remain protocol-specific. Other shared concepts use different
classes: atoms are ETMv3 `p_hdr` versus ETMv4 `atom_f1`–`atom_f6`; context uses ETMv3 `i_sync`/`context_id`/`vmid`
versus ETMv4 `context`/address-with-context. The lists separate encodings, not exclusive capabilities.

#### ETMv3-only packet classes

| `packet` | Meaning / qualification |
| --- | --- |
| `branch_address` | Branch address; can also carry exception information. |
| `i_sync`, `i_sync_cycle` | Instruction synchronization, without/with cycle count. |
| `p_hdr` | E/N atoms; retain P-header format 1–4 in the packet fields. |
| `cycle_count` | Standalone cycle count. |
| `trigger` | Trigger indication. |
| `context_id`, `vmid` | Context-ID / VMID update; VMID is an ETMv3.5 feature. |
| `exception_exit` | Exception-return marker. |

Instruction-only exception entry uses branch-address fields. Trace enable/overflow are I-Sync reasons;
there are no standalone ETMv3 `trace_on`/`overflow` packets. [A11], [O12]

#### ETMv4-only instruction-stream packet classes represented in OpenCSD

| `packet` | Meaning / qualification |
| --- | --- |
| `trace_info`, `trace_on` | Decoder synchronization state / trace becoming active. |
| `discard`, `overflow` | Discarded speculative state / trace overflow. |
| `exception`, `exception_return` | Exception and exception-return indication. |
| `ccnt_f1`, `ccnt_f2`, `ccnt_f3` | Cycle-count formats 1–3. |
| `commit`, `mispredict` | Speculation resolution. |
| `cancel_f1`, `cancel_f1_mispred`, `cancel_f2`, `cancel_f3` | Cancellation formats; F1 can include misprediction. |
| `cond_i_f1`, `cond_i_f2`, `cond_i_f3` | Conditional-instruction formats 1–3. |
| `cond_res_f1`, `cond_res_f2`, `cond_res_f3`, `cond_res_f4` | Conditional-result formats 1–4. |
| `cond_flush` | Conditional trace flush. |
| `event` | Trace event indication. |
| `context` | Processor context, including implemented/transmitted Context-ID and VMID fields. |
| `addr_ctxt_l_32is0`, `addr_ctxt_l_32is1` | Long 32-bit address with context, instruction-set encoding IS0/IS1. |
| `addr_ctxt_l_64is0`, `addr_ctxt_l_64is1` | Long 64-bit address with context, IS0/IS1. |
| `addr_match` | Exact match with a retained address. |
| `addr_s_is0`, `addr_s_is1` | Short address, IS0/IS1. |
| `addr_l_32is0`, `addr_l_32is1` | Long 32-bit address, IS0/IS1. |
| `addr_l_64is0`, `addr_l_64is1` | Long 64-bit address, IS0/IS1. |
| `q` | Q packet; preserve its subtype and valid fields. |
| `atom_f1`, `atom_f2`, `atom_f3` | E/N atom formats 1–3. |
| `atom_f4`, `atom_f5`, `atom_f6` | E/N atom formats 4–6. |
| `ts_marker` | Timestamp marker; ETMv4.6+ with the feature implemented. |

Enum membership does not establish hardware capability or full-decoder support (section 5).
IS0/IS1 are protocol instruction-set encodings whose meaning depends on processor state. [O28], [A10]

#### Additional parsed ETMv4 classes outside the initial M4/M7 instruction-only profile

| `packet` | Scope |
| --- | --- |
| `func_ret` | Function-return packet; additional profile/revision qualification required. |
| `num_ds_mkr`, `unnum_ds_mkr` | Numbered/unnumbered data-synchronization markers; data-trace association. |

These classes have parser handlers; omission from the initial profile is not an OpenCSD parser defect. [O28]

#### Additional ETMv4 architecture packets absent from the pinned OpenCSD processor

| Proposed `packet` | Architecture / status |
| --- | --- |
| `branch_future_flush` | Added in ETMv4.5; branch-future state handling is not implemented here. |
| `resynchronization` | Added in ETMv4.5; no corresponding packet class/handler in this OpenCSD pin. |

These architectural entries require decoder work; see IHI0064H Table 6-1 and the ETMv4.5 change summary, p. 536.
[A10], [O1], [O28]

### B.3 Parser status

- Map `NOTSYNC`, `INCOMPLETE_EOT`, `BAD_SEQUENCE`, `BAD_TRACEMODE`, `RESERVED`, `RESERVED_CFG` to
  `notsync`, `incomplete_eot`, `bad_sequence`, `bad_tracemode`, `reserved`, `reserved_cfg` with `record=status`.
  They are parser states, not on-wire packets. Preserve reason and available bytes.
- `NOERROR` / `NO_ERR_TYPE` are sentinels; `BRANCH_OR_BYPASS_EOT` and `EXTENSION` are internal dispatch states,
  not exported packet classes. Extension headers resolve to concrete types.
- The pinned ETMv3 processor can report a truncated branch at EOT as a valid branch with stale fields;
  see [Known OpenCSD issues](opencsd-issues.md#etmv3-unfinished-packet-emitted-without-eot-error-status).
- Decoder diagnostics retain existing `error`, `overflow`, `info` categories and source attribution when known.
- ETE-only source-address, transaction and PE-reset entries in the shared header are outside this proposal.

## Sources

OpenCSD links identify the inspected pin; architecture editions/pages are stated at each use.

[O1]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/README.md
[O2]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/docs/prog_guide/prog_guide_main.md
[O3]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/docs/prog_guide/prog_guide_generic_pkts.md
[O6]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/ocsd_lib_dcd_register.cpp
[O8]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/opencsd/etmv3/trc_cmp_cfg_etmv3.h
[O10]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/opencsd/etmv4/trc_pkt_types_etmv4.h
[O12]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_proc_etmv3_impl.cpp
[O13]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/trc_core_arch_map.cpp
[O14]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/opencsd/etmv3/trc_pkt_types_etmv3.h
[O17]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/opencsd/trc_gen_elem_types.h
[O19]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/mem_acc/trc_mem_acc_mapper.cpp
[O23]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/mem_acc/trc_mem_acc_cache.h
[O24]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/mem_acc/trc_mem_acc_cache.cpp
[O25]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv4/trc_pkt_decode_etmv4i.cpp
[O26]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/opencsd/c_api/opencsd_c_api.h
[O27]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/common/trc_pkt_decode_base.h
[O28]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv4/trc_pkt_proc_etmv4i.cpp

[A10]: https://documentation-service.arm.com/static/60017fbb3f22832ff1d6872b
[A11]: https://documentation-service.arm.com/static/5f90158b4966cd7c95fd5b5e
[A12]: https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f

[E1]: https://gabi.xinuos.com/elf/07-pheader.html
[E2]: https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst
[E3]: https://github.com/serge1/ELFIO
[E4]: https://gabi.xinuos.com/elf/03-sheader.html
[E5]: https://github.com/serge1/ELFIO/blob/main/elfio/elfio_section.hpp
[E6]: https://developer.arm.com/documentation/100070/0612

[H1]: https://www.keil.com/support/docs/1584/default.htm
[H2]: https://onlinedocs.microchip.com/oxy/GUID-C3F66E96-7CDD-47A0-9AB7-9068BADB46C0-en-US-4/GUID-DF9E479D-6BA8-49E3-A2A5-997BBA49D34D.html

[L1]: https://github.com/serge1/ELFIO/blob/main/LICENSE.txt

[C4]: ../src/tracerun/CtraceRunMeta.cpp
[C10]: ../src/model/TraceSelection.h
[C11]: ../src/output/csv/CsvRowMapper.cpp
[C13]: ../src/tracerun/YmlTraceRunConfigReader.cpp
[C16]: ../src/tracerun/CtraceRunMeta.h

[O30]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/opencsd/c_api/ocsd_c_api_types.h
[O31]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_decode_etmv3.cpp

[ref-m7]: ../../../../TraceBufferExamples/ReferenceTraces/STM32H7B3I-DK-ETMv4/README.md
[ref-m4]: ../../../../TraceBufferExamples/ReferenceTraces/STM32H747I-DISCO-CM4-ETMv3/README.md
[ref-cancel]: ../../../../TraceBufferExamples/ReferenceTraces/OpenCSD-ETMv3-Cancellation/README.md
[ref-fields]: ../../../../TraceBufferExamples/ReferenceTraces/OpenCSD-ETMv3-PacketFields/README.md
