# ESP32 ELF Loader

A modified version of [this excellent library](https://github.com/niicoooo/esp32-elfloader) for loading ELF files on ESP32.

## Installation

Clone this repository into your project's `lib/` directory.

## Building ELF Files

### Requirements
You'll need the ESP32 toolchain (`toolchain-xtensa-esp32` or equivalent).

### Build Script
Create a build script to compile your code into an ELF file:

```bash
#!/usr/bin/env bash
set -e

# Locate the ESP32 toolchain
if [ -d "$HOME/.platformio/packages/toolchain-xtensa-esp32/bin" ]; then
    TOOLCHAIN="$HOME/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-"
elif command -v xtensa-esp32-elf-gcc &> /dev/null; then
    TOOLCHAIN="xtensa-esp32-elf-"
else
    echo "ERROR: ESP32 toolchain not found!"
    echo "Install with: pio platform install espressif32"
    exit 1
fi

CC="${TOOLCHAIN}g++"
SIZE="${TOOLCHAIN}size"
FILE="blink"  # Your source file name (without extension)

echo "Using toolchain: ${TOOLCHAIN}"

# Compile
${CC} -c ${FILE}.cpp -o ${FILE}.o \
    -fno-rtti \
    -fno-exceptions \
    -fno-use-cxa-atexit \
    -Wl,-r \
    -lgcc \
    -lstdc++ \
    -nostartfiles

# Link
${CC} ${FILE}.o -o ${FILE}.elf \
    -Wl,-r \
    -nostartfiles \
    -nodefaultlibs \
    -nostdlib

rm ${FILE}.o
${SIZE} ${FILE}.elf
```

### Writing Your ELF Code

You must declare external symbols for any functions you'll use from the host program. Here's a simple LED blink example:

```cpp
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define LOW 0x0
#define HIGH 0x1
#define OUTPUT 0x03

// Declare external functions (provided by the host)
extern void digitalWrite(uint8_t pin, uint8_t val);
extern void pinMode(uint8_t pin, uint8_t mode);
extern void delay(uint32_t ms);

// Entry point - use extern "C" to prevent C++ name mangling
extern "C" int local_main() {
  pinMode(2, OUTPUT);
  while (1) {
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    delay(500);
  }
  return 0;
}
```

Run the build script to generate your `*.elf` file.

## Loading Methods

### Option 1: Embed in Header File

Convert your ELF to a C header for quick testing:

```bash
./build  # Compile your code
xxd -i blink.elf > blink_payload.h  # Convert to header
```

**Host program example:**

```cpp
#include <Arduino.h>
#include <loader.h>

#include "blink_payload.h"

// Export symbols to the ELF
static const ELFLoaderSymbol_t exports[] = {
    {"digitalRead", (void *)digitalRead},
    {"digitalWrite", (void *)digitalWrite},
    {"pinMode", (void *)pinMode},
    {"delay", (void *)delay},
};
static const ELFLoaderEnv_t env = {exports, sizeof(exports) / sizeof(*exports)};

void setup() {
  Serial.begin(115200);
  
  // Load and relocate the ELF
  ELFLoaderContext_t *ctx = elfLoaderInitLoadAndRelocate(blink_elf, &env);
  if (!ctx) {
    Serial.println("Failed to load ELF");
    return;
  }

  // Find and run the entry point
  if (elfLoaderSetFunc(ctx, "local_main") != 0) {
    Serial.println("Error: local_main function not found");
    elfLoaderFree(ctx);
    return;
  }

  Serial.println("Running local_main()");
  elfLoaderRun(ctx, NULL);
  elfLoaderFree(ctx);
}

void loop() {}
```

### Option 2: Load from Filesystem

Use SD card or LittleFS for more flexibility:

```cpp
template <typename T> struct Array {
  T *elem;
  size_t len;
};

// Load ELF file from filesystem
bool load_elf(fs::FS &fs, const char *filename, Array<uint8_t> *location) {
  File f = fs.open(filename, FILE_READ);
  if (!f) {
    Serial.printf("Failed to open: %s\n", filename);
    return false;
  }

  location->len = f.size();
  Serial.printf("ELF size: %zu bytes\n", location->len);

  location->elem = (uint8_t *)malloc(location->len);
  if (!location->elem) {
    Serial.println("Memory allocation failed");
    f.close();
    return false;
  }

  size_t bytesRead = f.read(location->elem, location->len);
  f.close();

  if (bytesRead != location->len) {
    Serial.printf("Read error: expected %zu, got %zu\n", location->len, bytesRead);
    free(location->elem);
    location->elem = nullptr;
    return false;
  }

  Serial.println("ELF loaded successfully");
  return true;
}

void setup() {
  // Initialize SD card or filesystem
  
  Array<uint8_t> elf;
  if (!load_elf(SD, "/blink.elf", &elf)) {
    Serial.println("Failed to load ELF file");
    return;
  }

  ELFLoaderContext_t *ctx = elfLoaderInitLoadAndRelocate(elf.elem, &env);
  if (!ctx) {
    Serial.println("Failed to load and relocate ELF");
    return;
  }

  if (elfLoaderSetFunc(ctx, "local_main") != 0) {
    Serial.println("Error: local_main function not found");
    elfLoaderFree(ctx);
    return;
  }

  Serial.println("Running local_main()");
  int result = elfLoaderRun(ctx, NULL);
  
  // Clean up when done
  elfLoaderFree(ctx);
}
```

## C++ Support

To enable C++ features in your ELF files, export these additional runtime symbols:

```cpp
extern "C" {
extern void *__dso_handle;

extern int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso);

extern void __cxa_pure_virtual() {
  Serial.println("ERROR: Pure virtual function called!");
  while (1)
    delay(1000);
}

extern int __cxa_guard_acquire(uint64_t *guard);

extern void __cxa_guard_release(uint64_t *guard);

extern void __cxa_guard_abort(uint64_t *guard);
}

static const ELFLoaderSymbol_t exports[] = {
    {"print", (void *)print},
    {"digitalRead", (void *)digitalRead},
    {"digitalWrite", (void *)digitalWrite},
    {"pinMode", (void *)pinMode},
    {"delay", (void *)delay},

    {"__dso_handle", (void *)&__dso_handle},
    {"__cxa_atexit", (void *)__cxa_atexit},
    {"__cxa_pure_virtual", (void *)__cxa_pure_virtual},
    {"__cxa_guard_acquire", (void *)__cxa_guard_acquire},
    {"__cxa_guard_release", (void *)__cxa_guard_release},
    {"__cxa_guard_abort", (void *)__cxa_guard_abort},

    // C++ new/delete operators
    {"_Znwj", (void *)malloc}, // operator new(unsigned int)
    {"_ZdlPv", (void *)free},  // operator delete(void*)
    {"_ZdlPvj", (void *)free}, // operator delete(void*, unsigned int)
    {"_Znaj", (void *)malloc}, // operator new[](unsigned int)
    {"_ZdaPv", (void *)free},  // operator delete[](void*)
    {"_ZdaPvj", (void *)free}, // operator delete[](void*, unsigned int)
};
static const ELFLoaderEnv_t env = {exports, sizeof(exports) / sizeof(*exports)};
```
