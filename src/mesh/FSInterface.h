/* OpenMeshOS platform-agnostic filesystem interface
   Used by mesh layer to avoid ESP32-specific fs::FS dependency.
   Copyright 2026 Joel Claw & contributors - WTFPL v2 */
#ifndef OMS_FS_INTERFACE_H
#define OMS_FS_INTERFACE_H

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
  // ESP32: use the real fs::FS / fs::File types
  #include <FS.h>
  #include <SPIFFS.h>
  namespace oms {
    using FSRef = fs::FS;
    using FileRef = fs::File;
  }
#else
  // nRF52: use Adafruit LittleFS types
  #include <Adafruit_LittleFS.h>
  #include <InternalFileSystem.h>
  namespace oms {
    using FSRef = InternalFileSystem;
    using FileRef = Adafruit_LittleFS_Namespace::File;
  }
#endif

#endif /* OMS_FS_INTERFACE_H */