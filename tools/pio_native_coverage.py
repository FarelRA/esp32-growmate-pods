# PlatformIO extra script for [env:native] only.
# Appends the gcov runtime to the *link* step: `build_flags = --coverage`
# instruments compilation (.gcno), but SCons links test programs without
# CFLAGS, so without this the link fails on `__gcov_init`. This keeps
# coverage collection (`pio test -e native` + gcov) working.
Import("env")

env.Append(LINKFLAGS=["--coverage"])
