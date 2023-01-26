# SVD Validation and Generation Tool

The utility `svdconv` validates [CMSIS-SVD](https://arm-software.github.io/CMSIS_5/develop/SVD/html/index.html) files
and generates CMSIS-compliant device header files. It distributed as part of
the [CMSIS-Toolbox](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/installation.md).

`svdconv` performs the following operations:

- Checks the syntactical and structural compliance with the specified CMSIS-SVD format.
- Checks the consistency, correctness, and completeness of the CMSIS-SVD file against the CMSIS-SVD schema file.
- Generates CMSIS-compliant device header files, which can be used for software development.

>` **NOTE:** Consider using the `--strict` option to receive all pedantic warnings. Some rules are skipped by default
due to backward compatibility reasons. All newly developed/updated SVD files should rather respect all rules.

## Usage

```bash
  svdconv.exe [OPTION...] positional parameters

  -o, --outdir arg            Output directory
      --generate arg          Generate header, partition or SDF/SFR file
      --fields arg            Specify field generation:
                              enum/macro/struct/struct-ansic
      --suppress-path         Suppress inFile path on check output
      --create-folder         Always create required folders
      --show-missingEnums     Show SVD elements where enumerated values
                              could be added
      --strict                Strict error checking (RECOMMENDED!)
  -b, --log arg               Log file
  -x, --diag-suppress arg     Suppress Messages
      --suppress-warnings     Suppress all WARNINGs
  -w arg                      Warning level (default: all)
      --allow-suppress-error  Allow to suppress error messages
  -v, --verbose               Verbose mode. Prints extra process
                              information
      --under-test            Use when running in cloud environment
      --nocleanup             Do not delete intermediate files
      --quiet                 No output on console
      --debug arg             Add information to generated files:
                              struct/header/sfd/break
      --version               Show program version
  -h, --help                  Print usage
```

## Return Codes

SVDConv returns the following codes:

| Code |  Description |  Action |
|------|--------------|---------|
| 0 |  OK |  No action required. Validation and conversion performed without errors. |
| 1 |  WARNINGS |  Warnings should be checked an possibly removed. The header file is created and could be used. |
| 2 |  ERRORS |  Errors in the SVD description file. Important elements are missing and must be corrected. |
| 3 |  Error in command line |  Check and correct the command line arguments. |

## Usage Examples

1. Retrieve help information on screen.

   ```bash
   svdconv
   ```

2. Perform a consistency check by passing only the SVD file name. Errors and warnings are printed on screen.

   ```bash
   svdconv ARM_Example.svd 
   ```

   The result is printed on screen:

   ```bash
   MVCM3110.svd(1688) : info
   `<description>` missing for value '2 : MODE2'
   MVCM3110.svd(1692) : info
   `<description>` missing for value '3 : MODE3'
   MVCM3110.svd(1696) : info
   `<description>` missing for value '4 : MODE4'
   Area of improvements:
   * Description contains 267 `<fields>` defined without associated `<enumeratedValues>
   Found 0 Errors and 1 Warnings
   Return Code: 1 (WARNINGS)
   ```

3. Generate the header file. Performs a consistency check. Errors and warnings are printed on screen.

   ```bash
   svdconv ARM_Example.svd --generate=header
   ```

   Code snippet from the generated header file showing the structure for **TIMER0**.

   ```C
   /* ================                     TIMER0                     ================ */
   typedef struct {                                    
     __IO uint32_t  CR;                                
     __IO uint16_t  SR;                                
     __I  uint16_t  RESERVED0[5];
     __IO uint16_t  INT;                               
     __I  uint16_t  RESERVED1[7];
     __IO uint32_t  COUNT;                             
     __IO uint32_t  MATCH;                             
     union {
       __O  uint32_t  PRESCALE_WR;                     
       __I  uint32_t  PRESCALE_RD;                     
     };
     __I  uint32_t  RESERVED2[9];
     __IO uint32_t  RELOAD[4];                         
   } TIMER0_Type;
   ```

4. Generate the header file containing bit fields. Performs a consistency check. Errors and warnings are printed on screen.

   ```bash
   svdconv ARM_Example.svd --generate=header --fields=struct
   ```

   Code snippet from the generated header file showing the structure for **TIMER0**.
   Compare to the code snippet above.

   ```C
   /* ================                     TIMER0                     ================ */
   typedef struct {                                    
     union {
       __IO uint32_t  CR;                              
       struct {
         __IO uint32_t  EN         :  1;               
         __O  uint32_t  RST        :  1;               
         __IO uint32_t  CNT        :  2;               
         __IO uint32_t  MODE       :  3;               
         __IO uint32_t  PSC        :  1;               
         __IO uint32_t  CNTSRC     :  4;               
         __IO uint32_t  CAPSRC     :  4;               
         __IO uint32_t  CAPEDGE    :  2;               
              uint32_t             :  2;
         __IO uint32_t  TRGEXT     :  2;               
              uint32_t             :  2;
         __IO uint32_t  RELOAD     :  2;               
         __IO uint32_t  IDR        :  2;               
              uint32_t             :  3;
         __IO uint32_t  S          :  1;               
       } CR_b;                                         
     };
     
     union {
       __IO uint16_t  SR;                              
       struct {
         __I  uint16_t  RUN        :  1;               
              uint16_t             :  7;
         __IO uint16_t  MATCH      :  1;               
         __IO uint16_t  UN         :  1;               
         __IO uint16_t  OV         :  1;               
              uint16_t             :  1;
         __I  uint16_t  RST        :  1;               
              uint16_t             :  1;
         __I  uint16_t  RELOAD     :  2;               
       } SR_b;                                         
     };
     __I  uint16_t  RESERVED0[5];
     
     union {
       __IO uint16_t  INT;                             
       struct {
         __IO uint16_t  EN         :  1;               
              uint16_t             :  3;
         __IO uint16_t  MODE       :  3;               
       } INT_b;                                        
     };
     __I  uint16_t  RESERVED1[7];
     __IO uint32_t  COUNT;                             
     __IO uint32_t  MATCH;                             
     union {
       __O  uint32_t  PRESCALE_WR;                     
       __I  uint32_t  PRESCALE_RD;                     
     };
     __I  uint32_t  RESERVED2[9];
     __IO uint32_t  RELOAD[4];                         
   } TIMER0_Type;
   ```

<!-- markdownlint-capture -->
<!-- markdownlint-disable MD013 -->

## Error and Warning Messages

The following table shows the errors and warnings issued by svdconv.

### Help messages

| Message Number |  Type |  Message Text |  Details/Action|
|----------------|---------|---------------|--------|
| M020 |  TEXT |  SVD_STRING_OPTIONS |  Displays programm help.|
| M021 |  TEXT |  'DESCR' 'VER' 'COPYRIGHT' |  Displays module name 'DESCR', version 'VER' and copyright information 'COPYRIGHT'.|
| M022 |  TEXT |  Found 'ERR' Error(s) and 'WARN' Warning(s). |  Displays the number of errors/warnings.|
| M023 |  TEXT |  Phase 'CHECK' |  Information about the check phase.|
| M024 |  TEXT |  Arguments: 'OPTS' |  Specify arguments.|

### Informative messages

| Message Number |  Type |  Message Text |  Details/Action|
|----------------|---------|---------------|--------|
| M040 |  Info |  'NAME': 'TIME' ms. Passed |  |
| M041 |  Info |  Overall time: 'TIME' ms. |  |
| M050 |  Info |  Current Working Directory: 'PATH' |  |
| M051 |  Info |  Reading SVD File: 'PATH' |  |
| M061 |  Info |  Checking SVD Description |  |

### Invocation errors

| Message Number |  Type |  Message Text |  Action|
|----------------|---------|---------------|--------|
| M101 |  ERROR |  Unknown error! |  Please contact support.|
| M102 |  ERROR |  MFC initialization failed |  Please contact support.|
| M103 |  ERROR |  Internal Error: 'REF' |  Please contact support.|
| M104 |  CRITICAL |  'MSG' |  Please contact support.|
| M105 |  ERROR |  Cannot add Register to group sorter: 'NAME' |  |
| M106 |  ERROR |  Command 'NAME' failed: 'NUM': 'MSG' |  |
| M107 |  ERROR |  Lost xml file stream. |  Check SVD file.|
| M108 |  ERROR |  SfrDis not supported.Disassembly not supported. |  |
| M109 |  ERROR |  Cannot find 'NAME' |  Check specified file.|
| M111 |  PROGRESS |  'NAME' failed |  Check specified file.|
| M120 |  ERROR |  Invalid arguments! |  Provide a list of valid arguments.|
| M121 |  ERROR |  File not found 'NAME' Check specified file. |  |
| M122 |  ERROR |  Name of command file should follow '@' |  Check specified command.|
| M123 |  ERROR |  File not found: 'PATH'! |  Check speficied path.|
| M124 |  ERROR |  Cannot execute SfrCC2: 'PATH'!" |  Check path to SfrCC2.|
| M125 |  WARNING |  SfrCC2 report: 'MSG' SfrCC2 report end. |  |
| M126 |  WARNING |  SfrDis: 'MSG' |  |
| M127 |  ERROR |  SfrCC2 reports errors! |  Check SVD file.|
| M128 |  WARNING |  SfrCC2 reports warnings! |  Check SVD file.|
| M129 |  ERROR |  Option unknown: 'OPT' |  Check given option 'OPT'.|
| M130 |  ERROR |  Cannot create file 'NAME' |  Check user rights.|
| M132 |  ERROR |  SfrCC2 report: 'MSG' SfrCC2 report end." |  |

### Validation errors

| Message Number |  Type |  Message Text |  Action|
|----------------|---------|---------------|--------|
| M201 |  ERROR |  Tag `<'TAG'>` unknown or not allowed on this level." |  Check tag|
| M202 |  ERROR |  Parse error: `<'TAG'>` = 'VALUE' |  Check tag/value.|
| M203 |  ERROR |  Value already set: `<'TAG'>` = 'VALUE' |  Check tag/value.|
| M204 |  ERROR |  Parse Error: 'VALUE' |  Check value.|
| M205 |  WARNING |  Tag `<'TAG'>` empty |  Assign value to tag.|
| M206 |  ERROR |  DerivedFrom not found: 'NAME' |  Check derivate.|
| M207 |  ERROR |  Expression marker found but no `<dim>` specified: 'NAME' |  Specify dimension.|
| M208 |  ERROR |  Ignoring `<dimIndex>` because specified `<name>` requires Array generation. |  Generate an array.|
| M209 |  WARNING |  CPU section not set. This is required for CMSIS Headerfile generation and debug support. |  Add CPU section.|
| M210 |  WARNING |  Use new Format CMSIS-SVD >= V1.1 and add `<CPU>` Section. |  Update schema and add CPU section.|
| M211 |  ERROR |  Ignoring 'LEVEL' 'NAME' (see previous message) |  |
| M212 |  ERROR |  Address Block `<usage>` parse error: 'NAME' |  Correct address block.|
| M213 |  ERROR |  Expression for 'NAME' incomplete, `<'TAG'>` missing. |  Add tag.|
| M214 |  ERROR |  Peripheral 'NAME' `<dim>` single-instantiation is not supported (use Array instead). |  Correct Regs to Reg[s].|
| M215 |  WARNING |  Size of `<dim>` is only one element for 'NAME', is this intended? |  Check single element.|
| M216 |  WARNING |  Unsupported character found in 'NAME' : 'HEX'. |  Correct name.|
| M217 |  WARNING |  Forbidden Trigraph '??CHAR' found in 'NAME'. |  |
| M218 |  WARNING |  Unsupported ESC sequence found in 'NAME' : 'CHAR'. |  Correct escape sequence.|
| M219 |  ERROR |  C Code generation error: 'MSG' |  |
| M220 |  WARNING |  C Code generation warning: 'MSG' |  |
| M221 |  WARNING |  Input filename must end with .svd: 'NAME' |  Correct input filename extension.|
| M222 |  WARNING |  Input filename has no extension: 'NAME' |  Correct input filename extension.|
| M223 |  ERROR |  Input File Name 'INFILE' does not match the tag `<name>` in the `<device>` section: 'NAME' |  Correct the MCU name.|
| M224 |  WARNING |  Deprecated: 'NAME' Use 'NAME2' instead |  Update SVD file.|
| M225 |  ERROR |  Upper/lower case error: 'NAME', should be 'NAME2'" |  Update SVD file.|
| M226 |  ERROR |  SFD Code generation error: 'MSG' |  |
| M227 |  WARNING |  SFD Code generation warning: 'MSG' |  |
| M228 |  ERROR |  Enumerated Value Container: Only one Item allowed on this Level! |  Remove additional items.|
| M229 |  ERROR |  Register 'NAME' is not an array, `<dimArrayIndex>` is not applicable |  Correct SVD.|
| M230 |  ERROR |  Value 'NAME':'NUM' out of Range for 'LEVEL' 'NAME2'['NUM2']. |  Correct SVD.|
| M231 |  ERROR |  Value `<isDefault>` not allowed for 'LEVEL'. |  Correct SVD.|
| M232 |  ERROR |  Tag `<'TAG'>` name 'NAME' must not have specifier 'CHAR'. Ignoring entry." |  Correct SVD.|
| M233 |  ERROR |  Parse error: `<'TAG'>` = 'VALUE' |  Correct SVD.|
| M234 |  ERROR |  No valid items found for 'LEVEL' 'NAME' |  Correct SVD.|
| M235 |  ERROR |  'LEVEL' 'NAME' cannot be an array. |  Correct SVD.|
| M236 |  ERROR |  Expression for `<'TAG'>` 'NAME' not allowed. |  Correct SVD.|
| M237 |  ERROR |  Nameless 'LEVEL' must have `<'TAG'>`. |  Correct SVD.|
| M238 |  ERROR |  'LEVEL' must not have `<'TAG'>`." |  Correct SVD.|
| M239 |  ERROR |  Dimed 'LEVEL' 'NAME' must have an expression. |  Correct SVD.|
| M240 |  ERROR |  Tag `<'TAG'>` unknown or not allowed on 'LEVEL2':'LEVEL'. |  Correct SVD.|
| M241 |  ERROR |  Parse Error: 'VALUE' invalid for Array generation |  Correct SVD.|
| M242 |  WARNING |  'LEVEL' 'NAME' `<dimArrayIndex>` found, but no `<dim>` |  Correct SVD.|
| M243 |  WARNING |  'LEVEL' 'NAME' `<dimArrayIndex>` found, but `<dim>` does not describe an array |  Correct SVD.|

### Data Check Errors

| Message Number |  Type |  Message Text |  Action|
|----------------|---------|---------------|--------|
|  M301 |  ERROR |  Interrupt number 'NUM' : 'NAME' already defined: 'NAME2' 'LINE' |  |
|  M302 |  ERROR |  Size of Register 'NAME':'NUM' must be 8, 16 or 32 Bits |  |
|  M303 |  WARNING |  Register name 'NAME' is prefixed with Peripheral name 'NAME2' |  RegName = USART_CR ==>` USART->USART_CR|
|  M304 |  WARNING |  Interrupt number overwrite: 'NUM' : 'NAME' 'LINE' |  |
|  M305 |  ERROR |  Name not C compliant: 'NAME' : 'HEX', replaced by '_' |  |
|  M306 |  ERROR |  Schema Version not set for `<device>`. |  |
|  M307 |  ERROR |  Name is equal to Value: 'NAME' |  |
|  M308 |  ERROR |  Number of `<dimIndex>` Elements 'NUM' is different to number of `<dim>` instances 'NUM2' |  |
|  M309 |  ERROR |  Field 'NAME': Offset error: 'NUM' |  |
|  M310 |  ERROR |  Field 'NAME': BitWidth error: 'NUM' |  |
|  M311 |  ERROR |  Field 'NAME': Calculation: MSB or LSB == -1 |  |
|  M312 |  ERROR |  Address Block missing for Peripheral 'NAME' |  |
|  M313 |  ERROR |  Field 'NAME': LSB > MSB: BitWith calculates to 'NUM' |  |
|  M314 |  ERROR |  Address Block: `<offset>` or `<size>` not set. |  |
|  M315 |  ERROR |  Address Block: `<size>` is zero. |  |
|  M316 |  ERROR |  'LEVEL' `<name>` not set. |  |
|  M317 |  WARNING |  'LEVEL' `<description>` not set. |  |
|  M318 |  WARNING |  'LEVEL' 'NAME' `<'TAG'>` is equal to `<name>` |  |
|  M319 |  WARNING |  'LEVEL' `<'TAG'>` 'NAME' ends with newline, is this intended? |  |
|  M320 |  WARNING |  'LEVEL' `<description>` 'NAME' is not very descriptive |  |
|  M321 |  WARNING |  'LEVEL' `<'ITEM'>` 'NAME' starts with '_', is this intended? |  |
|  M322 |  ERROR |  'LEVEL' 'ITEM' 'NAME' is meaningless text. Deleted. |  |
|  M323 |  WARNING |  'LEVEL' `<'ITEM'>` 'NAME' contains text 'TEXT' |  |
|  M324 |  ERROR |  Field 'NAME' 'BITRANGE' does not fit into Register 'NAME2':'NUM' 'LINE' |  |
|  M325 |  ERROR |  CPU Revision is not set" |  |
|  M326 |  ERROR |  Endianess is not set, using default (little) |  |
|  M327 |  ERROR |  NVIC Prio Bits not set or wrong value, must be 2..8. Using default (4) |  |
|  M328 |  WARNING |  'LEVEL' 'NAME' has no Registers, ignoring 'LEVEL'. |  |
|  M329 |  ERROR |  CPU Type is not set, using default (Cortex-M3) |  |
|  M330 |  ERROR |  Interrupt 'NAME' Number not set. |  |
|  M331 |  ERROR |  Interrupt 'NAME' Number 'NUM' greater 239. |  |
|  M332 |  WARNING |  'LEVEL' 'NAME' has only one Register. |  |
|  M333 |  ERROR |  Duplicate `<enumeratedValue>` 'NUM': 'NAME' ('USAGE'), already used by 'NAME2' ('USAGE2') 'LINE' |  |
|  M334 |  WARNING |  'LEVEL' `<'ITEM'>` 'NAME' is very long, use `<description>` and a shorter `<name>` |  |
|  M335 |  ERROR |  Value 'NAME':'NUM' does not fit into field 'NAME2' 'BITRANGE'. |  |
|  M336 |  ERROR |  'LEVEL' 'NAME' already defined 'LINE' |  |
|  M337 |  ERROR |  'LEVEL' 'NAME' already defined 'LINE' |  |
|  M338 |  ERROR |  Field 'NAME' 'BITRANGE' ('ACCESS') overlaps 'NAME2' 'BITRANGE2' ('ACCESS2') 'LINE' |  |
|  M339 |  ERROR |  Register 'NAME' ('ACCESS') (@'ADDRSIZE') has same address or overlaps 'NAME2' ('ACCESS2') (@'ADDRSIZE2') 'LINE' |  |
|  M340 |  ERROR |  No Devices found. |  |
|  M341 |  ERROR |  More than one devices found, only one is allowed per SVD File. |  |
|  M342 |  ERROR |  Dim-extended 'LEVEL' 'NAME' must not have `<headerStructName>` |  |
|  M343 |  ERROR |  'LEVEL' 'NAME' (@'ADDR') has same address as 'NAME2' 'LINE' |  |
|  M344 |  ERROR |  Register 'NAME' (@'ADDRSIZE') is outside or does not fit any `<addressBlock>` specified for Peripheral 'NAME2' 'TEXT' |  |
|  M345 |  ERROR |  Field 'NAME' 'BITRANGE' does not fit into Register 'NAME2':'NUM' |  |
|  M346 |  WARNING |  Register 'NAME' (@'ADDR') offset is equal or is greater than it's Peripheral base address 'NAME2' (@'ADDR2'), is this intended? |  |
|  M347 |  WARNING |  Field 'NAME' (width < 6Bit) without any `<enumeratedValue>` found. |  |
|  M348 |  ERROR |  Alternate 'LEVEL' 'NAME' does not exist at 'LEVEL' address (@'ADDR') |  |
|  M349 |  ERROR |  Alternate 'LEVEL' 'NAME' is equal to 'LEVEL' name 'NAME2' |  |
|  M350 |  WARNING |  Peripheral 'NAME' (@'ADDR') is not 4Byte-aligned. |  |
|  M351 |  WARNING |  Peripheral 'TYPE' 'NAME' is equal to Peripheral name. |  |
|  M352 |  WARNING |  AddressBlock of Peripheral 'NAME' (@'ADDR') 'TEXT' overlaps 'NAME2' (@'ADDR2') 'TEXT2' 'LINE' |  |
|  M353 |  WARNING |  Peripheral group name 'NAME' should not end with '_' |  |
|  M354 |  ERROR |  Interrupt ''NUM':'NAME' specifies a Core Interrupt. Core Interrupts must not be defined, they are set through `<cpu><name>`. |  |
|  M355 |  ERROR |  No Interrupts found on pos. 0..15. External (Vendor-)Interrupts possibly defined on position 16+. External Interrupts must start on position 0 |  |
|  M356 |  WARNING |  No Interrupt definitions found. |  |
|  M357 |  ERROR |  Core Interrupts found. Interrupt Numbers are wrong. Internal Interrupts must not be described, External Interrupts must start at 0. |  |
|  M358 |  ERROR |  AddressBlock of Peripheral 'NAME' 'TEXT' overlaps AddressBlock 'TEXT2' in same peripheral 'LINE' |  |
|  M359 |  ERROR |  Address Block: `<usage>` not set. |  |
|  M360 |  ERROR |  Address Block: found `<'TAG'>` ('HEXNUM') > 'HEXNUM2'. |  |
|  M361 |  ERROR |  'LEVEL' 'ITEM' 'NAME': 'RESERVED' items must not be defined. |  |
|  M362 |  WARNING |  'LEVEL' 'ITEM' 'NAME': 'RESERVED' items must not be defined. |  |
|  M363 |  ERROR |  CPU: `<sauNumRegions>` not set. |  |
|  M364 |  ERROR |  CPU: `<sauNumRegions>` value 'NUM' greater than SAU max num ('NUM2') |  |
|  M365 |  WARNING |  Register 'NAME' ('ACCESS') (@'ADDRSIZE') has same address or overlaps 'NAME2' ('ACCESS2') (@'ADDRSIZE2') 'LINE' |  |
|  M366 |  ERROR |  Register 'NAME' size ('NUM'Bit) is greater than `<dimIncrement>` * `<addressBitsUnits>` ('NUM2'Bit). |  |
|  M367 |  WARNING |  Access Type: Field 'NAME' ('ACCESS') does not match Register 'NAME2' ('ACCESS2') |  |
|  M368 |  WARNING |  'LEVEL' 'NAME' (@'ADDR') has same address as 'NAME2' 'LINE' |  |
|  M369 |  ERROR |  Enumerated Value 'NAME': `<value>` not set. |  |
|  M370 |  ERROR |  'LEVEL' 'NAME': `<offset>` not set. |  |
|  M371 |  ERROR |  'LEVEL' 'NAME' `<headerStructName>` is equal to hierarchical name |  |
|  M372 |  ERROR |  'LEVEL' `<'TAG'>` 'NAME' already defined 'LINE' |  |
|  M373 |  ERROR |  'LEVEL' `<'TAG'>` 'NAME' already defined 'LINE' |  |
|  M374 |  WARNING |  `<enumeratedValues>` can be one `<enumeratedValues>` container for all `<enumeratedValue>`s, where `<usage>` can be read, write, or read-write or two `<enumeratedValues>` containers, where one is set to `<usage>` read and the other is set to `<usage>` write |  |
|  M375 |  ERROR |  'LEVEL' 'NAME' (`<enumeratedValues>` 'NAME2'): Too many `<enumeratedValues>` container specified. |  |
|  M376 |  ERROR |  'LEVEL' 'NAME' (`<enumeratedValues>` 'NAME2'): 'USAGE' container already defined in 'LINE'. |  |
|  M377 |  ERROR |  'LEVEL' 'NAME' (`<enumeratedValues>` 'NAME2'): 'USAGE' container conflicts with 'NAME3' 'LINE'. |  |
|  M378 |  ERROR |  Register Array: Register 'NAME' size ('NUM'Bit) does not match `<dimIncrement>` ('NUM2'Bit). |  |
|  M379 |  ERROR |  XBin Number 'NAME' too large, skipping evaluation. |  |
|  M380 |  ERROR |  AddressBlock of Peripheral 'NAME' (@'ADDR') 'TEXT' does not fit into 32Bit Address Space. |  |
|  M381 |  ERROR |  Interrupt 'NAME' Number 'NUM' greater or equal deviceNumInterrupts ('NUM2'). |  |
|  M382 |  ERROR |  'LEVEL' 'NAME': 'NAME2' 'HEXNUM' does not fit into 'LEVEL' width: 'NUM' Bit. |  |

### Data modification errors

| Message Number |  Type   |  Message Text |  Action|
|----------------|---------|---------------|--------|
| M517           | WARNING | SFD Code generation: Forbidden Trigraph '??CHAR' found in 'NAME'. |  |
| M516           | WARNING | SFD Code generation: Unsupported character found in 'NAME' : 'HEX'. |  |
| M518           | WARNING | SFD Code generation: Unsupported ESC sequence found in 'NAME' : 'CHAR'. |  |

<!-- markdownlint-restore -->
