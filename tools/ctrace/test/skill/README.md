<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# analyze-ctrace evaluation kit

This kit prepares repeatable tasks from the existing approved or synthetic
[ctrace fixtures](../data/README.md). It checks decoder facts and protects original
artifacts. It does **not** run a model, grade arbitrary prose, or claim that a
passing fixture test proves the skill works.

Python 3 and a supplied ctrace executable are sufficient. No packages, model API,
credentials, downloads or network access are required. The optional assistant
evaluation uses a host and model selected by the evaluator.

## Check the kit and fixture contracts

From the repository root:

```sh
python3 -m unittest discover -s tools/ctrace/test/skill -p test_runner.py -v

python3 tools/ctrace/test/skill/run.py check-fixtures \
  --ctrace /absolute/path/to/ctrace \
  --output /tmp/ctrace-skill-contracts-001
```

Output roots must not exist, including empty directories and dangling symlinks.
Choose a new name for each run; the kit never resets an earlier workspace. Keep
the output if a check fails: logs and generated CSVs explain the failing oracle.

`check-fixtures` runs nine conversions for eight scenarios, including both
candidate recordings in the ambiguous-recording case. It checks exit status,
native selections and known semantic rows with Python's CSV parser. It checks
four PC/sleep/prohibited records, four value-less DWT matches, CM7 tick values
4801 through 4826, 84 CM4 exception transitions, a 249-transition selection that
exceeds the skill's record budget, and two reset-related decode-error records.
The report status is `fixture_contracts_passed_not_skill_evaluated`.

The checked-in YAML fixtures use `source`; some supplied executables require
`index`. For those executables add `--fixture-index-schema` to `check-fixtures`
and `prepare`. This explicit test-data adaptation changes only the staged YAML
copies and is recorded in the evaluator manifest. It is not behavior that the
analysis skill should perform. The CSV oracle accepts either `source` or `index`
as the comparator/port/exception column and rejects ambiguous schemas. Fixture
integrity remains covered by the existing `CtraceFixtureIntegrity` CTest.

## Evaluate behavior after skill selection

```sh
python3 tools/ctrace/test/skill/run.py prepare \
  --case cm7-tick --variant 0 \
  --ctrace /absolute/path/to/ctrace \
  --output /tmp/ctrace-skill-cm7-001
```

The command returns these paths:

- `prompt.txt`: complete user prompt that explicitly selects `analyze-ctrace`.
- `workspace/`: disposable capture workspace and recording ctrace wrapper.
- `evaluator/manifest.json`: evaluator-only case, rubric, original-file hashes,
  byte sizes and nanosecond timestamps, binary version/hash, skill file hashes,
  repository revision, preparation time, and kit hashes.
- `evaluator/calls/`: actual ctrace arguments, cwd, status, elapsed time, binary
  hash, stdout and stderr for every wrapper invocation.
- `user-prompt.txt` and `skill-catalog.txt`: inputs for the separate discovery
  evaluation described below.

Start a fresh assistant session with the normal host tools and the current skill.
Use `workspace/` as its working directory and send `prompt.txt`. Do not provide
`scenarios.json`, the evaluator directory or its expected answers to the agent.
Select the paraphrase with `--variant 1` in a new prepared workspace. Save the
actual conversation and host tool transcript, including model/reasoning settings
and host version, alongside the evaluator artifacts.

Original CSV/CTF/XML files are deliberate preservation sentinels, not decoder
goldens. The assistant must create its own isolated analysis job. The wrapper
forwards the supplied binary's arguments, cwd, output and exit status, while
saving an independent execution record. It buffers output on disk until the
child exits. It does not hide options, invent verbose support, or change input
data. Executable symlink handling requires a POSIX host; the kit does not install
anything to add that capability.

After the agent finishes:

```sh
python3 tools/ctrace/test/skill/run.py check-artifacts \
  --manifest /tmp/ctrace-skill-cm7-001/evaluator/manifest.json
```

The checks cover original-file content/stat preservation, fresh real analysis
leaves, copied configuration snapshots, relative links to exactly one selected
RAW file, recorded executable identity, CLI selections and resulting CSV facts.
Equivalent target aliases, union value order and unformatted stream `0` are
accepted. Additional error/overflow selections are identified for semantic
review; they do not establish completeness by themselves. The status remains
`artifact_checks_passed_semantic_review_required`.

Finish by reviewing the actual conversation against that case's `rubric` in
`scenarios.json`. Record pass/fail and concrete transcript evidence for each
criterion. Review factual claims, metadata attribution, uncertainty, missing
values, local clocks, and whether a natural question was answered without an
unnecessary filter-approval step. Record unexercised capabilities as not run.

## Clarification and diagnostic turns

`recording-choice` and `event-choice` end at the required question. The artifact
checker requires no conversion; only the semantic review can establish that the
question was asked and offered the appropriate choices. These scenarios do not
score the subsequent conversion after a user answer.

For `error-diagnosis`, first send its initial prompt and run `check-artifacts`.
It must use compact diagnostics. Then send that case's `followup` in the **same**
assistant session and check the cumulative result with `--followup`:

```sh
python3 tools/ctrace/test/skill/run.py check-artifacts \
  --manifest /tmp/ctrace-skill-errors-001/evaluator/manifest.json --followup
```

The follow-up may be answered from existing evidence. If more detail is needed,
at most one verbose replay is allowed, using the same selection in a new job,
and only when the supplied binary really advertises `--verbose`. Run this case
with both a capable executable and an explicitly supplied executable without
that capability. Missing support must be explained without installing, building,
substituting a binary or inventing diagnostic detail. The fixture-only command
does not test this conversational behavior.

## Evaluate automatic skill discovery separately

The explicit `prompt.txt` runs test behavior **after** skill selection. They do
not test whether natural intent causes the host to select this skill.

For a discovery pass, start another fresh session with the normal host skill
catalog. Add only the `name` and `description` from `skill-catalog.txt` to that
catalog; do not preload this skill's body or evaluator rubrics. Send
`user-prompt.txt`, which contains no skill invocation. Record whether the host
selects `analyze-ctrace` before loading its body, then optionally continue the
behavior evaluation. Keep discovery and execution verdicts separate. A host
that cannot expose or record its selection step leaves discovery unverified.

## Observation limits and integration

The wrapper is an execution recorder, not a security sandbox. It cannot observe
other processes, a direct call bypassing the wrapper, raw-file reads, total CSV
or diagnostic bytes shown to the model, clarification wording, or final-answer
semantics. It cannot prove read-budget compliance. Inspect the full host tool
transcript for those properties; if unavailable, mark them unverified. The
evaluator directory is a logical information boundary, not filesystem isolation.

The self-tests verify meaningful kit boundaries, including refusal to reuse an
output directory, preservation and overwrite detection, path-alias handling,
rejection of symlinked jobs, literal argument forwarding, and CSV schema/record
handling. Synthetic call records in those self-tests validate the checker only;
they are not model evaluations. No tests match keywords in the skill prose.

Run deterministic checks in native CI when a compatible ctrace binary is
available. Keep actual model runs optional and retain their evidence. This kit
adds no CMake target or workflow and does not change the restricted ctrace
workflow path filters.
