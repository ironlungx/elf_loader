// Simplest possible blink function in c

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
