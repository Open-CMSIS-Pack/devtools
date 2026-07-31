<!-- Copyright (c) 2026 Arm Limited. All rights reserved. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# ctrace Platform Runtime Inventory

The ctrace release combines the application dependencies listed in
`THIRD_PARTY_NOTICES.md` with components selected by each platform toolchain.
This inventory makes that separate runtime layer explicit; it is not a
substitute for inspecting each release artifact or for satisfying the
applicable runtime license terms.

- **Linux AMD64 and Arm64:** The linker is invoked with `-static`. The executable
  can therefore contain GNU C Library, GNU C++ Library, GCC runtime, and
  operating-system support code selected by the release toolchain. GNU libc is
  predominantly LGPL-2.1-or-later; libstdc++ and libgcc are distributed under
  GPL-3.0-or-later with the GCC Runtime Library Exception. The exact files and
  licenses depend on the pinned compiler, libc, and architecture.
- **Windows AMD64 and Arm64:** The MSVC runtime is selected with CMake's
  multithreaded static runtime setting. Microsoft toolchain runtime terms apply
  to incorporated runtime code.
- **macOS Arm64:** System runtime libraries are supplied by macOS; ctrace
  application dependencies remain linked into the executable.

Before publishing a production release, inspect the actual binaries produced
by the pinned runner images, record their runtime provenance, and confirm that
the corresponding notices, license texts, and distribution obligations are
complete. `THIRD_PARTY_NOTICES.md` intentionally does not claim to be a
complete inventory of compiler or operating-system runtime code.

In particular, publishing a statically linked glibc executable requires the
LGPL notices and license text, the corresponding library source and notices,
and a practical LGPL 2.1 section 6 mechanism that lets recipients relink ctrace
with a modified glibc, such as matching application object files and documented
relink instructions. Apache-2.0 remains the license of ctrace itself; static
linking does not relicense the application, but all applicable license
conditions must be satisfied together.
