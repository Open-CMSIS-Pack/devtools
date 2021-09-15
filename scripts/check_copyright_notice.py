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
import magic
from comment_parser import comment_parser

COPYRIGHT_TEXT = "Copyright (c) <ValidYear>"
LICENSE_TEXT = "SPDX-License-Identifier: Apache-2.0"

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

    mime_type = magic.from_file(filename, mime=True)
    if mime_type == "text/plain":
        mime_type = "text/x-c++"

    copyrightfound=False
    licensefound=False
    comments = ""
    for comment in comment_parser.extract_comments(filename,
                                                   mime=mime_type):
        comments += comment.text() + '\n'

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
