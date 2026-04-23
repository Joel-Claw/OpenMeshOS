// OpenMeshOS — pre-build script for PlatformIO
// Removes Helium ARM assembly from LVGL (not compatible with ESP32-S3/Xtensa)
// Copyright 2026 Joel Claw & contributors — CC0 1.0 Universal

import os
import glob

Import("env")

# Find and remove Helium assembly files from LVGL
# These are ARM-only and cause build failures on ESP32-S3 (Xtensa)
build_dir = env.subst("$BUILD_DIR")
lib_build_dir = os.path.join(os.path.dirname(build_dir), "lib")

# Walk the LVGL build directory and remove any .S (assembly) files
# that are Helium-specific (they have "helium" in their path)
for root, dirs, files in os.walk(lib_build_dir):
    for f in files:
        if f.endswith('.S') or f.endswith('.s'):
            filepath = os.path.join(root, f)
            # Only remove Helium assembly, not all assembly
            if 'helium' in filepath.lower() or 'arm2d' in filepath.lower():
                try:
                    os.remove(filepath)
                    print(f"Removed incompatible assembly: {filepath}")
                except OSError as e:
                    print(f"Warning: could not remove {filepath}: {e}")

# Also check source LVGL dirs in .pio/libdeps
libdeps_dir = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "libdeps", env.subst("$PIOENV"))
if os.path.exists(libdeps_dir):
    for root, dirs, files in os.walk(libdeps_dir):
        for f in files:
            if f.endswith('.S') or f.endswith('.s'):
                filepath = os.path.join(root, f)
                if 'helium' in filepath.lower() or 'arm2d' in filepath.lower():
                    try:
                        os.remove(filepath)
                        print(f"Removed incompatible assembly: {filepath}")
                    except OSError as e:
                        print(f"Warning: could not remove {filepath}: {e}")