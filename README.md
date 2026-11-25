# pcb2gcode [![Build Status](https://github.com/pcb2gcode/pcb2gcode/workflows/CI/badge.svg)](https://github.com/pcb2gcode/pcb2gcode/actions) [![Coverage Status](https://coveralls.io/repos/github/pcb2gcode/pcb2gcode/badge.svg?branch=master)](https://coveralls.io/github/pcb2gcode/pcb2gcode?branch=master) [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://payments.wikimedia.org/index.php?title=Special:PaypalExpressGateway&appeal=JimmyQuote&ffname=paypal_ec&recurring=&currency=USD&amount=0&payment_method=paypal&uselang=en&utm_medium=Waystogive&utm_campaign=C11_Waystogive&utm_source=Waystogive)

pcb2gcode is a command-line software for the isolation, routing and drilling of PCBs.
It takes Gerber files as input and it outputs gcode files, suitable for the milling of PCBs.
It also includes an Autoleveller, useful for the automatic dynamic calibration of the milling depth.

It includes native **Model Context Protocol (MCP)** server capability (`--mcp`), allowing AI assistants like Claude Desktop, Cursor, and Antigravity to directly execute PCB isolation and routing.

## Documentation

The [pcb2gcode wiki](https://github.com/pcb2gcode/pcb2gcode/wiki) explains the options, with pictures and sample configurations:

* [Generic](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Generic): the `millproject` file, units, and other general options
* [Common](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Common): CNC options shared by milling, drilling, and cutting, such as `--zsafe` and tiling
* [Milling](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Milling): isolation milling from Gerber files, including `--voronoi`
* [Drilling](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Drilling): drilling and milldrilling from Excellon files
* [Outline](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Outline): cutting the board out along its edge, including bridges
* [Optimizations](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Optimizations): tool path optimizations, and making pcb2gcode run faster
* [Alignment for two-sided PCBs and offsets](https://github.com/pcb2gcode/pcb2gcode/wiki/Options:-Alignment-for-two-sided-PCB-and-offsets): alignment pins, `--zero-start`, `--mirror-axis`, and offsets

`pcb2gcode --help` lists every option.

## Output manifest

`--manifest=FILE` writes a JSON list of every file that the run wrote, for GUIs and scripts that would otherwise have to guess names like `outp3_masked_front.svg`. A relative `FILE` goes in `--output-dir`, like the G-code. The manifest is only written when the run succeeds.

```json
{
  "format_version": 1,
  "files": [
    {"path": "back.ngc", "type": "gcode", "layer": "back",
     "tools": [{"number": 1, "diameter_mm": 3.0}, {"number": 2, "diameter_mm": 0.2}]},
    {"path": "outp2_masked_back.svg", "type": "svg", "layer": "back", "stage": "masked"},
    {"path": "traced_back_final_1.svg", "type": "svg", "layer": "back", "stage": "traced",
     "final": true, "tool_index": 1, "tool_diameter_mm": 0.2}
  ]
}
```

* `path` is relative to the directory of the manifest, or absolute for a file outside of it. The files are sorted by `path`.
* `layer` is `front`, `back`, `outline`, `drill` or `milldrill`.
* G-code files list their `tools` in the order that they are used, with the T number and the diameter in mm.
* The `stage` of an SVG is `original` (the input), `masked` (cut to the outline), `processed` (Voronoi regions and toolpaths), `traced` (just the toolpaths) or `contentions` (where the clearance couldn't be kept). Images of toolpaths also have the `tool_index` (from 0, in `--mill-diameters` order), the `tool_diameter_mm` and `final`, which is true for the toolpaths after post-processing, as they are in the G-code.
* `format_version` only changes when a reader could break. Ignore any keys that you don't know.

With `--no-export`, toolpaths aren't computed, so only the `original` and `masked` images are written.

## Model Context Protocol (MCP) Server

`pcb2gcode` includes a native Model Context Protocol (MCP) server mode over stdio (`--mcp`). This enables AI coding assistants (such as Antigravity, Cursor, and Claude Desktop) to invoke `pcb2gcode` directly for PCB isolation and routing.

### Quick Setup (Ubuntu / WSL)

Run the setup script inside your Ubuntu or WSL terminal. It automatically installs dependencies, applies Ubuntu 24.04 workarounds, compiles `pcb2gcode`, and generates `mcp-config.json` in the project root:

```bash
chmod +x setup-ubuntu.sh
./setup-ubuntu.sh
```

### MCP Client Configuration

Copy the contents of the generated `mcp-config.json` file into your MCP client configuration (e.g., `mcp.json` or `claude_desktop_config.json`).

**Example (WSL environment calling from Windows):**
```json
{
  "mcpServers": {
    "pcb2gcode": {
      "command": "wsl.exe",
      "args": [
        "-e",
        "/home/user/pcb2gcode/build/pcb2gcode",
        "--mcp"
      ]
    }
  }
}
```

**Example (Native Linux environment):**
```json
{
  "mcpServers": {
    "pcb2gcode": {
      "command": "/home/user/pcb2gcode/build/pcb2gcode",
      "args": [
        "--mcp"
      ]
    }
  }
}
```

## Option metadata for GUIs (`--dump-options`)

`pcb2gcode --dump-options --noconfigfile` prints every option as JSON, so that a GUI can build its forms from the installed pcb2gcode instead of keeping its own copy of the options. It exits without checking options or reading input files. Pass `--noconfigfile` so that a `millproject` in the working directory can't break it; the output depends only on the binary. A pcb2gcode without the flag exits with code 101 (unknown option).

```json
{
  "format_version": 1,
  "program": "pcb2gcode",
  "version": "3.2.0",
  "git_commit": "v3.2.0-1-gabcdef0",
  "types": {"length": {"bare_unit": "in", "metric_bare_unit": "mm", "units": [{"name": "mm", "aliases": ["millimeter", "millimeters"], "factor": 0.001}, ...]}, ...},
  "conditions": {"milling": {"description": "A front or back gerber file is given.", "expression": {"any": [{"set": "front"}, {"set": "back"}]}}, ...},
  "groups": [{"id": "cli", "title": "command line only options"}, ...],
  "options": [{"name": "zwork", "group": "milling", "description": "...", "type": "length", "default": null, "example": "-0.05mm", "required_when": "milling", "used_when": "milling"}, ...],
  "rules": [{"kind": "greater", "options": ["zsafe", "zwork"], "when": "milling", "code": 15, "message": "--zsafe must be higher than --zwork."}, ...],
  "exit_codes": [{"code": 0, "name": "ERR_OK"}, ...]
}
```

* `format_version` changes only when the format changes incompatibly. New keys, types, rule kinds and expression nodes can appear without a change, so a reader should cope with them: ignore unknown keys, treat options of an unknown type as strings, and skip rules of an unknown kind. Treat an unknown expression node as true in `used_when` and `required_when`, so the option still gets a value, and skip a rule whose `when`, `if` or `unless` contains one. pcb2gcode still checks the rules that a reader skips.
* `types` explains the syntax of each value type: the units that it accepts and what a number without a unit means. Lengths and velocities without a unit are in inches (in/min), or millimeters (mm/min) with `--metric`; times without a unit are in milliseconds; a percentage without `%` is a fraction (0.5 is 50%).
* `groups` and `options` are in `--help` order. Each option has `name`, `group`, `description`, `type`, `default` and `used_when`, and, when they apply:
  * `default`: in pcb2gcode's own syntax (`"0.0001in"`, `"50in/min"`, `"any"`), `null` if there is none, or a list of values for `repeatable` options. `"inf"` means infinity, which pcb2gcode can't read back: leave the option out to get it. `default_note` explains defaults that depend on other options, like `mill-vertfeed`'s "half of --mill-feed".
  * `implicit`: the value of the option without `=value`, like `"true"` for `--voronoi`.
  * `repeatable` (the option can be given several times), `comma_separated` (each value can be a list separated by commas) and `multitoken` (one option can take several values on the command line).
  * `choices` for enums, each with its `aliases`, matched ignoring case.
  * `range`: the limits that pcb2gcode checks, like `{"min": "0", "min_exclusive": true, "max": "100%"}`.
  * `example`, `required_when`: a valid value for an option that pcb2gcode requires while the named condition holds.
  * `bare_unit`, `metric_bare_unit`: the units of plain numbers that are lengths, like `tolerance`.
  * `cli_only` (not allowed in configuration files), `deprecated`, `replaced_by`, `value_map` (how old values map to the replacement; without it the value carries over). A deprecated option has `replaced_by` only when its values convert like that. `extra-passes` has none: it still works, and pcb2gcode mills whichever needs more passes, it or `--isolation-width`. With a tool of diameter d and an overlap of o, n extra passes cover an isolation width of d + n × (d − o).
  * `path` (`input_file`, `output_file` or `directory`), `file_format` (`gerber`, `excellon`, `gcode`, `text`, `millproject`, or `json` for `--manifest`), and `relative_to` for output files, which are inside `--output-dir`.
* `conditions` are the named conditions that `used_when`, `required_when` and the rules refer to. An expression is `true`, `{"set": option}` (it has a value, maybe its default), `{"given": option}` (given on the command line or in a configuration file), `{"equal": [option, value]}`, `{"finite": option}` (set and not `inf`), `{"not": expression}`, `{"any": [expressions]}` or `{"all": [expressions]}`.
* `rules` are the checks that pcb2gcode runs on the options before it does any work, in order, with the exact error message and exit code. pcb2gcode reports every rule that fails and exits with the code of the first. A rule applies only while its `when` condition holds. Kinds: `required` (the option must have a value), `range` (the value must be within `min` and `max`), `greater` and `greater_or_equal` (between two options, both set, unless the `unless` expression holds), and `forbidden` (the `if` expression must not hold).
* `exit_codes` lists the exit codes. 1 is also the exit code of unexpected errors, like an `--output-dir` that doesn't exist.

pcb2gcodeGUI, the official GUI for pcb2gcode, is available [here](https://github.com/pcb2gcode/pcb2gcodeGUI).

If you find this project useful, consider [donating money to charity](https://payments.wikimedia.org/index.php?title=Special:PaypalExpressGateway&appeal=JimmyQuote&ffname=paypal_ec&recurring=&currency=USD&amount=0&payment_method=paypal&uselang=en&utm_medium=Waystogive&utm_campaign=C11_Waystogive&utm_source=Waystogive).

## Quick Installation
This development version of pcb2gcode does not get into repositories of distros. If you want to test this version, you will have to go to the section below (installation from GIT).

#### Archlinux:
* pcb2gcode stable -> [`https://aur.archlinux.org/packages/pcb2gcode/`](https://aur.archlinux.org/packages/pcb2gcode/)
* pcb2gcode git -> [`https://aur.archlinux.org/packages/pcb2gcode-git/`](https://aur.archlinux.org/packages/pcb2gcode-git/)

#### Fedora:
* Download the latest tarball from https://github.com/pcb2gcode/pcb2gcode/releases
* Open a terminal and cd to the extracted tarball
* type the following:

    ```
    su
    <the root password>
    yum groupinstall "Development Tools"
    yum install cmake boost-devel gtkmm24-devel gerbv-devel
    exit
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    su -c 'cmake --install build'
	```

* done.

#### Debian, Ubuntu:
There are pcb2gcode packages in the official repositories. You can install the with

    sudo apt-get install pcb2gcode

Unfortunately, these packages are seriously outdated. If you want to download the latest development version, go to "Installation from GIT".

#### Windows
Windows prebuilt binaries (with all the required DLLs) may be available in the [release](https://github.com/pcb2gcode/pcb2gcode/releases) page. Due to difficulties with the CI pipeline, these are not reliably built and may be missing for certain releases. See below for instructions how to build pcb2gcode yourself.

#### Mac OS X
pcb2gcode is available in [Homebrew](http://brew.sh/). To install it open the "Terminal" app and run the following commands; pcb2gcode and the required dependencies will be automatically downloaded and installed:

     $ /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
     $ brew install pcb2gcode

## Installation from GIT (latest development version):
If you want to install the latest version from git you'll need CMake, Boost with the program_options library
(dev, >= 1.56), gtkmm2.4 (dev) and libgerbv (dev).

Unfortunately pcb2gcode requires a rather new version of Boost (1.56), often not included in the oldest distros (like Ubuntu < 15.10 or Debian Stable).
Moreover Boost 1.56 sometimes freezes pcb2gcode, while Boost 1.59, 1.60 and 1.61 are affected by a [program options bug](https://svn.boost.org/trac/boost/ticket/11905).
You can [download](http://www.boost.org/users/download/) a working version of Boost ([1.57](http://www.boost.org/users/history/version_1_57_0.html) and [1.58](http://www.boost.org/users/history/version_1_58_0.html) work well) and build it manually with:

    $ ./bootstrap.sh --with-libraries=program_options --prefix=<somewhere>
    $ ./b2 variant=release link=static
    $ ./b2 install

Then configure CMake with `-DBOOST_ROOT=<boost install directory> -DPCB2GCODE_ENABLE_STATIC_BOOST=ON`.

To build with coverage outputs, configure with `-DPCB2GCODE_ENABLE_CODE_COVERAGE=ON` and run `ctest --test-dir build --output-on-failure`.

Ubuntu 12.04 does not include gcc 4.8 (needed for the C++11 support); you can install it with:

    $ sudo apt-get update
    $ sudo apt-get install software-properties-common python-software-properties
    $ sudo add-apt-repository "ppa:ubuntu-toolchain-r/test"
    $ sudo apt-get update
    $ sudo apt-get install g++-4.8
    $ export CXX=g++-4.8

#### Debian Testing or newer, Ubuntu Wily or newer<a name="debianlike"></a>

    $ sudo apt-get update
    $ sudo apt-get install build-essential cmake libboost-program-options-dev libgtkmm-2.4-dev gerbv git librsvg2-dev
    $ git clone https://github.com/pcb2gcode/pcb2gcode.git
    $ cd pcb2gcode

Then follow the [common build steps](#commonbuild)

#### Fedora

    su
    <the root password>
    yum groupinstall "Development Tools"
    yum install cmake boost-devel gtkmm24-devel gerbv-devel git
    exit

Then follow the [common build steps](#commonbuild)

#### Common build steps<a name="commonbuild"></a>

    $ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    $ cmake --build build -j$(nproc)
    $ ctest --test-dir build --output-on-failure
    $ sudo cmake --install build

### Windows
You can build pcb2gcode for Windows with MSYS2 (https://www.msys2.org/).
Download MSYS2 and install it somewhere, then run "MSYS MINGW64" (if you want a 64-bit x86_64 binary) or "MSYS MINGW32" (if you want a 32-bit i686 binary).

The following commands are for the x86_64 binary, if you want the i686 binary replace all the "/mingw64" with "/mingw32" and all the mingw-w64-x86_64-* packages with mingw-w64-i686-*

    $ pacman -Sy
    $ pacman --needed -S bash pacman pacman-mirrors msys2-runtime

Close and reopen the shell

    $ pacman -Su  # This may also close the shell; if so, reopen it.
    $ pacman --needed -S base-devel git mingw-w64-x86_64-gcc mingw-w64-x86_64-boost mingw-w64-x86_64-gtkmm mingw-w64-x86_64-cmake

Now let's download, build and install gerbv:

    $ git clone https://github.com/gerbv/gerbv.git
    $ cd gerbv
    $ git checkout v2.9.6  # Optional step. This version is known to work with pcb2gcode 2.5.0.
    $ ./autogen.sh
    $ ./configure --disable-update-desktop-database
    $ make
    $ make install

Finally, download and build pcb2gcode

    $ cd ..
    $ git clone https://github.com/pcb2gcode/pcb2gcode.git
    $ cd pcb2gcode/
    $ cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS='-s'
    $ cmake --build build -j$(nproc)

The dynamically linked binary is &lt;msys2 installation folder&gt;/home/&lt;user&gt;/pcb2gcode/build/pcb2gcode.exe.
You can find all the DLLs in &lt;msys2 installation folder&gt;/mingw32/bin; copy them in the same folder of pcb2gcode.
These steps can be automated with:

    $ mkdir my-pcb2gcode-build
    $ cp .libs/pcb2gcode.exe my-pcb2gcode-build
    $ cd my-pcb2gcode-build
    $ cp $(ldd pcb2gcode.exe  | cut -d'>' -f2 | cut -d'(' -f1 | grep mingw64 | sort -u) .

The required DLLs are:
 * libatk-1.0-0.dll
 * libboost_program_options-mt.dll
 * libbz2-1.dll
 * libcairo-2.dll
 * libcairomm-1.0-1.dll
 * libexpat-1.dll
 * libffi-6.dll
 * libfontconfig-1.dll
 * libfreetype-6.dll
 * libgcc_s_dw2-1.dll (for the i686 binary)
 * libgcc_s_seh-1.dll (for the x86_64 binary)
 * libgdkmm-2.4-1.dll
 * libgdk_pixbuf-2.0-0.dll
 * libgdk-win32-2.0-0.dll
 * libgerbv-1.dll
 * libgio-2.0-0.dll
 * libglib-2.0-0.dll
 * libglibmm-2.4-1.dll
 * libgmodule-2.0-0.dll
 * libgobject-2.0-0.dll
 * libgtk-win32-2.0-0.dll
 * libharfbuzz-0.dll
 * libiconv-2.dll
 * libintl-8.dll
 * libpango-1.0-0.dll
 * libpangocairo-1.0-0.dll
 * libpangoft2-1.0-0.dll
 * libpangomm-1.4-1.dll
 * libpangowin32-1.0-0.dll
 * libpixman-1-0.dll
 * libpng16-16.dll
 * libsigc-2.0-0.dll
 * libstdc++-6.dll
 * libwinpthread-1.dll
 * zlib1.dll

#### Mac OS X
You can build the latest pcb2gcode version with [Homebrew](http://brew.sh). If Homebrew is not installed yet, install it with the following command:

     $ /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

You might need some build tools that typically are not present:

     $ brew install autoconf automake libtool

Then you can download and build the git version with

     $ brew install --HEAD pcb2gcode

or (if pcb2gcode is already installed)

     $ brew upgrade --HEAD pcb2gcode

For further details, see INSTALL.

#### An other docker way: Insolante
You can use the insolante docker image to use this tool throught a web interface.
This project is under active development and need your feedback but give it a try and let us know.

You can find it in [dockerhub](https://hub.docker.com/r/ngargaud/insolante) for arm and x64 architecture.
The current embedded version of pcb2gcode is 2.1.0.
