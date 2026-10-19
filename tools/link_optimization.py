"""Enable GCC LTO at both compile and link/archive stages on the pinned toolchain."""
Import("env")
env.Append(LINKFLAGS=["-flto", "-fuse-linker-plugin"])
env.Replace(AR="xtensa-esp32-elf-gcc-ar", RANLIB="xtensa-esp32-elf-gcc-ranlib")
