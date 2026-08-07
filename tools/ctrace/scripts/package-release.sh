#!/usr/bin/env bash

# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd -- "${script_dir}/../../.." && pwd)
distribution_dir=${1:-"${source_dir}/tools/ctrace/distribution"}

expected_binaries=(
  bin/darwin-arm64/ctrace
  bin/linux-amd64/ctrace
  bin/linux-arm64/ctrace
  bin/windows-amd64/ctrace.exe
  bin/windows-arm64/ctrace.exe
)

mkdir -p "${distribution_dir}/THIRD_PARTY_LICENSES"
cp "${source_dir}/LICENSE" "${distribution_dir}/LICENSE.txt"
cp "${source_dir}/tools/ctrace/docs/THIRD_PARTY_NOTICES.md" "${distribution_dir}/"
cp "${source_dir}/external/cxxopts/LICENSE" "${distribution_dir}/THIRD_PARTY_LICENSES/cxxopts.txt"
cp "${source_dir}/external/yaml-cpp/LICENSE" "${distribution_dir}/THIRD_PARTY_LICENSES/yaml-cpp.txt"
cp "${source_dir}/external/OpenCSD/LICENSE" "${distribution_dir}/THIRD_PARTY_LICENSES/OpenCSD.txt"
cp "${source_dir}/tools/ctrace/docs/OpenCSD-NOTICE.txt" "${distribution_dir}/THIRD_PARTY_LICENSES/"

for binary in "${expected_binaries[@]}"; do
  if [[ ! -f "${distribution_dir}/${binary}" ]]; then
    echo "Missing ctrace release binary: ${binary}" >&2
    exit 1
  fi
done

expected_inventory=$(printf '%s\n' "${expected_binaries[@]}")
actual_inventory=$(cd "${distribution_dir}" && find bin -type f | LC_ALL=C sort)
if [[ "${actual_inventory}" != "${expected_inventory}" ]]; then
  echo "Unexpected ctrace release binary inventory:" >&2
  printf '%s\n' "${actual_inventory}" | sed 's/^/  /' >&2
  exit 1
fi

chmod +x \
  "${distribution_dir}/bin/darwin-arm64/ctrace" \
  "${distribution_dir}/bin/linux-amd64/ctrace" \
  "${distribution_dir}/bin/linux-arm64/ctrace"

checksum()
{
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1"
  else
    shasum -a 256 "$1"
  fi
}

cd "${distribution_dir}"
{
  while IFS= read -r file; do
    checksum "${file}"
  done < <(find bin THIRD_PARTY_LICENSES -type f | LC_ALL=C sort)
  checksum LICENSE.txt
  checksum THIRD_PARTY_NOTICES.md
} > SHA256SUMS

rm -f ctrace.zip
zip -r ctrace.zip bin LICENSE.txt THIRD_PARTY_LICENSES THIRD_PARTY_NOTICES.md SHA256SUMS
