# Pack Checking Tool

The utility `packchk` assists the validation of a CMSIS-Pack. It operates on
the unzipped content of the Software Pack. It distributed as part of
the [CMSIS-Toolbox](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/installation.md).

`packchk` performs the following operations:

- Checks the `*.pdsc` file against the PACK.xsd schema file in the installation path.
- Reads the content of the specified `*.pdsc` file. The path to this `*.pdsc`  file is considered as root directory of
  the Software Pack.
- Verifies the existence of all files in the Software Pack that are referenced  in the `*.pdsc` file.
- Checks for presence and correctness of mandatory elements such as `<vendor>`,  `<version>`, etc. - Optionally, reads
  other `*.pdsc` files to resolve dependencies on `<apis>`,  `<boards>`, and `<conditions>`.
- Optionally, verifies the element `<url>`.
- Optionally, composes the versioned pack ID from the information contained in the PDSC:
  `package:vendor.package:name.release.latest:version`
- Sets the exit status reflecting the validation result to:
  - 0 : no errors detected
  - 1 : errors during validation detected

## Usage

```bash
packchk [-V] [--version] [-h] [--help]
         [OPTIONS...] <PDSC file>

packchk options:
 -i, --include arg           PDSC file(s) as dependency reference
 -b, --log arg               Log file
 -x, --diag-suppress arg     Suppress Messages
 -s, --xsd arg               Specify PACK.xsd path.
 -v, --verbose               Verbose mode. Prints extra process information
 -w, --warning arg           Warning level [0|1|2|3|all] (default: all)
 -u, --url arg               Verifies that the specified URL matches with the 
                             <url> element in the *.PDSC file (default: "")
 -n, --name arg              Text file for pack file name (default: "")
 -V, --version               Print version
 -h, --help                  Print usage
     --disable-validation    Disable the pdsc validation against the PACK.xsd.
     --allow-suppress-error  Allow to suppress error messages
     --break                 Debug halt after start
     --ignore-other-pdsc     Ignores other PDSC files in working folder
     --pedantic              Return with error value on warning
```

## Quick Start

This tutorial aims to get you up and running with `packchk` using **CMake**. We
recommend this tutorial as a starting point. In order to build `packchk` and
run related tests. Please employ the sequence of steps mentioned
[here](./../../../README.md).

> Note: `PackChkIntegTests` makes use of test packs from
[test](./../../../test/packs/ARM) & [data](./../test/data) directory contains the example
pack required for integration tests.

### Usage Examples

Run `packchk` on the package description file called `MyVendor.MyPack.pdsc`.
It runs a schema check and verifies the file against the Software Pack that is located in the same directory.

```bash
packchk MyVendor.MyPack.pdsc
```

Run `packchk` on the package description file called MyVendor.MyPack.pdsc in the
current directory and try to resolve conditions using the RefVendor.RefPack.pdsc file
based in another directory.

```bash
packchk MyVendor.MyPack.pdsc -i /path/to/reference/pdsc/RefVendor.RefPack.pdsc
```

Run `packchk` on the package description file called `MyVendor.MVCM3.pdsc`,
verify the URL to the Pack Server, and generate a ASCII text file with the
standardized name of the Software Pack.

```bash
packchk "MyVendor.MVCM3.pdsc" -u "http://www.myvendor.com/pack" -n packname.txt
```

Run `packchk` on the package description file called MyVendor.MVCM3.pdsc.
Suppress validation messages M304 and M331.

```bash
packchk MyVendor.MVCM3.pdsc --diag-suppress M304,M331   // messages as a list
packchk MyVendor.MVCM3.pdsc -x M304 -x M331             // option repeated
```

## Error and Warning Messages

The following table explains the categories for the output messages issued by
PackChk utility. Sections below list the errors and warnings and contain
recommendations on how to resolve them.

### Categories

<!-- markdownlint-capture -->
<!-- markdownlint-disable MD013 -->

| Code           | Category            | Description
|:---------------|:--------------------|:-----------------------------------
| M0xx           | Info messages       | Help and Progress messages. No action required.
| M1xx           | Internal errors     | Internal execution errors
| M2xx           | Invocation Errors   | Errors in the command line input.
| M3xx, M4xx     | Validation Messages | Errors and Warnings from the pack validation.
| M5xx           | Model Errors        | Errors in RTE model creation

### Internal Errors

The errors in this category are issued because of an internal error in the `packchk` utility.

| Message Number     | Type                | Description               | Details and Actions
|:------------------ |:--------------------|:--------------------------|:------------------------------
| M101               | ERROR               | Unknown error!            | Contact Arm for clarifications
| M103               | ERROR               | Internal Error: 'REF'     | Contact Arm for clarifications
| M104               | ERROR               | Path empty when searching for other PDSC files | Execute the check in verbose mode and try to identify the PDSC file intended for use. Correct the PDSC.
| M108               | TEXT                | Reading PDSC File failed! | XML parsing of the PDSC file failed. Verify syntax of the PDSC file.
| M109               | TEXT                | Constructing Model failed!| Contact Arm for clarifications
| M110               | TEXT                | Verifying Model failed    | Contact Arm for clarifications

### Invocation Errors

The errors in this category are issued because of an incorrect command-line input that prevents the `packchk`
execution.

| Message Number     | Type                | Message Text                                                              | Details and Actions
|:------------------ |:------------------- |:--------------------------------------------------------------------------|:---------------------------------------------------------
| M202               | ERROR               | No PDSC input file specified                                              | Correct the command line. **packchk** expects a *.pdsc file name as input.
| M203               | ERROR               | Error reading PDSC file _'PATH/FILENAME'!_                                | Verify the PDSC file for consistency.
| M204               | ERROR               | File not found: _'PATH'_                                                  | The specified PDSC file could not be found in the _PATH_ displayed in the message. Correct the path or the filename.
| M205               | ERROR               | Cannot create Pack Name file _'PATH'_                                     | Check the disk space or your permissions. Correct the path name.
| M206               | ERROR               | Multiple PDSC files found in package: _'FILES'_                           | Only one PDSC file is allowed in a package. Remove unnecessary PDSC files. The message lists all \*.pdsc files found.
| M207               | ERROR               | PDSC file name mismatch! Expected: _'PDSC1.pdsc'_ Actual : _'PDSC2.pdsc'_ | The PDSC file expected has not been found. Rename or exchange the PDSC file.
| M210               | ERROR               | Only one input file to be checked is allowed.                             | You can only check one PDSC file at a time.
| M218               | ERROR               | Cannot find the schema file specified by "--xsd".                         | CHeck whether the file exists.

### Validation Messages

The messages in this category are issued by PackChk during package validation.
This can be for example incorrect use of the pack schema in the PDSC file,
missing files, broken dependencies and others. ERROR messages must be resolved
to ensure that the package is compliant to the CMSIS-format. WARNING messages
should be checked and are strongly recommended to be resolved.

| Message Number     | Type                | Message Text               | Details and Actions
|:------------------ |:------------------- |:-------------------------- |:---------------------------------------------------------
| M301               | ERROR               | Checking Pack URL of PDSC file failed: Expected URL : _'URL1'_ Package URL : _'URL2'_ | The URL specified in the package does not match the value entered for comparison in option `-u`. Check for possible misspellings in both URLs. Refer to [/package/url](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_url).
| M302               | ERROR               | No vendor tag found in the PDSC file! Add the `<vendor>` tag and provide the vendor name. For example: `<vendor>Keil</vendor>`. | Vendor name is mandatory required in the package description but was not found in the [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package) element in the PDSC file. Within the parent `<package>` element enter the tag `<vendor>` and add the vendor name. For example: `<vendor>Keil</vendor>`. Refer to [/package/vendor](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_vendor).
| M303               | ERROR               | No package name found in the PDSC file! Add the `<name>` tag and provide the package name. For example: `<name>MCU-Name_DFP</name>`. | Package name is mandatory required in the package description but was not found in the [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package) element in the PDSC file. Within the parent `<package>` element enter the tag `<name>` and add the package name. For example `<name>MCU-Name_DFP</name>`. Refer to [/package/name](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_name).
| M304               | WARNING             | No package URL (`<url>`-tag and/or value) found in PDSC file! | Package URL is mandatory required in the package description but was not found in the [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package) element in the PDSC file. Within the parent `<package>` element enter the tag `<url>` and add the URL that should be used to download the package. Refer to [/package/url](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_url).
| M305               | ERROR               | No releases (`<release>` elements in a `<releases>`-tag) found in PDSC file!" | At least one package release shall be specified in the PDSC file, but none was not found. Use `<release>` tag to specify release information in `<releases>` element. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M306               | ERROR               | No package description found in the PDSC file. Add the `<description>`-tag and provide a descriptive text. | Package description is mandatory required in the package description but was not found in the [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package) element in the PDSC file. Within the parent `<package>` element add the tag `<description>` and briefly describe the package content. Refer to [/package/description](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_description). |
| M307               | ERROR           | Either attribute _'TAG'_ or _'TAG2'_ + _'TAG3'_ must be specified for 'memory' | `<memory>` element requires that either attribute id or attributes name and access are specified, but none of those were found. Specify the expected attributes. Refer to [/package/devices/family/.../memory](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_memory).
| M308               | ERROR           | Attribute _'TAG'_ missing on _'TAG2'_ | Element _'TAG2'_ requires attribute _'TAG'_, but it was not found. Add missing attribute.
| M309               | ERROR           | Attribute _'TAG'_ missing on _'TAG2'_ (when _'TAG3'_ is specified) | Add missing attribute _'TAG'_ or remove the attribute _'TAG3'_ in the element _'TAG2'_.
| M310               | ERROR               | Filename mismatch (case sensitive): PDSC name : _'PDSC\_FILENAME'_ Filename : _'SYSTEM'_ | Filenames are case sensitive. Correct spelling.
| M311               | ERROR           | Redefinition of _'TAG'_ : _'NAME'_, see Line _'LINE'_ | Remove the redefinition of element _'TAG'_ with name _'NAME'_.
| M312               | WARNING         | No _'TAG'_ found for device _'NAME'_ | Element _'TAG'_ is missing for device _'NAME'_. Add missing element.
| M315               | ERROR               | Invalid URL / Paths to Drives are not allowed in Package URL: _'URL'_ | Correct package URL _'URL'_. Refer to [/package/url](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_url).
| M316               | WARNING             | URL must end with slash '/': 'URL' | Correct package URL _'URL'_. Refer to [/package/url](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#package_url).
| M323               | ERROR               | File/Path not found: _'PATH'_ | The file or path _'PATH'_ entered in the PDSC file could not be found. Verify the path information.
| M324               | WARNING             | Board referenced in Example _'EXAMPLE'_ is undefined: _'VENDOR'_ : _'BOARD'_ | The definition of the board referenced in the example application could not be found. Define the board ([/package/boards/board](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board)) or correct the reference information ([/package/examples/example/board](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_examples_pg.html#element_example_board)).
| M325               | ERROR               | Board _'NAME'_ redefined, already defined in Line _'LINE'_: _'PATH'_ | This board has been defined already in the line _'LINE'_ of the file _'PATH'_. Verify and remove one of the board definitions. Refer to [/package/boards/board](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board).
| M326               | ERROR               | Path is not conformant: _'PATH'_: Absolute Paths or Drive references are not allowed, use Paths relative to PDSC file. | The path needs to be relative to the PDSC file so that a dependency of a certain file system does not occur.
| M327               | WARNING             | Path is not conformant: _'PATH'_: Backslashes are not recommended, use forward slashes. | Paths to files should adhere to the POSIX standard using forward slashes (/).
| M328               | ERROR               | Version not set for Release Information _'DESCR'_ | The release defined through the description _'DESCR'_ requires the attribute `<version>`. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M329               | ERROR               | Description not set for Release Information _'VER'_ | The release defined through the version _'VER'_ needs a description. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M330               | ERROR               | Condition redefined: _'COND'_, already defined in Line _'LINE'_ | The condition has been defined already in a previous line. Correct the condition name, or remove the duplicate. Refer to [/package/conditions/condition](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_conditions_pg.html#element_condition).
| M331               | WARNING             | Condition unused: _'COND'_ | The condition has been defined but not used further. Remove the condition or add condition rules. Refer to [/package/conditions/condition](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_conditions_pg.html#element_condition).
| M332               | ERROR               | Condition undefined: _'COND'_ | A condition has been used but not defined. Correct the name of the condition or define the missing condition. Refer to [/package/conditions/condition](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_conditions_pg.html#element_condition).
| M333               | WARNING             | Component has no condition: Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_, _Csub='CSUB'_, _Cversion=_ 'CVER'| The component defined has no condition. If the component has restrictions, then add a condition to the component definition. Refer to [/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component).
| M334               | WARNING             | Config File has no version: _'PATH'_ | Specified file 'PATH' does not contain version information. It is recommended to specify versions for configuration files. Add attribute version with the file version in the `<file>` tag that defines the _'PATH'_ file in the PDSC. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M335               | WARNING             | Component declared as 'Board Support' has no ref to a device: Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_, Cversion= _'CVER'_ | A component defined as 'board support' needs a reference to a device. Add a device or correct the component definition. Refer to attribute _Cclass_ of [/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component) and [/package/boards/board/mountedDevice](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board_mountedDevice).
| M336               | WARNING             | No reference to a device or device not found: Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_, Cversion= _'CVER'_ | Define the device [/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device) or correct the information about the device in the reference.
| M337               | WARNING             | File with category _'CAT'_ has wrong extension _'EXT'_: _'PATH'_ | The extension of the file does not match the file category _'CAT'_ specified in the category attribute. Verify the extension to match the category. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M338               | WARNING             | No releases found. | The PDSC file is missing release information. Add `<release>` information to the file. Refer to [/package/releases element](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html).
| M339               | WARNING             | Include Path _'PATH'_ must not be a file! | The path specified contains a filename. Correct the path infomation and remove the filename. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M340               | WARNING             | Include Path _'PATH'_ must end with '/' or '\\' | Include paths must end with a slash or backslash. Verify and correct the path name. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M341               | WARNING             | File with _'COMP'_ dependency must have extension _'EXT'_ : _'PATH'_ | The file _'PATH'_ with dependency on component _'COMP'_ must have a specific extension _'EXT'_. Verify the dependency and correct the file extension. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M342               | WARNING             | File with attribute _'ATTR'_ must not have category _'CAT'_: _'PATH'_ | File _'PATH'_ is defined with the attribute attr set to _'ATTR'_. This conflicts with the file category _'CAT'_ specified in category attribute. For example attr=config and category=include are not allowed for the same file. Correct the file attribute or the file category. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M343               | WARNING             | File with attribute _'ATTR'_ requires _'ATTR2'_ attribute: _'PATH'_ | File _'PATH'_ is defined with the attribute _'ATTR'_ that requires presense of attribute _'ATTR2'_ as well, but such attribute was not found. For example, attr=template requires that attribute select is defined. Add the required attribute _'ATTR2'_ or correct the attribute _'ATTR'_ in the `<file>` element. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M344               | WARNING             | File shall have condition containing _'COND'_: _'PATH'_ | File definition for _'PATH'_ shall contain a condition _'COND'_. Add attribute condition with the _'COND'_ in the `<file>` tag that defines the _'PATH'_ file. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M345               | WARNING             | URL not found : _'URL'_ | The specified URL could not be found. Correct the URL. Refer to [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package).
| M346               | WARNING             | Referenced device(s) in _'BOARD'_ not found: _'DEVICE'_ | The device or devices specified for the board could not be found. Verify and correct the device name or the board name. Refer to [/package/boards/board/mountedDevice](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board_mountedDevice) and [/package/boards/board/compatibleDevice](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board_compatibleDevice).
| M347               | WARNING             | Generator ID in Component Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_, Cversion= _'CVER'_ is undefined: _'GENID'_ | The generator ID used in the component could not be found. Verify and correct the generator ID ([/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component)), or define the generator ID (ref [/package/generators/generator](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_generators_pg.html#element_generator)).
| M348               | WARNING             | Feature redefined for _'MCU'_, see Line _'REF\_LINE'_: _'FEATURE'_ | This feature _'FEATURE'_ has been defined already on the same level in line _'REF\_LINE'_. The feature characteristics defined on line _'LINE'_ overwrite those from _'REF\_LINE'_. Correct the feature ([/package/devices/family/.../feature](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_feature)).
| M349               | WARNING             | Examples found, but no board description(s) found | Example projects have been found for a board that was not defined. Correct the entry for the examples ([/package/examples/example/board](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_examples_pg.html#element_example_board)) or define the board ([/package/boards/board](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board)).
| M350               | WARNING             | No _'COMP'_ found for _'VENDOR'_ : _'MCU'_ (_'COMPILER'_) | The package ([/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package)) defines a Vendor-MCU combination for which no component was defined. Define a component ([/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component)) or verify the _'VENDOR'_ - _'MCU'_ settings.
| M351               | WARNING             | Component _'COMP'_ (_'COMPID'_) error for _'VENDOR'_: _'MCU'_ ( _'COMPILER'_): _'MSG'_ | An unspecified error was found for the component. The message might give detailed information about the error.
| M352               | WARNING             | No Directories/Files found for _'COMP'_ (_'COMPID'_) for _'VENDOR'_: _'MCU'_ (_'COMPILER'_) | No files or directories could be found for the defined component. Add the missing information. Refer to [/package/.../files](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_files).
| M353               | WARNING             | No _'FILECAT'_ File found for Component _'COMP'_ (_'COMPID'_) for _'VENDOR'_ : _'MCU'_ (_'COMPILER'_) | No file with the mentioned file category was found for the component. Verify whether the file exists or correct the information. Refer to attribute _category_ in [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M354               | WARNING             | Multiple _'FILECAT'_ Files found for Component _'COMP'_ (_'COMPID'_) for _'VENDOR'_ : _'MCU'_ (_'COMPILER'_) | Multiple files with the mentioned file category were found for the specified component. Verify and rename the files, or correct the component settings ([/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file)).
| M355               | WARNING          | No _'FILECAT'_ Directory found for Component _'COMP'_ (_'COMPID'_) for _'VENDOR'_ : _'MCU'_ (_'COMPILER'_) | The directory specified for the file category was not found. Correct the information in the component settings ([/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component)).
| M358               | WARNING           | Header File _'HFILE'_ for _'CFILE'_ missing for Component _'COMP'_ (_'COMPID'_) for _'VENDOR'_ : _'MCU'_ (_'COMPILER'_) | The header file defined for the component could not be found. Verify the header file settings ([/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file)) or whether the file exists.
| M359               | WARNING          | Family has no Device(s) or Subfamilies: _'FAMILY'_ | The device family has no devices or subfamilies. Add the missing information ([/package/devices/family](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_family)).
| M360               | WARNING          | Subfamily has no Device(s): _'SUBFAMILY'_ | Add the missing information. Refer to [/package/devices/family/subFamily](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_subFamily).
| M361               | WARNING          | Generator ID in Taxonomy Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_ is undefined: _'GENID'_ | The generator ID used in the taxonomy is not defined. Define or correct the generator ID ([/package/taxonomy element](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_taxonomy.html)).
| M362               | WARNING         | Not all Component Dependencies for Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_, Csub= _'CSUB'_, Cversion= _'CVER'_, Capiversion= _'APIVER'_ can be resolved. RTE Model reports: _'MSG'_ | Some of the component dependencies could not be resolved. The message might contain additional information. Verify and correct component definition and dependency information. Refer to [/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component) attribute _condition_.
| M363               | WARNING         | No API defined for Component Cclass= _'CCLASS'_, Cgroup= _'CGROUP'_, Csub= _'CSUB'_, Cversion= _'CVER'_, Capiversion= _'APIVER'_ | The package is missing the API information for the specified component. Refer to [/package/apis](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_apis_pg.html#element_apis).
| M364               | WARNING         | No Devices for Condition _'COND'_ available. | The specified condition refers to a device that does not exist. Define the device ([/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device)) or correct the information for the condition ([/package/conditions/condition](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_conditions_pg.html#element_condition)).
| M365               | ERROR           | Redefined _'DEVTYPE'_ _'MCU'_ found, see Line _LINE_ | Remove duplicate device/variant entries. Refer to [/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device).
| M366               | ERROR           | Redefined _'DEVTYPEEXIST'_ as _'DEVTYPE'_ _'MCU'_ found, see Line _LINE_ | Device has been redefined as variant or vice versa. Remove duplicate device/variant entries. Refer to [/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device).
| M367               | ERROR           | Redefined _'TYPE'_ _'NAME'_ found, see Line _'LINE'_ | Remove duplicate device/variant entries. Refer to [/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device).
| M369               | WARNING         | Feature is already defined for _'DEVICE'_ and will be added, see Line _'LINE'_: _'FEATURE'_. | This feature _'FEATURE'_ has been defined already on a higher level and as such it gets added to this _'DEVICE'_. This is usually done when some devices have a higher number of basic features. Correct the feature ([/package/devices/family/.../feature](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_feature)) if this is a typo.
| M370               | WARNING         | URL is not conformant: _'URL'_: Backslashes are not allowed in URL, use forward slashes. | Use standard URL notation using forward slashes (/).
| M371               | ERROR           | _'SECTION'_ Feature for _'MCU'_: _'FEATURE'_ unknown. | This feature _'FEATURE'_ is unknwon to the specified _'MCU'_. Correct the feature ([/package/devices/family/.../feature](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_feature)) if this is a typo.
| M372               | ERROR           | _'SECTION'_ Feature for _'MCU'_: _'FEATURE'_ misspelled, did you mean _'KNOWNFEATURE'_ (_'DESCR'_). | This feature _'FEATURE'_ resembles the feature _'KNOWNFEATURE'_. Correct the feature ([/package/devices/family/.../feature](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_feature)) if this is a typo.
| M373               | ERROR           | Unsupported Schema Version: _'VER'_. | The schema version is not supported. Verify the attribute schemaVersion of the element [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package).
| M374               | ERROR           | While checking Feature for _'MCU'_: Pname _'CPU'_ not found. | The processor could not be found for the specified device. Refer to [/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device) and [/package/devices/family/.../processor](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_processor).
| M375               | ERROR           | _'path/pdsc\_file'_: No `<mountedDevice>` for board _'BOARD'_ found. | If a board element does not contain a `<mountedDevice>` element, then the examples for this board are not shown and example projects may not appear in the development tools. Refer to [/package/boards/board/mountedDevice](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board_mountedDevice) of [/package/boards](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_boards).
| M376               | ERROR           | Schema Version not set! | Set a valid schema version in the PDSC file. Refer to schemaVersion attribute in the [/package](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#element_package).
| M377               | WARNING         | File _'NAME'_ _'TYPE'_ must have _'attr="config"'_ | The category _'TYPE'_ of the file _'NAME'_ requires it to be defined as a configuration file. Add the attribute attr="config" to the `<file>` element that defines the file. Refer to [/package/.../files/file](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_file).
| M378               | WARNING         | Component Cclass=_'CCLASS_, Cgroup=_'CGROUP_, Csub=_'CSUB'_, Cversion=_'CVER'_, implements the API defined in _'NAME'_ but does not attribute 'Capiversion' specifying the version it implements. | Add attribute Capiversion specifying the API version that is implemented in the component described in the message. Refer to [/package/components/.../component](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_components_pg.html#element_component).
| M379               | WARNING         | No example(s) found for Board '\[_'VENDOR'_\] _'BOARD'_. | There are no examples found for the board _'BOARD'_. Provide examples. Refer to [/package/examples/example](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_examples_pg.html#element_example).
| M380               | WARNING         | No description found for \[_'VENDOR'_\] 'MCU' | No description was found for the device _'MCU'_ from _'VENDOR'_. Use description attribute to provide information about the device, its subfamily, or family. Refer to [/package/devices/family/.../description](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_description).
| M381               | WARNING         | Vendor names are not equal: '\[_'VENDOR'_\] _'MCU'_, MCU '\[_'VENDOR2'_\] _'MCU2'_, see Line _'LINE'_ | Vendor name specified in the Dvendor attribute for the `<mountedDevice>` tag does not match the actual device vendor name. Verify and correct vendor name. Refer to [/package/boards/board/mountedDevice](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_boards_pg.html#element_board_mountedDevice).
| M382               | WARNING         | Requirement <_'TAG'>_ '\[_'VENDOR'_\] _'NAME'_ _'VER'_ could not be resolved._'MSG'_ | The package requires a package with name _'NAME'_ and version _'VER'_ from vendor _'VENDOR'_. That package was not found. Verify the requirement. Use option `-i` to point to the required package. Refer to [/package/requirements/packages](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_requirements_pg.html#element_packages).
| M383               | ERROR           | _'TAG'_ _'NAME'_ is not conformant to the pattern "_'CHAR'"_ | Unsupported characters are found in the _'NAME'_ for the tag/attribute _'TAG'_. Specify the name using only supported characters from _'CHAR'_.
| M384               | ERROR           | Condition _'NAME'_: Direct or indirect recursion detected. Skipping condition while searching for _'NAME2'_ | Resolve recursion in condition _'NAME'_.
| M385               | INFO            | Release date is empty. | In the `<release>` tag add attribute date with the release date. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M386               | WARNING         | Release date is in future: _'RELEASEDATE'_ (today: _'TODAYDATE'_) | Set the date to the future. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M391               | WARNING         | Redefined Item _'NAME'_: _'MSG_ | Multiple defitions of item _'NAME'_ are found. Use unique names for the items listed in the message _'MSG'_.
| M392               | ERROR           | Redefined Device or Variant _'NAME'_: _'MSG'_ | Multiple defitions of device or variant _'NAME'_ are found. Use unique names for the devices listed in the message _'MSG'_. Refer to [/package/devices/family/../device](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_device) and [/package/devices/family/.../device/variant](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_variant)
| M393               | WARNING         | Package has development version _'RELEASEVER'_ | The package has development version specified in the version attribute. For release use only MAJOR, MINOR and PATCH sections as described in the [CMSIS-pack version semantics](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#VersionType). Also refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M394               | ERROR           | Not Semantic Versioning: _'RELEASEVER'_ | Package version does not follow expected [semantic versioning](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_package_pg.html#VersionType). Correct the value specified for the version attribute using correct semantic. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M395               | WARNING         | Release date attribute is not set for release version: _'RELEASEVER'_ | Specify release date in the attribute date for the returned release version _'RELEASEVER'_. Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M396               | WARNING         | Release _'TAG'_ not consecutive (newest first): _'RELEASEVER'_, _'RELEASEDATE'_ (prev.: _'LATESTVER'_, _'LATESTDATE'_, see Line _'LINE'_) | Package releases shall be entered in consecutive way: newest release version and latest date first. Rearrange the releases in the list or correct values for the returned 'TAG' attributes (version or date). Refer to [/package/releases/release](https://www.keil.com/pack/doc/CMSIS/Pack/html/element_releases.html#element_release).
| M397               | WARNING         | File extension '.pdsc' must be lowercase: _'PATH'_ | The PDSC file shall have lowercase extension `.pdsc` Correct extension of the PDSC file _'PATH'_.
| M398               | ERROR           | Attribute 'Dname' missing in expression using 'Pname="_'NAME'"_ | When using attribute Pname in the `<condition>` element the Dname attribue shall be present as well. Add attribute token{Dname} specifying the device name. Refer to [/package/conditions/condition](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_conditions_pg.html#element_condition).
| M399               | WARNING         | Attribute _'TAG'_ is ignored, because _'TAG2'_ + _'TAG3'_ is specified | Deprecated attribute _'TAG'_ is ignored because newer attributes are found. Triggered for id attribute in the [/package/devices/family/.../memory](https://www.keil.com/pack/doc/CMSIS/Pack/html/pdsc_family_pg.html#element_memory) element when name or access attributes are present as well. remove deprecated attribute _'TAG'_

### RTE Model Errors

| Message Number     | Type                | Message Text              | Action
|:------------------ |:------------------- |:------------------------- |:---------------------------------------------------------
| M500               | TEXT                | RTE Model reports: _'MSG'_| Error while preparing data. See massage for more details.
| M502               | TEXT                | RTE Model reports: #error _'NUM'_: _'NAME'_ : _'MSG'_ | Additional software components required.
| M504               | TEXT                | RTE Model reports: MISSING: – _SPACE_ _NAME_ | Add the missing component.

<!-- markdownlint-restore -->
