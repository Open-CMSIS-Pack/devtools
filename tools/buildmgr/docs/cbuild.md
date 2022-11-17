# cbuild - Build Invocation

The **cbuild** utility implements the build flow by chaining the utilities *cbuildgen*, *CMake*, and a
*CMake generator*. It replicates the build steps of Pack-aware IDEs with the following build flow:

- Call *cbuildgen* to generate a packlist with URLs of missing software packs, if packs are missing:
- Call [**cpackget**](../../cpackget/docs/cpackget.md) to download and install missing software packs.
- Call *cbuildgen* to generate a CMakeLists.txt file.
- Call [**CMake**](https://cmake.org/documentation/) to compile the project source code into the binary image using the
specified *CMake generator*.

## Invocation

The **cbuild** utility has the following command line invocation and uses `*.cprj` project file that is generate by
various tools, for example the [**csolution**](../../projmgr/docs/Manual/Overview.md) project manager.

```txt
Usage:
  cbuild <project.cprj> [flags]

Flags:
  -c, --clean              Remove intermediate and output directories
  -d, --debug              Enable debug messages
  -g, --generator string   Select build system generator (default "Ninja")
  -h, --help               Print usage
  -i, --intdir string      Set directory for intermediate files
  -j, --jobs int           Number of job slots for parallel execution
  -l, --log string         Save output messages in a log file
  -o, --outdir string      Set directory for output files
  -p, --packs              Download missing software packs with cpackget
  -q, --quiet              Suppress output messages except build invocations
  -r, --rebuild            Remove intermediate and output directories and rebuild
  -s, --schema             Check *.cprj file against CPRJ.xsd schema
  -t, --target string      Optional CMake target name
  -u, --update string      Generate *.cprj file for reproducing current build
  -v, --verbose            Enable verbose messages from toolchain builds
  -V, --version            Print version
```

## Command Line Examples

The following examples show typical invocations of the **cbuild** utility.

---

*Simple Build:* Translate the application that is defined with the project file `MyProgram.cprj`

```bash
cbuild MyProgram.cprj 
```

---

*Protocol Output:* Translate with using the directory `./IntDir/Debug` to store intermediate files and `./OutDir` for
the final output image along with build information.  Store the build output information the file `MyProgram.log`.

```bash
cbuild MyProgram.cprj -i ./IntDir/Debug -o ./OutDir -l MyProgram.log
```

---

*Clean Project:* Remove the build output and intermediate files of the previous build command.

```bash
cbuild MyProgram.cprj -i ./IntDir/Debug -o ./OutDir -c
```

---

*Quite Mode:* Build the application, but suppress details about the build process.

```bash
cbuild MyProgram.cprj -q
```

---

*Restrict Cores:* Build the application, but only use 2 processor cores during build process.

```bash
cbuild MyProgram.cprj -j2
```

## Example Build Process

The following command shows the output of a **cbuild** command that also downloads a missing software pack.

```bash
cbuild Blinky.test.cprj
```
