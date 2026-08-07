# Repository Instructions

## ctrace workflow triggers

- Keep the `push.paths` and `pull_request.paths` filters in `.github/workflows/ctrace.yml` identical.
- Limit them to the ctrace workflow and matrix, the root and ctrace CMake configuration, and ctrace source and tests:
  `.github/workflows/ctrace.yml`, `.github/matrix_includes_ctrace.json`, `CMakeLists.txt`,
  `tools/ctrace/CMakeLists.txt`, `tools/ctrace/cmake/**`, `tools/ctrace/src/**`, and `tools/ctrace/test/**`.
- Never add `.gitmodules` or paths below `external/` to the ctrace workflow triggers. Changes made only in an external
  submodule repository do not trigger workflows in this repository.
