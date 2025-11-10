#include <Arduino.h>
#include <loader.h>

#include "blink_payload.h" // Or any other payload

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
