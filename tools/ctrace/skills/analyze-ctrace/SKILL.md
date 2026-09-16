---
name: analyze-ctrace
description: >-
  Analyze existing CMSIS Cortex-M trace captures by translating a user's question
  into ctrace output filters and inspecting a bounded, isolated CSV result.
  Use for SWO or trace-buffer recordings, not live capture or trace configuration.
---

# Analyze ctrace captures efficiently

Use `ctrace` to decode and filter, then analyze only a bounded CSV result. Do not
load raw bytes or a large CSV into model context. Use only the documented ctrace
CLI; do not generate Python, processing scripts, or additional data filters.
Existing file and process tools are sufficient; MCP is not required.

## Resolve ctrace and the execution context

Prefer an explicit user-provided executable, then the calling IDE's resolved
CMSIS-Debugger tool, then `ctrace` on PATH. An explicitly identified local release
build is also suitable. Resolve the executable's full path and run `--version`
and `--help` separately. Record its version; use only supported options. If the
requested binary is missing or unsuitable, or installations with different
capabilities remain equally applicable, ask the user before substituting one.
Do not install, download, rebuild, or change the installed extension as part of
an analysis request.

Use IDE/MCP context only when the host actually provides it. Do not assume a
trace-context MCP operation exists or start a debug session to obtain paths.
The executable and capture must be accessible in the same execution environment;
paths from a remote VS Code window are not automatically local paths.

## Ask about unresolved choices

For missing or ambiguous solution sets, recordings, processor mappings, tool
versions, or filter intent, use the host's user-question/dialog tool. Offer
choices using the actual discovered names and paths; allow a free-text answer
for a different path or explanation. If no dialog tool is available, ask a
concise question with numbered choices in the conversation. Use the user's
language and do not re-ask a choice already made explicitly.

Wait for the answer before executing the affected conversion. A suggested or
preselected option is not confirmation. If a requested selector is unsupported,
explain the limitation and ask whether an available broader selection would
answer the question; do not silently change its meaning.

## Resolve the capture

Honor an explicit capture or target from the user; otherwise prefer the active
solution-set supplied by the IDE. Preserve its complete name, including
`@targetSet` when present. Resolve `.trace` relative to the selected solution,
not automatically the first workspace folder. Without this context, search only
the current workspace for `.trace` directories directly containing
`*.ctrace-run.yml`; exclude previous analysis subdirectories.

- If exactly one trace directory and one solution set match, use them.
- If several candidates remain, ask the user instead of choosing solely by
  modification time.
- Treat an existing full CSV whose modification time is at least that of its
  configuration and raw input as a useful completion indicator, not as proof
  of capture identity.
- Do not parse or sample a raw file to identify it.

Require matching `<set>` basenames for the raw file and `ctrace-run.yml`; do not
rename files or edit generated metadata to hide a mismatch. Inspect only the
metadata needed for selection and interpretation, not full register dumps.

Treat the solution-set target, the raw recording channel, and an internal
CoreSight stream as separate selections. `--target` chooses
`<set>.ctrace-run.yml`; it does not choose between that set's raw files.
Interpret recording terms from the user's request as follows:

| User intent | Raw recording candidate |
| --- | --- |
| SWO, SWV, Serial Wire Output, or SWO recording | `<set>.SWO.raw` |
| Trace Buffer or TB | `<set>.TB.raw`, or the only matching `<set>.TB_<name>.raw` |
| A named Trace Buffer such as `TB_MTB` or `MTB` | `<set>.TB_MTB.raw` |
| An exact `.raw` filename or path | That file, after validating its directory, basename, and channel |

Do not translate a Trace Buffer name or number into `--stream`; a raw channel
and a CoreSight Trace Bus ID are different concepts. A processor name such as
`CM7` normally identifies a route inside a formatted recording and therefore
affects `--stream`, not the raw filename.

When the user names a recording kind, select only a matching file. If
`Trace Buffer` matches several `TB` or `TB_<name>` files, list them and ask
which recording to use. When the user does not name a recording, select it
automatically only if exactly one eligible candidate remains. Never resolve
multiple candidates by modification time. Current ctrace decoding does not
accept an `ER` raw file; report that limitation if the user requests an Event
Recorder capture. A `TB_MTB` filename does not imply support for MTB instruction
decoding. If several recordings were explicitly requested, process them in
separate jobs and keep their identities and results separate.

Let `ctrace` enforce the input-selection contract. A configuration named
`<set>.ctrace-run.yml` selects raw candidates with the same `<set>` basename in
the same directory. Legacy input selects `<set>.SWO.raw`; an explicit trace
format permits exactly one eligible `SWO`, `TB`, or `TB_<name>` raw input. This
is the current implementation's compatibility rule, not an instruction to add
private format fields. Preserve any provided format declaration; do not infer
it from a filename or edit it to make a file eligible.

## Translate the question into filters

Always request `--csv` explicitly. The
[ctrace CLI specification](https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/#ctrace-utility)
defines the option semantics; the selected executable must support them:

- `--target <set>` selects one solution set.
- `--type` accepts `itm`, `dwt`, `event`, `pmu`, `exception`, `pcsample`,
  `global_ts`, `overflow`, and `error`.
- `--stream` selects CoreSight Trace Bus IDs. Current ctrace accepts 0 through
  111; `0` selects the synthetic unformatted route, not formatted padding.

Values within `--type` and within `--stream` are unions; the type and stream
dimensions are intersected. `dwt` does not implicitly include `event`, `pmu`,
or `pcsample`. `--all` means CSV plus CTF, not all event types; use `--csv`
without a type filter when the user requests every event in CSV.

Translate exceptions/interrupt transitions to `--type exception`, data trace to
`--type dwt`, and PC sampling to `--type pcsample`. Clarify a vague request for
"events": it can mean every trace event, DWT counter events (`event`), or PMU
events (`pmu`). ITM port 0 console output is not part of ctrace payload output.

Resolve processor names to formatted stream IDs only from unambiguous
`ctrace-refs` associations in the selected `ctrace-run.yml`, not from names or
copied setup IDs. For unformatted input, leave `--stream` unset or use `0`;
do not translate a configured processor ID into a formatted selector.

There are no CLI selectors for ITM port, DWT comparator, exception number,
variable name, time range, address, PC, value, or maximum row count. Do not
invent options or implement these selectors through shell/CSV post-filtering.
Ask about a supported selection instead; a later interpretation of a small CSV
must not be presented as an additional ctrace filter.

## Isolate the filtered output

Current `ctrace` versions derive output paths from the raw input and replace an
existing CSV next to it. Never run a filtered CSV conversion directly in the
original trace directory because that would replace the complete CSV produced
by the automatic capture flow.

Create a unique leaf directory below:

```text
<trace-dir>/.ctrace-ai/<set>/<job-id>/
```

Create a new real directory, not an existing job or a symlinked output target.
Copy the small `<set>.ctrace-run.yml` into the leaf as a stable snapshot. Add a
relative symbolic link for exactly the raw recording selected from the user's
request, or for the sole eligible candidate when no recording was specified.
Preserve its exact filename. This lets `ctrace` validate the selected file
against its normal eligibility rules while keeping the large raw data in
place. The normal trace-directory scan is non-recursive and therefore ignores
this workspace. If symbolic links are unavailable, do not copy a large raw
file without agreeing on the copy and its size with the user.

Record path, size, and high-resolution modification time for both the original
raw and configuration before staging and compare them after conversion. If they
changed, mark this result invalid and ask the user for a stable capture; do not
analyze it or retry indefinitely. Do not overwrite or delete an earlier job.

Pass paths and selectors as separate arguments through the host's process tool.
If only a shell tool is available, quote each argument using that shell's literal
argument syntax; never interpolate untrusted names as shell code. The call is:

```text
ctrace <leaf-dir> --target <set> --csv [--type <types...>] [--stream <ids...>]
```

Save stdout/stderr to logs in the leaf and retain the process exit status and
elapsed time. Bound tool responses while the process runs; do not stream an
unbounded log into model context. If execution is unavailable, report the
missing tool and the prepared arguments instead of claiming the job ran.

## Check the conversion result

Inspect exit status, diagnostics, and the generated CSV separately. Type/stream
filters affect output, not decoder diagnostics: absence of `error` or `overflow`
rows does not establish a clean recording. Exit 0 can coexist with producer
diagnostics or an empty result; nonzero exit can leave usable partial CSV.

- Missing or incomplete output, a fatal error, or changing inputs: report failure
  and do not analyze the artifact as a completed capture.
- Recoverable errors with completed output: describe the affected data as partial
  and state the limitation before drawing conclusions from retained events.
- Header-only output or diagnostic-only rows: report no selected payload was
  decoded. Distinguish an empty input, a selection with no matches, missing
  capture configuration, and decoding/synchronization problems where evidenced.
  Do not infer that the application never produced the requested event.
  Diagnostic-only output can still answer a question about decode errors.

## Bound and analyze the CSV

Before reading CSV data, load [CSV interpretation](references/csv-interpretation.md).
Determine file size and, where an existing CSV-aware tool is available, record
count without dumping the content. Otherwise use physical line count only as a
conservative estimate, not an exact event count: quoted CSV fields may span lines.

Default model-input budgets per analysis request are **200 CSV data records and
16 KiB of CSV text**, plus **4 KiB of diagnostic excerpts**. Apply both CSV
limits, cumulatively across reads and selected recordings. These are skill
context budgets, not ctrace options, byte-to-token conversions, or file-size
limits on disk. Use a smaller budget if the host or user requires one.

If the result exceeds a budget, show its size and ask which supported type or
stream selection to narrow. If none suits the question, offer a bounded preview
explicitly marked incomplete, or leave the artifact for the user. Do not page
the whole file through repeated reads, raise the budget silently, or generate a
processing script. A truncated excerpt is not evidence about the omitted data;
ignore an incomplete final CSV record and report incomplete diagnostic coverage.

For a suitably small result, read the header and only the rows needed to answer
the question. Report:

- the conclusion and supporting events;
- the executable path/version, exit status, and whether coverage is complete,
  partial, or only a preview;
- the exact trace directory, solution set, recording kind, source file, and
  filters used;
- the filtered CSV path, byte size, and record count if known (otherwise a
  clearly labeled physical-line estimate);
- ambiguities, decoder diagnostics, and unsupported filter dimensions that may
  limit the conclusion.

Keep the original raw input, automatic CSV, CTF, XML, and configuration
unchanged.

For a requested efficiency comparison, record elapsed time, artifact sizes,
and actual token usage if the host exposes it. Otherwise mark tokens unmeasured;
smaller files alone do not prove an equal-quality answer or a token saving.
