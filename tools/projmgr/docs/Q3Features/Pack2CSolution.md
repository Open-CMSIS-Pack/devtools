# Proposal: Copy Content from Packs to CSolution projects

This proposal explains how content from software packs are copied into `csolution` based projects.

The copy process applies to:

- `clayer`: a complete `*.clayer.yml` file including configuration, source files, etc.
- `cproject`: a complete `*.cproject.yml` file including configuration, source files, etc.
- `cimage`: a pre-compiled image including

## Pack content defined in the *.PDSC file

/package/csolution element

Define content that is available for `csolution` based projects.

Example:

<!-- markdownlint-capture -->
<!-- markdownlint-disable MD013 -->

```xml
  <csolution>
    <layer   type="Board" name="./Board/MIMXRT1050-EVKB/Board.clayer.yml" directory="./clayers/Board/Basic"/>
    <layer   type="Board" name="./Board/MIMXRT1050-EVKB/Board.clayer.yml" directory="./clayers/Board/WiFi"/>
    <layer   type="Board" name="./Board/MIMXRT1050-EVKB/Board.clayer.yml" directory="./clayers/Board/Ethernet"/>
    <layer   type="Board" name="./Board/MIMXRT1050-EVKB/Board.clayer.yml" directory="./clayers/Board/AudioIO" condition="AudioIO"/>
    <layer   type="Board" name="./Board/MIMXRT1050-EVKB/Board.clayer.yml" directory="./clayers/Board/SensorIn"/>
    <project type="TFM" name="./TFM/TFM.cproject.yml" directory="./STM32U5-DFP/TFM-project" description="Security based on TFM"/>
    <image   type="TFM" name="./TFM/TFM.axf" directory="./STM32U5-DFP/TFM-pre-build" description="Security based on TFM"/>
  </csolution>
```

<!-- markdownlint-restore -->

## References to Layers stored in Packs

In a `csolution` based project the `<csolution>` content of software packs is accessed with a *Access Sequences* using
`{<type>}`.  

**Examples:**

```yml
csolution:

# ... definition of the software packs used; i.e. the DFP contains the TFM implementation for a device

  projects:
    - project: .\MyApp\Application.cproject.yml
    - project: {TFM}
    - image: {TFM}          # alternatively use the pre-build image
```

```yml
project:

# ... content from layer: ./App.clayer.yml

  layers:
   - layer: {Board}
```

The `csolution` manages interface requirements.  If more then one `{<type>}` content is available in the selected
software pack, the interface requirements are used to select a matching content.  If there are more then one content is
available the `csolution` tool requires user interaction.

### Example Board Layer

The directory structure of a board layer in the software pack could be:

```text
./clayers/Board/WiFi/                          # base directory of the WiFi configuration
./clayers/Board/WiFi/Board/MIMXRT1050-EVKB     # clayer.yml and other source files
./clayers/Board/WiFi/RTE                       # component configurations of the Board layer
```

A {Board} layer is copied by the `csolution` tool to the project with this structure:

```text
./RTE                                          # component configurations of the Board layer
./Board/MIMXRT1050-EVKB                        # clayer.yml and other source files
```

## Copy from Pack to CSolution Project

When a content is identified the `csolution` tool:

- Copies the `directory` content specified with the `<csolution>` element of the pack.
- Uses for the `{<type>}` reference in the yml file, the `name` specified with the `<csolution>` element of the pack.
- Protocols the content copy in a `registry.yml` file with the following information:
  - the build-type and target-type that creates the reference.
  - the original pack including version information that contained the content.
  - the `directory` along with the files that where copied.
  - the `name` that is used instead of the `{<type>}` reference.

The `csolution` tool does not copy the content when:

- there is already a content information in the `registry.yml` file
- a file overwritten in the target directory of the `*.csolution.yml` or `*.cproject.yml` project.

### Example

File: `.\MyApp\Application.cproject.registry.yml`

```yml
registry:
  layers:
    - layer: {Board}
      for-type: +MIMXRT1050-Board
      name: ./Board/MIMXRT1050-EVKB/Board.clayer.yml
      pack: NXP::MIMXRT1050-EVKB_BSP
      directory: ./clayers/Board/WiFi
      files:
        - file: ./Board/MIMXRT1050-EVKB/setup.c
        - file: ...
      
```
