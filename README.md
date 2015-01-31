This is a complete rewrite of the original pcb2gcode in C++.

### Quick Installation
This development version of pcb2gcode does not get into repositories of distros. If you want to test this version, you will have to go to the section below (installation from GIT).

#### Archlinux:
* pcb2gcode stable 1.1.3 -> `https://aur.archlinux.org/packages.php?ID=50457`
* pcb2gcode git 1.1.4 -> `https://aur.archlinux.org/packages.php?ID=55198`

#### Fedora:
* Download the latest tarball from https://sourceforge.net/projects/pcb2gcode/files/pcb2gcode/
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

Unfortunately, these packages are outdated (as 31/1/2015). If you want to download the latest development
version, go to "Installation from GIT"

### Installation from GIT (latest development version):
If you want to install the latest version from git you'll need the autotools, boost program options library
(dev), gtkmm2.4 (dev) and libgerbv (dev). You can download them from your repositories; if you use
Debian/Ubuntu type

    $ sudo apt-get install build-essential automake autoconf libtool libboost-program-options-dev libgtkmm-2.4-dev gerbv

or, if you use Fedora

    su
    <the root password>
    yum groupinstall "Development Tools"
    yum install automake autoconf libtool boost-devel gtkmm24-devel gerbv-devel
    exit

Then you can download it from git, build and install it

    $ git clone git://git.code.sf.net/p/pcb2gcode/code pcb2gcode-code
    $ cd pcb2gcode
    $ ./git-build.sh
    $ sudo make install

### Windows
Building pcb2gcode in Windows isn't easy, you should download a precompiled release.
If you really want to build it by yourself you'll need MinGW and a MSYS environment.
You can find a good guide here http://ingar.satgnu.net/devenv/mingw32/index.html (follow
the steps 1, 2 and 3, or follow the step 1 and download the precompiled packages here
http://ingar.satgnu.net/devenv/mingw32/base.html#build)

For further details, see INSTALL.
