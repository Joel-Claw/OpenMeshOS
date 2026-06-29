# OpenMeshOS - pre-build script for RAK4631 (nRF52)
# Installs WisCore_RAK4631_Board variant files into the Adafruit nRF52 framework
# Copyright 2026 Joel Claw & contributors - WTFPL v2

import os
import shutil

Import("env")

project_dir = env.subst("$PROJECT_DIR")
variant_src = os.path.join(project_dir, "boards", "WisCore_RAK4631_Board")

# Find the framework-arduinoadafruitnrf52 package
framework_dir = None
pio_packages = os.path.join(os.path.expanduser("~"), ".platformio", "packages")
if os.path.exists(pio_packages):
    for d in os.listdir(pio_packages):
        if d.startswith("framework-arduinoadafruitnrf52"):
            framework_dir = os.path.join(pio_packages, d)
            break

if not framework_dir:
    print("RAK4631 pre-build: framework-arduinoadafruitnrf52 not found, skipping variant install")
else:
    variants_dir = os.path.join(framework_dir, "variants", "WisCore_RAK4631_Board")
    if os.path.exists(variants_dir):
        print("RAK4631 pre-build: variant already installed")
    elif os.path.exists(variant_src):
        shutil.copytree(variant_src, variants_dir)
        print("RAK4631 pre-build: installed WisCore_RAK4631_Board variant to " + variants_dir)
    else:
        print("RAK4631 pre-build: variant source not found at " + variant_src)