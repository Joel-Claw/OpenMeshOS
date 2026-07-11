/* OpenMeshOS nRF52 SPIFFS compatibility shim
   Maps SPIFFS API to Adafruit InternalFS (LittleFS) on nRF52 Arduino core.
   The Adafruit nRF52 core does not provide SPIFFS — it uses InternalFS
   (a LittleFS wrapper). This shim provides a SPIFFS-compatible class
   with the same API as ESP32's SPIFFSClass (open with string mode, exists,
   remove, mkdir, begin, format) so the rest of the codebase can use
   it unchanged on both platforms.
   Copyright 2026 Joel Claw & contributors - WTFPL v2 */
#ifndef OMS_SPIFFS_SHIM_H
#define OMS_SPIFFS_SHIM_H

#if defined(ARDUINO_ARCH_NRF52840)

  #include <Adafruit_LittleFS.h>
  #include <InternalFileSystem.h>

  // SPIFFS-compatible wrapper around Adafruit InternalFS.
  // Provides the same API surface as ESP32's SPIFFSClass (fs::FS).
  class SPIFFSCompat
  {
  public:
      bool begin(bool formatOnFail = false)
      {
          if (formatOnFail)
          {
              InternalFS.format();
          }
          return InternalFS.begin();
      }

      bool format() { return InternalFS.format(); }
      bool exists(const char* path) { return InternalFS.exists(path); }
      bool remove(const char* path) { return InternalFS.remove(path); }
      bool mkdir(const char* path) { return InternalFS.mkdir(path); }

      // ESP32-compatible open(): accepts mode string "r", "w", "a".
      // Adafruit LittleFS only has FILE_O_READ and FILE_O_WRITE;
      // for append, we use FILE_O_WRITE (which opens RW + creates).
      Adafruit_LittleFS_Namespace::File open(const char* path, const char* mode = "r", bool createIfMissing = true)
      {
          (void)createIfMissing; // LittleFS creates on write automatically
          uint8_t flags = Adafruit_LittleFS_Namespace::FILE_O_READ;
          if (mode[0] == 'w' || mode[0] == 'a')
          {
              flags = Adafruit_LittleFS_Namespace::FILE_O_WRITE;
          }
          return InternalFS.open(path, flags);
      }

      // Underlying InternalFS access (for MeshCore's IdentityStore
      // which expects Adafruit_LittleFS&).
      InternalFileSystem& raw() { return InternalFS; }
  };

  // Type alias: on nRF52, "File" resolves to Adafruit LittleFS File.
  using File = Adafruit_LittleFS_Namespace::File;

  // C++14-compatible global instance accessor.
  // (inline variables require C++17; use a function with static local instead.)
  inline SPIFFSCompat& SPIFFSInstance()
  {
      static SPIFFSCompat instance;
      return instance;
  }

  // Macro so existing code `SPIFFS.begin()`, `SPIFFS.open(...)` etc. works.
  #define SPIFFS SPIFFSInstance()

#else
  // ESP32: use the real SPIFFS
  #include <SPIFFS.h>
#endif

#endif /* OMS_SPIFFS_SHIM_H */