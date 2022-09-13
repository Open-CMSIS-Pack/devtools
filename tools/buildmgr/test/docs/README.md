# Automated Test Development

This project contains automated integration and unit tests. Developed using
google test framework in order to validate various features and functionalities
of `cbuildgen` and its components

## How to run the tests

Please employ the sequence of the steps mentioned [here](../../../../README.md).

## Integration Tests with commands & args

This section lists the important integration tests to validate `cbuildgen` with
different commands and arguments.

<!-- markdownlint-capture -->
<!-- markdownlint-disable MD013 -->

| Test | Command | Arguments | Env | Comments |
| ---- | ------- | --------  | --- | -------- |
| GenCMake_Fixed_Cprj | cmake | --update=Simulation.fixed.cprj | Yes | Check creation of fixed version of CPRJ file |
| InvalidCommandTest | Invalid | | | Test with Invalid command |
| GenCMakeTest | cmake | | Yes | Checks the generation of CMakeLists |
| Gen_Output_In_SameDir | cmake | --outdir=OutDir, ./Build | Yes | Test to build the output directory in same folder |
| GenCMake_Under_multipleLevel_OutDir_Test | cmake | --outdir=./Out1/Out2  --intdir=./Int1/Int2 | Yes | Tests generation of build directory under multiple levels |
| GenCMake_Output_At_Absolute_path | cmake | --outdir=ABSOLUTE_PATH --intdir=ABSOLUTE_PATH | Yes | Tests generation of build directory under abs path |
| GenCMake_Output_At_Relative_path | cmake | --outdir=../RelativeOutput --intdir=../RelativeIntermediate | Yes | Tests generation of build directory under rel path |
| NoArgTest | | | Yes | Checks cbuildgen with no command, no args |
| MultipleCommandsTest | packlist cmake | | Yes | Tests multiple commands to cbuildgen |
| NoTargetArgTest | cmake | | Yes | No target specified |
| InvalidArgTest | cmake | --Test | Yes | Test with Invalid arguments |
| GeneratePackListTest | packlist | | Yes | Checks the missing packs |
| GeneratePackListDirTest | packlist | --intdir=INPUT_PATH | Yes | Checks the missing packs |
| GenCMake_DuplicatedSourceFilename | cmake | | Yes | Checks the cmake command with project having duplicate source file names |
| Layer_Extract | extract | --outdir=ABSOLUTE_PATH | Yes | Checks extraction of layers from projects |
| Layer_Compose | compose | 1.clayer..N.clayer | Yes | Checks Creation of new project from layers |
| Layer_Add | add | 1.clayer..N.clayer | Yes | Tests addition of layers to the project |
| Layer_Remove | remove | --layer=LAYER_NAME | Yes | Checks removal of layers from project |
| MkdirCmdTest | mkdir | DIR_1...DIR_N | Yes | Test creation of directory(s) |
| RmdirCmdTest | rmdir | DIR_1...DIR_N | Yes | Checks removal of directory(s) |
| TouchCmdTest | touch | FILE | Yes | Test to create, change and modfiy timestamps of file |
| MultipleAuxCmdTest | mkdir rmdir touch | | Yes | Test with multiple commands |

<!-- markdownlint-restore -->
