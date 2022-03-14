# cpackget: Pack Installer

**Table of Contents**
- [cpackget: Pack Installer](#cpackget-pack-installer)
  - [Usage](#usage)
    - [Specify the working pack root folder](#specify-the-working-pack-root-folder)
    - [Adding packs](#adding-packs)
    - [Accepting the End User License Agreement (EULA) from the command line](#accepting-the-end-user-license-agreement-eula-from-the-command-line)
    - [Removing packs](#removing-packs)


## Usage

This `cpackget` utility allows to install (or uninstall) software packs to the local development environment.

```txt
Usage:
  cpackget [command] [flags]

Available Commands:
  help        Help about any command
  index       Updates public index
  init        Initializes a pack root folder
  pack        Adds/Removes Open-CMSIS-Pack packages
  pdsc        Adds/Removes Open-CMSIS-Pack packages in the local file system via PDSC files.

Flags:
  -h, --help               help for cpackget
  -R, --pack-root string   Specifies pack root folder. Defaults to CMSIS_PACK_ROOT environment variable
  -q, --quiet              Run cpackget silently, printing only error messages
  -v, --verbose            Sets verboseness level: None (Errors + Info + Warnings), -v (all + Debugging). Specify "-q" for no messages
  -V, --version            Prints the version number of cpackget and exit

Use "cpackget [command] --help" for more information about a command.
```

For example, if one wanted help removing a pack, running `cpackget pack rm --help` would print out useful information on the subject.

### Specify the working pack root folder

If cpackget is going to work on an existing pack root folder, there are two ways to specify it:

1. `export CMSIS_PACK_ROOT=path/to/pack-root; cpackget pack add ARM.CMSIS`
2. `cpackget --pack-root path/to/pack-root pack add ARM.CMSIS`

To create a new pack root folder with an up-to-date index file of publicly available Open-CMSIS-Pack packs run:

```
$ cpackget init --pack-root path/to/new/pack-root https://keil.com/pack/index.pidx
```

The command will create a folder called `path/to/new/pack-root` and the following subfolders: `.Download`, `.Local`, `.Web`.
A copy of the index file (if specified) is placed in `.Web/index.pidx`.

If later it is needed to update the public index file, just run `cpackget index https://vendor.com/index.pidx` and
`.Web/index.pidx` will be updated accordingly.


### Adding packs

The commands below demonstrate how to add packs:

Install a pack version that is present in the file system already:
* `cpackget pack add path/to/Vendor.PackName.x.y.z.pack`

Install a pack version that can be downloaded using a web link:
* `cpackget pack add https://vendor.com/example/Vendor.PackName.x.y.z.pack`

Install a pack version from the public package index. The download url will be looked up by the tool:
* `cpackget pack add Vendor.PackName.x.y.z`

Install the latest published version of a public package listed in the package index:
* `cpackget pack add Vendor.PackName`

Install the pack versions specified in the ascii file. Each line specifies a single pack.
* `cpackget pack add -f list-of-packs.txt`

The command below is an example how to add packs via PDSC files:
* `cpackget pdsc add path/to/Vendor.PackName.pdsc`

Note that for adding packs via PDSC files is not possible to provide an URL as input. Only local files are allowed.

### Accepting the End User License Agreement (EULA) from the command line

Some packs come with licenses and by default cpackget will prompt the user for agreement. This can be avoided
by using the `--agree-embedded-license` flag:

* `cpackget pack add --agree-embedded-license Vendor.PackName`

Also there are cases where users might want to only extract the pack's license and not install it:

* `cpackget pack add --extract-embedded-license Vendor.PackName`

The extracted license file will be placed next to the pack's. For example if Vendor.PackName.x.y.z had a licese file
named `LICENSE.txt`, cpackget would extract it to `.Download/Vendor.PackName.x.y.z.LICENSE.txt`.

### Removing packs

The commands below demonstrate how to remove packs.

Remove pack `Vendor.PackName` version `x.y.z` only, leave others untouched
* `cpackget pack rm Vendor.PackName.x.y.z`

Remove all versions of pack `Vendor.PackName`
* `cpackget pack rm Vendor.PackName`

Same as above, except that now it also removes the cached pack file.
* `cpackget pack rm --purge Vendor.PackName`: using `--purge` triggers removal of any downloaded files.

And for removing packs that were installed via PDSC files, consider the example commands below:

Remove pack `Vendor.PackName` version `x.y.z` only, from the local packs.
* `cpackget pdsc rm Vendor.PackName.x.y.z`

Remove all versions of pack `Vendor.PackName`, from the local packs.
* `cpackget pdsc rm Vendor.PackName`

Note that removing packs does not require pack of PDSC file location specification, e.g. no need to provide the path for the PDSC file or the URL of the pack.

