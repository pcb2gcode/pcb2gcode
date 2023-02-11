# pcb2gcode [![Build Status](https://github.com/pcb2gcode/pcb2gcode/workflows/CI/badge.svg)](https://github.com/pcb2gcode/pcb2gcode/actions) [![Coverage Status](https://coveralls.io/repos/github/pcb2gcode/pcb2gcode/badge.svg?branch=master)](https://coveralls.io/github/pcb2gcode/pcb2gcode?branch=master) [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://payments.wikimedia.org/index.php?title=Special:PaypalExpressGateway&appeal=JimmyQuote&ffname=paypal_ec&recurring=&currency=USD&amount=0&payment_method=paypal&uselang=en&utm_medium=Waystogive&utm_campaign=C11_Waystogive&utm_source=Waystogive)

pcb2gcode is a command-line software for the isolation, routing and drilling of PCBs.
It takes Gerber files as input and it outputs gcode files, suitable for the milling of PCBs.
It also includes an Autoleveller, useful for the automatic dynamic calibration of the milling depth.

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
    yum install automake autoconf libtool boost-devel gtkmm24-devel gerbv-devel
    exit
    ./configure
    make
    su -c 'make install'
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
If you want to install the latest version from git you'll need the autotools, Boost with the program_options library
(dev, >= 1.56), gtkmm2.4 (dev) and libgerbv (dev).

Unfortunately pcb2gcode requires a rather new version of Boost (1.56), often not included in the oldest distros (like Ubuntu < 15.10 or Debian Stable).
Moreover Boost 1.56 sometimes freezes pcb2gcode, while Boost 1.59, 1.60 and 1.61 are affected by a [program options bug](https://svn.boost.org/trac/boost/ticket/11905).
You can [download](http://www.boost.org/users/download/) a working version of Boost ([1.57](http://www.boost.org/users/history/version_1_57_0.html) and [1.58](http://www.boost.org/users/history/version_1_58_0.html) work well) and build it manually with:

    $ ./bootstrap.sh --with-libraries=program_options --prefix=<somewhere>
    $ ./b2 variant=release link=static
    $ ./b2 install

Then add `--with-boost=<boost install directory> --enable-static-boost` to the `./configure` command.

To build with coverage outputs, add `--enable-code-coverage` to `./configure` and then later run `make check-code-coverage` to run unit tests to collect coverage.  The last line of the output will include a URL to view the coverage.

Ubuntu 12.04 does not include gcc 4.8 (needed for the C++11 support); you can install it with:

    $ sudo apt-get update
    $ sudo apt-get install software-properties-common python-software-properties
    $ sudo add-apt-repository "ppa:ubuntu-toolchain-r/test"
    $ sudo apt-get update
    $ sudo apt-get install g++-4.8
    $ export CXX=g++-4.8

#### Debian Testing or newer, Ubuntu Wily or newer<a name="debianlike"></a>

    $ sudo apt-get update
    $ sudo apt-get install build-essential automake autoconf autoconf-archive libtool libboost-program-options-dev libgtkmm-2.4-dev gerbv git librsvg2-dev
    $ git clone https://github.com/pcb2gcode/pcb2gcode.git
    $ cd pcb2gcode

Then follow the [common build steps](#commonbuild)

#### Fedora

    su
    <the root password>
    yum groupinstall "Development Tools"
    yum install automake autoconf libtool boost-devel gtkmm24-devel gerbv-devel git
    exit

Then follow the [common build steps](#commonbuild)

#### Common build steps<a name="commonbuild"></a>

    $ autoreconf -fvi
    $ ./configure
    $ make
    $ sudo make install

### Windows
You can build pcb2gcode for Windows with MSYS2 (https://www.msys2.org/).
Download MSYS2 and install it somewhere, then run "MSYS MINGW64" (if you want a 64-bit x86_64 binary) or "MSYS MINGW32" (if you want a 32-bit i686 binary).

The following commands are for the x86_64 binary, if you want the i686 binary replace all the "/mingw64" with "/mingw32" and all the mingw-w64-x86_64-* packages with mingw-w64-i686-*

    $ pacman -Sy
    $ pacman --needed -S bash pacman pacman-mirrors msys2-runtime

Close and reopen the shell

    $ pacman -Su  # This may also close the shell; if so, reopen it.
    $ pacman --needed -S base-devel git mingw-w64-x86_64-gcc mingw-w64-x86_64-boost mingw-w64-x86_64-gtkmm libtool mingw-w64-x86_64-autotool

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
    $ autoreconf -fvi
    $ ./configure
    $ make LDFLAGS='-s'

The dynamically linked binary is &lt;msys2 installation folder&gt;/home/&lt;user&gt;/pcb2gcode/.libs/pcb2gcode.exe.
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
