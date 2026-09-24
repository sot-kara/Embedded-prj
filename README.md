# Repository for Embedded Systems class project

## Requirements
To build this projects you need to install a few dependencies
- libwebsockets for c
- cjson (libcjson-dev)
- matplotlib, pandas and numpy for plotting with python
- cmake for building the project

## Crosscompiling
Crosscompiling is done through this toolchain https://sourceforge.net/projects/raspberry-pi-cross-compilers/. Choose your prefered cross-compiler. In this project the 64 bit compiler for bookwork with gcc 13.3 is used.

### IMPORTANT NOTE!
    You should follow the instructions on how to install the cross-compiler from their github and how to establish a sysroot. For this project the sysroot directory is named rpi-sysroot set up in the home directory, in rpi-sysroot/rootfs is the actual raspberry pi sysroot. The compiler is setup at rpi-toolchain. (Also in rpi-sysroot/tools just to follow the cmake tutorial)
Should you wish to install the compiler and sysroot differently, please change the RPI_SYSROOT and RPI_TOOLCHAIN variables in the cmake/raspberrypi-aarch64.cmake file.

Here are the links for installation and sysroot tutorial:

Wiki of toolchain: https://github.com/abhiTronix/raspberry-pi-cross-compilers/wiki

Sysroot: https://github.com/abhiTronix/raspberry-pi-cross-compilers/wiki/Cross-Compiler-CMake-Usage-Guide-with-rsynced-Raspberry-Pi-64-bit-OS#cross-compiler-cmake-usage-guide-with-rsynced-raspberry-pi-64-bit-os


## Building
If you want to build the project for your native machine just run build_native.sh (dont forget to chmod it).

If you want to build for your raspberry pi (4 in this case) run bake_pi.sh.

One executable Embedded_prj is produced each time in the build directory.

## Running
You can run the executable on its own, in which case the app will run for a default amount of time (you can change this in main.c editing the macro SLEEP_SECONDS).

If you want to specify a start and end time, write the unix timestamp equivalent in two lines (first the start and then the end time) in schedule.txt.

For example:
```
1790260500
1790260600
``` 
Then please edit the SCHEDULE_PATH macro to point to the full path of the schedule.txt. The application will not start actually receiving and categorizing messages until the start time, and will deactivate on the end time.

## Output and plotting

The output will be in metrics_log.txt with the suggested .csv format.

You can plot the data by running makeplot.py .