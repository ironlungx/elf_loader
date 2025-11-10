# ESP32 ELF Loader
---
Some edits of [this](https://github.com/niicoooo/esp32-elfloader) fantastic library

## Usage

- Clone this repo into `lib/`

## Compiling ELFs
You need `toolchain-xtensa-esp32` or any other relevant toolchains.
```bash
#!/usr/bin/env bash
# Build script for test app

set -e

# Find PlatformIO's toolchain
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
FILE="blink" # blink.cpp


echo "Using toolchain: ${TOOLCHAIN}"

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

Now you must **export** all the symbols you will need in your elf file. For example, here's a simple blink program in cpp:

```c
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define LOW 0x0
#define HIGH 0x1
#define OUTPUT 0x03

// All these are symbols that we're going to be using
// We must provide a definition to these in our host program
extern void digitalWrite(uint8_t pin, uint8_t val);
extern void pinMode(uint8_t pin, uint8_t mode);
extern void delay(uint32_t ms);

// This is the entry point, you can change it from your host program if you wish to do so
// The extern "C" bit is important because c++ does name mangling which messes up the host program :(
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

To compile it, just use the above build script and it should make a `*.elf` file for you :). You now have 2 options:

### ELF inside a header
You can test out your program by putting it into a header file using `xxd`:
```bash
$ ./build # Build the project
$ xxd -i blink.elf > blink_payload.h # This will dump the hex content into a c-header that you can include
                                     # in your project
```

Example host program:


```cpp
#include <Arduino.h>
#include "blink_payload.h"


static const ELFLoaderSymbol_t exports[] = {
// Example symbols to export, you can add more
    {"digitalRead", (void *)digitalRead},
    {"digitalWrite", (void *)digitalWrite},
    {"pinMode", (void *)pinMode},
    {"delay", (void *)delay},
};
static const ELFLoaderEnv_t env = {exports, sizeof(exports) / sizeof(*exports)};


void setup() {
  ELFLoaderContext_t *ctx = elfLoaderInitLoadAndRelocate(blink_elf, &env);

  if (!ctx) {
    Serial.println("elfLoaderInitLoadAndRelocate failed");
    return;
  }

  Serial.begin(115200);

  if (elfLoaderSetFunc(ctx, "local_main") != 0) { // Run 'local_main' from the elf file
    Serial.println("elfLoaderSetFunc error: local_main function not found");
    elfLoaderFree(ctx);
    return;
  }

  Serial.println("Running local_main(0x10) function as int local_main(int arg)");

  elfLoaderFree(ctx);
}

void loop() {}
```

### Proper filesystem
Use something like SD_FS or LittleFS (takes a bit more effort, but is probably what you want):

```cpp
template <typename T> struct Array {
  T *elem;
  size_t len;
};

// Loads an elf file and puts it into an array
bool load_elf(fs::FS &fs, const char *filename, Array<uint8_t> *location) {
  // Open the file
  File f = fs.open(filename, FILE_READ);
  if (!f) {
    Serial.printf("Failed to open: %s\n", filename);
    return false;
  }

  location->len = f.size();
  Serial.printf("ELF size: %zu bytes\n", location->len);

  // Allocate a new buffer
  location->elem = (uint8_t *)malloc(location->len);
  if (!location->elem) {
    Serial.println("Memory allocation failed");
    f.close();
    return false;
  }

  // Read the file
  size_t bytesRead = f.read(location->elem, location->len);
  f.close();

  if (bytesRead != location->len) {
    Serial.printf("Read error: expected %zu, got %zu\n", location->len,
                  bytesRead);
    free(location->elem);
    location->elem = nullptr;
    return false;
  }

  Serial.println("ELF loaded successfully");
  return true;
}

void setup() {
  // ...
  // Setup SD and stuff

  Array<uint8_t> elf;
  if (!load_elf(SD, "/blink.elf", &elf)) {
    Serial.println("Failed to load ELF file");
    return;
  }

  ELFLoaderContext_t *ctx = elfLoaderInitLoadAndRelocate(elf.elem, &env);

  if (!ctx) {
    Serial.println("elfLoaderInitLoadAndRelocate failed");
    return;
  }

  if (elfLoaderSetFunc(ctx, "local_main") != 0) {
    Serial.println("elfLoaderSetFunc error: local_main function not found");
    elfLoaderFree(ctx);
    return;
  }
  Serial.println(
      "Running local_main(0x10) function as int local_main(int arg)");

  // You probably should free it after you're done with it, I'm not one to judge 
}
```
