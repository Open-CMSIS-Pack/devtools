#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2022 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# DEPRECATED
# This bash script wraps the cbuild binary for backward
# compatibility. It will be removed in future versions.

for cmdline in "$@"
do
  arg="${cmdline/--cmake/-g=Ninja}"
  arg="${arg/database/-t=database}"
  args+=("${arg}")
done
cbuild "${args[@]}"
