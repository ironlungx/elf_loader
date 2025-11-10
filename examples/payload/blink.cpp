// Slightly more complex setup, uses c++ unnecessarily

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define LOW 0x0
#define HIGH 0x1
#define INPUT 0x01
#define OUTPUT 0x03
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09
#define OPEN_DRAIN 0x10
#define OUTPUT_OPEN_DRAIN 0x13
#define ANALOG 0xC0

extern "C" {
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
void pinMode(uint8_t pin, uint8_t mode);
void delay(uint32_t ms);
}

class App {
protected:
  int version;

public:
  App(int v) : version(v) {}
  virtual void run() {}
  virtual ~App() {}
};

class Blink : public App {
  uint8_t led_pin;

public:
  Blink(uint8_t pin, int v = 1) : App(v), led_pin(pin) {}

  void run() override {
    pinMode(led_pin, OUTPUT);
    while (1) {
      digitalWrite(led_pin, HIGH);
      delay(500);
      digitalWrite(led_pin, LOW);
      delay(500);
    }
  }
};

extern "C" int local_main() {
  Blink f(2);
  f.run();
  return 0;
}
