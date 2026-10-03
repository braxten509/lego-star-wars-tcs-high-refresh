#!/bin/bash
# Builds dinput8.dll (the mod) and a test build that also logs physics to phys.bin.
cd "$(dirname "$0")"
i686-w64-mingw32-gcc -shared -O2 -Wall -static-libgcc highrefresh.c dinput8.def -o dinput8.dll
i686-w64-mingw32-gcc -shared -O2 -Wall -static-libgcc -DHIGHREFRESH_PROBE highrefresh.c dinput8.def -o dinput8-probe.dll
