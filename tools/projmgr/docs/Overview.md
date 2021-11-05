# [DRAFT] Overview

![Overview](./images/Overview.png "Overview")

The CMSIS Project Manager essentially uses **Project Files** and **CMSIS-Packs** to create self-contained
CMSIS-Build input files.

| Project Files     | Description
|:------------------|:---------------------------------------------------------------------------------
| `*.csolution.yml` | Container for related projects.
| `*.cproject.yml`  | Build description of an independant image.
| `*.clayer.yml`    | Set of source files along with pre-configured components for reuse in different context.
| `*.csettings.yml` | Default setup for project creation.

## Project Examples

### Minimal Project Setup

Simple applications require just one self-contained.

**Simple Project: `Sample.cproject.yml`**
```yml
default:
  target:
    device: LPC55S69
  optimize: size
  debug: on

groups:
  - group: My files
    files:
      - file: main.c

  - group: HAL
    files:
      - file: .\hal\driver1.c

components:
  - component: Device:Startup
```


**Flexible Builds: `Multi.cproject.yml`**

Complex examples require frequently slightly different targets and or modifications during build, i.e. for testing.

It adds target-types and build-types.  After processing it with the CMSIS Project Manager the *.cprj output files have the following naming conventions:
`<projectname>.<build-type>[~target-type].cprj`    --> Example: `Multi.Debug~Production-HW.cprj`


```yml
target-types:
  - type: Board
    target: 
      board: NUCLEO-L552ZE-Q

  - type: Production-HW
    target:
      device: STM32U5X          # specifies device

  - type: Virtual
    target:
      board: VHT-Corstone-300   # FVP platform (appears as board)
      
build-types:
  - type: Debug
    optimize: debug
    debug: on

  - type: Test
    optimize: max
    debug: on

  - type: Release
    optimize: max
    debug: off

groups:
  - group: My group1
    files:
      - file: file1a.c
      - file: file1b.c
      - file: file1c.c

  - group: My group2
      - file: file2a.c

  - group: Test-Interface
    include: .Test
      - file: fileTa.c

layers:
  - layer: NUCLEO-L552ZE-Q\Board.clayer.yml   # Need to find a better way: $Board$.clayer.yml
    include: ~Board

  - layer: Production.clayer.yml
    include: ~Production-HW

  - layer: Corstone-300\Board.clayer.yml
    include: ~VHT-Corstone-300
```

**Related Projects `iot-product.csolution.yml`**

```yml
target-types:
  - type: Board
    target: 
      board: NUCLEO-L552ZE-Q

  - type: Production-HW
    target:
      device: STM32U5X          # specifies device
      
build-types:
  - type: Debug
    optimize: debug
    debug: on

  - type: Test
    optimize: max
    debug: on
    
solution:
  - project: \security\TFM.cproject.yml
    type: .Release
  - project: \application\MQTT_AWS.cproject.yml
  - project: \bootloader\Bootloader.cproject.yml
    exclude: ~Virtual
```

