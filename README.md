This is a complete rewrite of the original pcb2gcode in C++.

### Quick Installation
This development version of pcb2gcode does not get into repositories of distros. If you want to test this version, you will have to go to the section below (installation from GIT).

#### Archlinux:
pcb2gcode stable 1.1.3 -> `https://aur.archlinux.org/packages.php?ID=50457`
pcb2gcode git 1.1.4 -> `https://aur.archlinux.org/packages.php?ID=55198`

#### Fedora:
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
There are pcb2gcode packages in the official repositories.
The Ubuntu ones are outdated (1.1.2) as of 26-12-2011, I recommend installing 1.1.4 from source.

* Open a terminal and cd to the extracted tarball
* type the following:
    
    ```
    sudo apt-get install build-essential automake autoconf libtool libboost-all-dev libgtkmm-2.4-dev gerbv
    <your own password>
    ./configure
    make
    sudo make install
    ```
    
* done.


### Installation from GIT (latest development version):

```
$ git clone https://github.com/patkan/pcb2gcode.git
$ cd pcb2gcode
$ ./git-build.sh
$ sudo make install
```

### Windows
Building pcb2gcode in Windows isn't easy, you should download a precompiled release.
If you really want to build it by yourself you'll need MinGW and a MSYS environment.
You can find a good guide here http://ingar.satgnu.net/devenv/mingw32/index.html (follow
the steps 1, 2 and 3, or follow the step 1 and download the precompiled packages here
http://ingar.satgnu.net/devenv/mingw32/base.html#build)

For further details, see INSTALL.
