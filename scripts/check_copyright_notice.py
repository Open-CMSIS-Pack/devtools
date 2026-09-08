# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

"""
Checks the presence of copyright notice in the files
"""

from typing import Optional, Sequence
import argparse
import os
import sys
import re

COPYRIGHT_TEXT = "Copyright (c) <ValidYear>"
LICENSE_TEXT = "SPDX-License-Identifier: Apache-2.0"

def extract_header(source: str, is_shell: bool) -> str:
    """Return leading blank and comment lines, stopping at the first code line."""
    header = []
    in_block_comment = False
    include_guard = None

    for line in source.splitlines(keepends=True):
        stripped = line.strip()

        if in_block_comment:
            header.append(line)
            if "*/" in stripped:
                in_block_comment = False
                if stripped.split("*/", 1)[1].strip():
                    break
            continue

        if not stripped:
            header.append(line)
            continue

        if is_shell and stripped.startswith("#"):
            header.append(line)
            continue

        if not is_shell and stripped.startswith("//"):
            header.append(line)
            continue

        if not is_shell and include_guard is None:
            guard_match = re.fullmatch(r"#ifndef\s+([A-Za-z_]\w*)", stripped)
            if guard_match:
                include_guard = guard_match.group(1)
                header.append(line)
                continue

        if not is_shell and include_guard:
            define_match = re.fullmatch(r"#define\s+([A-Za-z_]\w*)", stripped)
            if define_match and define_match.group(1) == include_guard:
                include_guard = ""
                header.append(line)
                continue

        if not is_shell and stripped.startswith("/*"):
            header.append(line)
            if "*/" not in stripped[2:]:
                in_block_comment = True
            elif stripped.split("*/", 1)[1].strip():
                break
            continue

        break

    return "".join(header)

def check_file(filename: str, copyright_reg_exp: re.Pattern) -> int:
    """
    Checks a file for the presence of a comment in the form of a copyright
    and license notice.
    @param filename: The name of the file to check.
    @param copyright_reg_exp  A regular expression giving the format of the
    copyright notice (exclusing language-specific comment chars).
    @return 0 If the copyright & license notice are found, otherwise 1.
    """
    if os.path.getsize(filename) == 0:
        return 0

    copyrightfound=False
    licensefound=False
    with open(filename, encoding="utf-8-sig", errors="replace") as source_file:
        source = source_file.read()
    is_shell = source.startswith("#!") or filename.endswith((".sh", ".bash"))
    comments = extract_header(source, is_shell)

    if copyright_reg_exp.search(comments):
        copyrightfound=True
    if comments.find(LICENSE_TEXT) != -1:
        licensefound=True

    if copyrightfound and licensefound:
        return 0

    errstr = ""
    if not copyrightfound:
        errstr = "\n\t # Missing or invalid copyright text. Please follow format: " + COPYRIGHT_TEXT
    if not licensefound:
        errstr += "\n\t # Missing or invalid license text. Please write : " + LICENSE_TEXT

    print(f"# Copyright check error(s) in : {filename} {errstr}")
    return 1

def main(argv: Optional[Sequence[str]] = None) -> int:
    """
    Entry point that checks for copyright notices being present in all the
    files supplied on the command-line.
    @param argv: The names of the files to check.
    @return Non-zero if one or more of the passed files was missing a copyright
    notice.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('filenames', nargs='*')
    args = parser.parse_args(argv)

    print("Checking copyright headers...")
    ret = 0
    copyright_reg_exp=re.compile(r"(Copyright\s\(c\)\s(19|20)[0-9][0-9][^0-9])")
    for filename in args.filenames:
        ret |= check_file(filename, copyright_reg_exp)

    if ret != 0:
        print(">> error: Files are missing a valid copyright header")

    return ret

if __name__ == '__main__':
    sys.exit(main())
