#!/usr/bin/env python3
# Copyright (c) 2026 Arm Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
"""Fixture and artifact checks; this program does not run or grade an LLM."""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "data"
CATALOG = json.loads((HERE / "scenarios.json").read_text(encoding="utf-8"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def fresh_root(path):
    path = Path(os.path.abspath(path))
    path.mkdir(parents=True, exist_ok=False)  # Also refuses dangling symlinks.
    return path.resolve(strict=True)


def fingerprint(path):
    require(path.is_file() and not path.is_symlink(), f"Expected regular file: {path}")
    stat = path.stat()
    return {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "size": stat.st_size, "mtime_ns": stat.st_mtime_ns}


def originals(trace):
    return {str(p.relative_to(trace)): fingerprint(p)
            for p in sorted(trace.rglob("*")) if p.is_file()}


def binary_info(executable):
    executable = Path(executable).resolve(strict=True)
    version = subprocess.run([str(executable), "--version"], capture_output=True, text=True, check=True)
    help_result = subprocess.run([str(executable), "--help"], capture_output=True, text=True, check=True)
    return {"executable": str(executable), "version": version.stdout.strip(), "sha256": fingerprint(executable)["sha256"],
            "verbose": "--verbose" in help_result.stdout}


def provenance():
    skill = HERE.parents[1] / "skills" / "analyze-ctrace"
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=HERE, capture_output=True, text=True) if shutil.which("git") else None
    return {"created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "repository_revision": revision.stdout.strip() if revision and revision.returncode == 0 else None,
            "skill_files": {str(p.relative_to(skill)): fingerprint(p)["sha256"] for p in sorted(skill.rglob("*.md"))},
            "runner_sha256": fingerprint(HERE / "run.py")["sha256"],
            "scenarios_sha256": fingerprint(HERE / "scenarios.json")["sha256"]}


def stage(trace, fixture, sentinels=True, index_schema=False):
    trace.mkdir(parents=True)
    for name, source in fixture["files"].items():
        shutil.copy2(DATA / source, trace / name)
        if index_schema and name.endswith(".yml"):
            config = trace / name
            config.write_text(re.sub(r"(?m)^(\s*)source:", r"\1index:", config.read_text(encoding="utf-8")), encoding="utf-8")
    if sentinels:
        for name in fixture["files"]:
            if name.endswith(".raw"):
                base = name[:-4]
                (trace / (base + ".csv")).write_text("Preserve automatic CSV\n", encoding="utf-8")
                ctf = trace / (base + ".ctf")
                ctf.mkdir()
                (ctf / "metadata").write_text("Preserve CTF metadata\n", encoding="utf-8")
                (ctf / "stream_0").write_bytes(b"Preserve CTF stream\n")
        (trace / (fixture["set"] + ".traceanalysis.xml")).write_text("<preserve/>\n", encoding="utf-8")


def prepare(output, case_id, executable, variant=0, index_schema=False):
    case = next(c for c in CATALOG["cases"] if c["id"] == case_id)
    require(0 <= variant < len(case["prompts"]), "Prompt variant is out of range")
    tool = binary_info(executable)
    root = fresh_root(output)
    trace = root / "workspace" / "demo" / ".trace"
    fixture = CATALOG["fixtures"][case["fixture"]]
    stage(trace, fixture, index_schema=index_schema)
    evaluator = root / "evaluator"
    (evaluator / "calls").mkdir(parents=True)
    manifest_path = evaluator / "manifest.json"
    wrapper = root / "workspace" / "bin" / "ctrace"
    wrapper.parent.mkdir()
    wrapper.write_text("#!/usr/bin/env python3\nimport runpy, sys\n"
                       f"sys.argv[1:1] = {['_record', str(manifest_path)]!r}\n"
                       f"runpy.run_path({str(HERE / 'run.py')!r}, run_name='__main__')\n", encoding="utf-8")
    wrapper.chmod(0o755)
    manifest = {"case": case, "fixture": fixture, "trace": str(trace), "tool": tool, "fixture_index_schema": index_schema,
                "provenance": provenance(),
                "wrapper": str(wrapper), "originals": originals(trace)}
    write_json(manifest_path, manifest)
    prompt = (f"Arbeitsverzeichnis: {root / 'workspace'}\nVerwende dieses vorhandene ctrace: {wrapper}\n"
              f"Aufnahmeverzeichnis: {trace}\n\n" + case["prompts"][variant] + "\n")
    (root / "prompt.txt").write_text("Verwende den Skill analyze-ctrace.\n" + prompt, encoding="utf-8")
    (root / "user-prompt.txt").write_text(prompt, encoding="utf-8")
    frontmatter = (HERE.parents[1] / "skills" / "analyze-ctrace" / "SKILL.md").read_text(encoding="utf-8").split("---", 2)[1]
    (root / "skill-catalog.txt").write_text(frontmatter.strip() + "\n", encoding="utf-8")
    return {"status": "prepared_not_evaluated", "prompt": str(root / "prompt.txt"),
            "user_prompt": str(root / "user-prompt.txt"), "skill_catalog": str(root / "skill-catalog.txt"),
            "workspace": str(root / "workspace"), "manifest": str(manifest_path)}


def record(manifest_path, arguments):
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    prefix = manifest_path.parent / "calls" / uuid.uuid4().hex
    stdout, stderr = prefix.with_suffix(".stdout"), prefix.with_suffix(".stderr")
    argv = [manifest["tool"]["executable"], *arguments]
    binary_hash = fingerprint(Path(argv[0]))["sha256"]
    started, monotonic = time.time_ns(), time.monotonic()
    with stdout.open("wb") as out, stderr.open("wb") as err:
        result = subprocess.run(argv, stdout=out, stderr=err, check=False)
    write_json(prefix.with_suffix(".json"), {"argv": argv, "cwd": os.getcwd(), "started_ns": started,
               "elapsed_s": time.monotonic() - monotonic, "exit": result.returncode,
               "binary_sha256": binary_hash, "stdout": str(stdout), "stderr": str(stderr)})
    for path, stream in ((stdout, sys.stdout.buffer), (stderr, sys.stderr.buffer)):
        with path.open("rb") as source:
            shutil.copyfileobj(source, stream)
        stream.flush()
    if result.returncode < 0:
        os.kill(os.getpid(), -result.returncode)
    return result.returncode


def read_csv(path):
    with Path(path).open(newline="", encoding="utf-8-sig") as source:
        reader = csv.DictReader(source)
        fields = reader.fieldnames or []
        require(len(fields) == len(set(fields)), "Duplicate CSV columns")
        index = "source" if "source" in fields else "index"
        require(set(fields) == {"cycles", "stream", "type", index, "value", "pc", "address", "note"},
                f"Unsupported CSV schema: {fields}")
        rows = []
        for row in reader:
            require(None not in row and all(v is not None for v in row.values()), "Incomplete CSV record")
            if index == "index":
                row["source"] = row.pop("index")
            rows.append(row)
        return rows


def selection(native):
    return [part for option, key in (("--type", "types"), ("--stream", "streams"))
            if native[key] for part in [option, *native[key]]]


def assert_oracle(case, csv_path, exit_code, extra_types=()):
    rows = [row for row in read_csv(csv_path) if row["type"] not in ("info", *extra_types)]
    oracle = case["oracle"]
    require(exit_code == oracle["exit"], f"{case['id']}: exit {exit_code}")
    require(len(rows) == oracle["rows"], f"{case['id']}: {len(rows)} payload/diagnostic rows")
    native = case["native"]
    require(not native["types"] or all(r["type"] in native["types"] for r in rows), "Unexpected row type")
    require(not native["streams"] or all(r["stream"] in native["streams"] for r in rows), "Unexpected stream")
    if oracle["kind"] in ("sleep", "match"):
        require([r["cycles"] for r in rows] == ["1", "3", "6", "10"], "Unexpected local cycles")
    if oracle["kind"] == "sleep":
        require([r["pc"] for r in rows] == ["0x08001234", "", "", "0x08005678"], "Unexpected PCs")
        require([rows[1]["note"], rows[2]["note"]] == ["CPU Sleeping", "Trace prohibited"], "Unexpected markers")
    if oracle["kind"] == "match":
        require([r["source"] for r in rows] == ["0", "1", "2", "3"], "Unexpected comparators")
        require(all(not r[k] for r in rows for k in ("value", "pc", "address")), "Match invented a value")
    if oracle["kind"] == "tick":
        require(all(r["source"] == "0" for r in rows), "Unexpected tick comparator")
        require([int(r["value"], 16) for r in rows] == list(range(4801, 4827)), "Unexpected captured tick values")
    return len(rows)


def check_fixtures(output, executable, index_schema=False):
    tool = binary_info(executable)
    root, results = fresh_root(output), []
    for case in CATALOG["cases"]:
        fixture = CATALOG["fixtures"][case["fixture"]]
        trace = root / case["id"] / ".trace"
        stage(trace, fixture, sentinels=False, index_schema=index_schema)
        for raw in sorted(trace.glob("*.raw")):
            leaf = root / case["id"] / raw.stem
            leaf.mkdir()
            shutil.copy2(trace / (fixture["set"] + ".ctrace-run.yml"), leaf)
            (leaf / raw.name).symlink_to(os.path.relpath(raw, leaf))
            argv = [tool["executable"], str(leaf), "--target", fixture["set"], "--csv", *selection(case["native"])]
            with (leaf / "stdout.log").open("wb") as out, (leaf / "stderr.log").open("wb") as err:
                result = subprocess.run(argv, stdout=out, stderr=err, check=False)
            count = assert_oracle(case, leaf / (raw.stem + ".csv"), result.returncode)
            results.append({"case": case["id"], "raw": raw.name, "rows": count, "exit": result.returncode})
    report = {"status": "fixture_contracts_passed_not_skill_evaluated", "tool": tool, "provenance": provenance(),
              "fixture_index_schema": index_schema, "cases": results}
    write_json(root / "report.json", report)
    return report


def check_artifacts(manifest_path, followup=False):
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    original_trace = Path(manifest["trace"])
    require(original_trace.is_dir() and not original_trace.is_symlink(), "Original trace directory was replaced")
    trace, fixture, case = original_trace.resolve(), manifest["fixture"], manifest["case"]
    for relative, expected in manifest["originals"].items():
        require(fingerprint(trace / relative) == expected, f"Original changed: {relative}")
    for path in trace.rglob("*"):
        relative = path.relative_to(trace)
        if relative.parts[0] != ".ctrace-ai" and (path.is_file() or path.is_symlink()):
            require(str(relative) in manifest["originals"], f"Unexpected original-directory artifact: {relative}")
    calls = sorted((json.loads(p.read_text(encoding="utf-8"))
                    for p in (manifest_path.parent / "calls").glob("*.json")), key=lambda c: c["started_ns"])
    conversions, seen, probes, review = [], set(), set(), []
    original_selection = None
    for call in calls:
        argv = call["argv"][1:]
        if manifest["tool"].get("sha256"):
            require(call.get("binary_sha256") == manifest["tool"]["sha256"], "Executed binary changed")
        if argv in (["--version"], ["-V"], ["--help"]):
            probes.add("--version" if argv == ["-V"] else argv[0])
            continue
        require(probes == {"--version", "--help"}, "Conversion before separate version/help probes")
        require(argv and not argv[0].startswith("-"), "Trace directory must precede multi-value options")
        leaf = Path(call["cwd"]) / argv[0]
        require(all(p.is_dir() and not p.is_symlink() for p in (leaf, leaf.parent, leaf.parent.parent)), "Symlinked job")
        leaf = leaf.resolve()
        require(leaf not in seen, f"Job reused: {leaf}")
        seen.add(leaf)
        require(leaf.parent == trace / ".ctrace-ai" / fixture["set"], f"Conversion outside isolated leaf: {leaf}")
        config = leaf / (fixture["set"] + ".ctrace-run.yml")
        require(fingerprint(config)["sha256"] == manifest["originals"][config.name]["sha256"], "Changed configuration snapshot")
        raws = list(leaf.glob("*.raw"))
        require(len(raws) == 1 and raws[0].is_symlink(), "Job must link exactly one recording")
        require(raws[0].name in fixture["files"] and not os.path.isabs(os.readlink(raws[0]))
                and raws[0].resolve() == trace / raws[0].name, "Wrong or non-relative RAW link")
        verbose = "--verbose" in argv or "-v" in argv
        expected = [str(leaf), "--target", fixture["set"], "--csv", *selection(case["native"])]
        # Options may be reordered; multi-value groups must preserve their meaning.
        parsed, current = {}, None
        for arg in argv[1:]:
            if arg.startswith("-"):
                current = {"-v": "--verbose", "-t": "--target"}.get(arg, arg)
                require(current not in parsed, f"Repeated option: {current}")
                parsed[current] = []
            else:
                require(current is not None, "Unexpected positional argument")
                parsed[current].append(arg)
        wanted = {"--target": [fixture["set"]], "--csv": []}
        for option, key in (("--type", "types"), ("--stream", "streams")):
            if case["native"][key]:
                wanted[option] = case["native"][key]
        extra_types = set(parsed.get("--type", [])) - set(case["native"]["types"])
        if extra_types and extra_types <= {"error", "overflow"} and case["native"]["types"]:
            wanted["--type"] = [*case["native"]["types"], *extra_types]
            review.append(f"Review expanded diagnostic selection {sorted(extra_types)} in {leaf}")
        if case["fixture"] in ("sleep", "match", "reset") and parsed.get("--stream") == ["0"]:
            wanted["--stream"] = ["0"]
        if case["id"] == "budget" and sorted(parsed.get("--stream", [])) == ["1", "2"]:
            wanted["--stream"] = ["1", "2"]
        if verbose:
            wanted["--verbose"] = []
        require({k: sorted(v) for k, v in parsed.items()} == {k: sorted(v) for k, v in wanted.items()},
                f"Unexpected native selection: {argv}; expected {expected}")
        streams = frozenset(parsed.get("--stream", []))
        if case["fixture"] in ("sleep", "match", "reset") and streams == {"0"}:
            streams = frozenset()
        actual_selection = (raws[0].name, frozenset(parsed.get("--type", [])), streams)
        if original_selection is None:
            original_selection = actual_selection
        require(actual_selection == original_selection, "Replay changed the original capture or selection")
        require(call["exit"] == case["oracle"]["exit"], "Unexpected decoder exit")
        fingerprint(leaf / (raws[0].stem + ".csv"))
        assert_oracle(case, leaf / (raws[0].stem + ".csv"), call["exit"], extra_types)
        conversions.append(verbose)
    ai = trace / ".ctrace-ai"
    for path in ai.rglob("*") if ai.exists() else []:
        require(not path.is_symlink() or (path.parent in seen and path.suffix == ".raw"), f"Unexpected analysis symlink: {path}")
        if path.is_dir() and len(path.relative_to(ai).parts) == 2:
            require(path in seen, f"Unrecorded analysis job: {path}")
    require(not case.get("clarify") or not conversions, "Conversion before required clarification")
    if not case.get("clarify"):
        require(conversions, "No conversion recorded; skill behavior was not evaluated")
        allow_replay = followup and case["id"] == "error-diagnosis" and manifest["tool"]["verbose"]
        require(conversions == [False] or (allow_replay and conversions == [False, True]), "Unexpected verbose/replay behavior")
    return {"status": "artifact_checks_passed_semantic_review_required", "recorded_conversions": len(conversions),
            "selection_review": review,
            "unverified": ["other processes and file reads", "read budgets", "clarification wording", "final-answer semantics"]}


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "_record":
        return record(sys.argv[2], sys.argv[3:])
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prep = commands.add_parser("prepare")
    prep.add_argument("--case", choices=[c["id"] for c in CATALOG["cases"]], required=True)
    prep.add_argument("--variant", type=int, default=0)
    fixtures = commands.add_parser("check-fixtures")
    for command in (prep, fixtures):
        command.add_argument("--output", type=Path, required=True)
        command.add_argument("--ctrace", required=True)
        command.add_argument("--fixture-index-schema", action="store_true", help="Adapt staged test YAML source keys to index keys")
    artifacts = commands.add_parser("check-artifacts")
    artifacts.add_argument("--manifest", type=Path, required=True)
    artifacts.add_argument("--followup", action="store_true")
    args = parser.parse_args()
    if args.command == "prepare":
        result = prepare(args.output, args.case, args.ctrace, args.variant, args.fixture_index_schema)
    elif args.command == "check-fixtures":
        result = check_fixtures(args.output, args.ctrace, args.fixture_index_schema)
    else:
        result = check_artifacts(args.manifest, args.followup)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"check failed: {error}", file=sys.stderr)
        sys.exit(1)
