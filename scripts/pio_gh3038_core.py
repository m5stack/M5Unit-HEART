# Link the precompiled GH3038 core (src/<mcu>/libm5gh3038core.a) for StampHeart
# Used as extra_scripts of this repository and as build.extraScript of library.json
from os.path import isfile, join

Import("env")

try:
    Import("pio_lib_builder")
    root = pio_lib_builder.path
except Exception:
    root = env.subst("$PROJECT_DIR")

mcu = env.BoardConfig().get("build.mcu", "") if env.get("BOARD") else ""
lib_dir = join(root, "src", mcu)
if mcu and isfile(join(lib_dir, "libm5gh3038core.a")):
    for e in (env, DefaultEnvironment()):
        e.Append(LIBPATH=[lib_dir], LIBS=["m5gh3038core"])
elif mcu:
    # Not an error: UnitHeart / HatHeart can be used without the core
    print("M5Unit-HEART: libm5gh3038core.a for %s is not found in %s. StampHeart (UnitGH3038) is not available"
          % (mcu, lib_dir))
