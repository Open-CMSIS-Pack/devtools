# ctrace Third-Party Notices

The ctrace executable incorporates the following open-source components. The
release archive contains the unmodified license text for each component under
`THIRD_PARTY_LICENSES`. OpenCSD's retained source-file copyright notices are
provided in `THIRD_PARTY_LICENSES/OpenCSD-NOTICE.txt`.

| Component | Version | License      | Source                                 |
| --------- | ------: | ------------ | -------------------------------------- |
| cxxopts   |   3.3.1 | MIT          | <https://github.com/jarro2783/cxxopts> |
| yaml-cpp  |   0.9.0 | MIT          | <https://github.com/jbeder/yaml-cpp>   |
| OpenCSD   |   1.8.3 | BSD-3-Clause | <https://github.com/Linaro/OpenCSD>    |

The source revisions used for this notice are pinned by the devtools gitlinks:

- cxxopts: `44380e5a44706ab7347f400698c703eb2a196202`
- yaml-cpp: `56e3bb550c91fd7005566f19c079cb7a503223cf`
- OpenCSD: `59f15320637ce6e510604e7bed550d7ad109d15d`

GoogleTest is used only to build the test executable and is not incorporated
into the released ctrace executable.

## Dependency build configuration

ctrace consumes the repository-wide cxxopts and yaml-cpp targets configured by devtools.

`tools/ctrace/cmake/dependencies/ConfigureOpenCSD.cmake` builds OpenCSD's
upstream minimal static target, enables large trace-source IDs, and adjusts its
MSVC build options. It applies `tools/ctrace/cmake/dependencies/OpenCSD.patch`,
which replaces an invalid empty-vector subscript in the ITM sync search with
`std::vector::data()`. See the [OpenCSD issue notes](opencsd-issues.md).

The OpenCSD minimal static target still contains upstream decoders other than
ITM because OpenCSD 1.8.3 does not expose a supported ITM-only CMake target.
ctrace does not maintain a private source list that could silently diverge
from upstream.

Compiler and operating-system runtime code is separate from the application
dependency list above and must be reviewed for every production artifact.
