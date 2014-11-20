This is a fork of `git://pcb2gcode.git.sourceforge.net/gitroot/pcb2gcode/pcb2gcode` by Patrick Kanzler (patkan).
I forked the project since the original is not under active development anymore.
At the moment I do not have that much time to look into the code and tend to issues, but I will try to merge pull requests.

------------------------------------------------------------------------------------

changes by "Corna" (Nicola Corna nicola@corna.info)
- added bridge height option

changes by "erik74"
Changes to original:
- additional option "optimise" reduces the g-code file size up to 40%.
- additional option "bridges" adds four bridges to the outline cut.
- additional option "g64" to manually set the g64 value.
- additional option "metric output".
- additional option "cut front" for outline cutting from top layer.
- additional option "onedrill" to skip the tool change when drilling.
- reformatted the code with Eclipse code styler (K&R).
- started documenting the code.

ToDo:
- The Milldrill option does not work with metricoutput yet.
- Why is no front+back side generated if a single pcb2gcode call is used?

## ORIGINAL README:

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

For further details, see INSTALL.
