# ctrace Third-Party Notices

The ctrace executable incorporates the following open-source components. The
release archive contains the unmodified license text for each component under
`THIRD_PARTY_LICENSES`. OpenCSD's retained source-file copyright notices are
provided in `THIRD_PARTY_LICENSES/OpenCSD-NOTICE.txt`.

| Component | Version | License      | Source                                 |
| --------- | ------: | ------------ | -------------------------------------- |
| cxxopts   |   3.0.0 | MIT          | <https://github.com/jarro2783/cxxopts> |
| yaml-cpp  |   0.8.0 | MIT          | <https://github.com/jbeder/yaml-cpp>   |
| OpenCSD   |   1.8.3 | BSD-3-Clause | <https://github.com/Linaro/OpenCSD>    |

The source revisions used for this notice are pinned by the devtools gitlinks:

- cxxopts: `df229cff0d5b96e146f3f11441f714e8e240cad0`
- yaml-cpp: `f7320141120f720aecc4c32be25586e7da9eb978`
- OpenCSD: `59f15320637ce6e510604e7bed550d7ad109d15d`

GoogleTest is used only to build the test executable and is not incorporated
into the released ctrace executable.

## Dependency build configuration

ctrace does not add private dependency patches. It consumes the repository-wide
cxxopts and yaml-cpp targets configured by devtools.

`tools/ctrace/cmake/dependencies/ConfigureOpenCSD.cmake` builds OpenCSD's
upstream minimal static target, enables large trace-source IDs, and adjusts its
MSVC build options. The pinned OpenCSD sources remain unchanged.

The OpenCSD minimal static target still contains upstream decoders other than
ITM because OpenCSD 1.8.3 does not expose a supported ITM-only CMake target.
ctrace does not maintain a private source list that could silently diverge
from upstream.

Compiler and operating-system runtime code is separate from the application
dependency list above and must be reviewed for every production artifact.
