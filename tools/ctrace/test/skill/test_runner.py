# Copyright (c) 2026 Arm Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
"""Checks the evaluation kit's safety boundaries, not model behavior."""

import importlib.util
import json
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("skill_runner", Path(__file__).with_name("run.py"))
RUN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUN)


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def prepare(self, case="recording-choice"):
        result = RUN.prepare(self.root / "evaluation", case, sys.executable)
        manifest_path = Path(result["manifest"])
        return manifest_path, json.loads(manifest_path.read_text(encoding="utf-8"))

    def test_existing_workspace_is_refused_without_modification(self):
        output = self.root / "already-exists"
        output.mkdir()
        protected = output / "keep.txt"
        protected.write_bytes(b"existing user work")
        before = RUN.fingerprint(protected)
        with self.assertRaises(FileExistsError):
            RUN.prepare(output, "sleep", sys.executable)
        self.assertEqual(RUN.fingerprint(protected), before)
        self.assertEqual(list(output.iterdir()), [protected])

    def test_dangling_output_symlink_is_refused(self):
        output = self.root / "link"
        output.symlink_to(self.root / "missing", target_is_directory=True)
        with self.assertRaises(FileExistsError):
            RUN.fresh_root(output)
        self.assertFalse((self.root / "missing").exists())

    def test_preparation_preserves_source_fixtures_and_sentinels(self):
        fixture = RUN.CATALOG["fixtures"]["recordings"]
        before = {name: RUN.fingerprint(RUN.DATA / name) for name in fixture["files"].values()}
        manifest_path, manifest = self.prepare()
        self.assertEqual({name: RUN.fingerprint(RUN.DATA / name) for name in before}, before)
        self.assertEqual(RUN.originals(Path(manifest["trace"])), manifest["originals"])
        result = RUN.check_artifacts(manifest_path)
        self.assertEqual(result["status"], "artifact_checks_passed_semantic_review_required")
        self.assertEqual(result["recorded_conversions"], 0)

    def test_overwritten_automatic_csv_is_detected(self):
        manifest_path, manifest = self.prepare()
        csv_path = next(Path(manifest["trace"]).glob("*.csv"))
        csv_path.write_text("overwritten by a filtered conversion\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Original changed"):
            RUN.check_artifacts(manifest_path)

    def test_symlinked_job_is_rejected_even_with_a_recorded_conversion(self):
        manifest_path, manifest = self.prepare("sleep")
        leaf = Path(manifest["trace"]) / ".ctrace-ai" / manifest["fixture"]["set"] / "job"
        leaf.parent.mkdir(parents=True)
        outside = self.root / "outside"
        outside.mkdir()
        leaf.symlink_to(outside, target_is_directory=True)
        for number, args in enumerate((["--version"], ["--help"], [str(leaf), "--csv"])):
            RUN.write_json(manifest_path.parent / "calls" / f"{number}.json",
                           {"argv": [sys.executable, *args], "cwd": str(self.root), "started_ns": number,
                            "binary_sha256": manifest["tool"]["sha256"]})
        with self.assertRaisesRegex(ValueError, "Symlinked job"):
            RUN.check_artifacts(manifest_path)

    def test_parent_alias_and_native_option_equivalents_are_accepted(self):
        manifest_path, manifest = self.prepare("sleep")
        trace = Path(manifest["trace"])
        alias = self.root / "alias"
        alias.symlink_to(trace.parent, target_is_directory=True)
        manifest["trace"] = str(alias / ".trace")
        RUN.write_json(manifest_path, manifest)
        leaf = trace / ".ctrace-ai" / manifest["fixture"]["set"] / "job"
        leaf.mkdir(parents=True)
        raw = next(trace.glob("*.raw"))
        (leaf / raw.name).symlink_to(os.path.relpath(raw, leaf))
        shutil.copy2(next(trace.glob("*.yml")), leaf)
        (leaf / (raw.stem + ".csv")).write_text(
            "cycles,stream,type,source,value,pc,address,note\n"
            "1,,pcsample,,,0x08001234,,\n3,,pcsample,,,,,CPU Sleeping\n"
            "6,,pcsample,,,,,Trace prohibited\n10,,pcsample,,,0x08005678,,\n", encoding="utf-8")
        conversion = [str(alias / ".trace" / leaf.relative_to(trace)), "--stream", "0", "--type", "pcsample",
                      "-t", manifest["fixture"]["set"], "--csv"]
        for number, args in enumerate((["-V"], ["--help"], conversion)):
            RUN.write_json(manifest_path.parent / "calls" / f"{number}.json",
                           {"argv": [sys.executable, *args], "cwd": str(self.root), "started_ns": number,
                            "binary_sha256": manifest["tool"]["sha256"], "exit": 0})
        self.assertEqual(RUN.check_artifacts(manifest_path)["recorded_conversions"], 1)

    def test_wrapper_preserves_literal_arguments_and_child_exit(self):
        manifest_path, manifest = self.prepare("sleep")
        literal = "quoted ' argument; $(touch should-not-exist)"
        result = subprocess.run([manifest["wrapper"], "-c", "import sys; print(sys.argv[1]); sys.exit(7)", literal],
                                cwd=self.root, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 7)
        self.assertEqual(result.stdout, literal + "\n")
        self.assertFalse((self.root / "should-not-exist").exists())
        call = json.loads(next((manifest_path.parent / "calls").glob("*.json")).read_text(encoding="utf-8"))
        self.assertEqual(call["argv"][-1], literal)
        self.assertEqual(call["exit"], 7)

    def test_verbose_replay_preserves_actual_selection(self):
        manifest_path, manifest = self.prepare("error-diagnosis")
        manifest["tool"]["verbose"] = True
        RUN.write_json(manifest_path, manifest)
        trace = Path(manifest["trace"])
        raw = next(trace.glob("*.raw"))
        arguments = [["--version"], ["--help"]]
        for name, options in (("compact", ["--type", "error", "overflow", "--stream", "0"]),
                              ("detail", ["--verbose", "--type", "overflow", "error"])):
            leaf = trace / ".ctrace-ai" / manifest["fixture"]["set"] / name
            leaf.mkdir(parents=True)
            (leaf / raw.name).symlink_to(os.path.relpath(raw, leaf))
            shutil.copy2(next(trace.glob("*.yml")), leaf)
            (leaf / (raw.stem + ".csv")).write_text(
                "cycles,stream,type,source,value,pc,address,note\n"
                "0,,error,,,,,Invalid ITM packet sequence\n"
                "0,,error,,,,,116 raw bytes without usable ITM packets\n", encoding="utf-8")
            arguments.append([str(leaf), "--target", manifest["fixture"]["set"], "--csv", *options])
        for number, args in enumerate(arguments):
            RUN.write_json(manifest_path.parent / "calls" / f"{number}.json",
                           {"argv": [sys.executable, *args], "cwd": str(self.root), "started_ns": number,
                            "binary_sha256": manifest["tool"]["sha256"], "exit": 1 if number > 1 else 0})
        self.assertEqual(RUN.check_artifacts(manifest_path, followup=True)["recorded_conversions"], 2)
        replay_path = manifest_path.parent / "calls" / "3.json"
        replay = json.loads(replay_path.read_text(encoding="utf-8"))
        replay["argv"].remove("overflow")
        RUN.write_json(replay_path, replay)
        with self.assertRaisesRegex(ValueError, "Replay changed"):
            RUN.check_artifacts(manifest_path, followup=True)

    def test_source_and_index_headers_have_the_same_oracle_meaning(self):
        path = self.root / "output.csv"
        expected = None
        for column in ("source", "index"):
            path.write_text(f"cycles,stream,type,{column},value,pc,address,note\n"
                            '6,,dwt,2,,,,"line one\nline two"\n', encoding="utf-8")
            rows = RUN.read_csv(path)
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["source"], "2")
            if expected is not None:
                self.assertEqual(rows, expected)
            expected = rows

    def test_ambiguous_and_incomplete_csv_schemas_are_rejected(self):
        path = self.root / "output.csv"
        for content in (
            "cycles,stream,type,source,index,value,pc,address,note\n",
            "cycles,stream,type,source,value,pc,address,note\n6,,dwt,2\n",
            "cycles,stream,type,source,value,pc,address,note,note\n",
        ):
            path.write_text(content, encoding="utf-8")
            with self.assertRaises(ValueError):
                RUN.read_csv(path)


if __name__ == "__main__":
    unittest.main()
