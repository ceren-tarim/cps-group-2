// Sound to Sea: NeoPixel test only (no motors, no Processing)
// Library: Adafruit NeoPixel (Sketch > Include Library > Manage Libraries)
//
// Open the Serial Monitor (115200 baud, line ending "New Line") and type:
//   a        auto: the mood slowly goes from calm to lively and back (starts like this)
//   0        calm song (warm: orange, gold, light yellow)
//   50       in between (rose, violet, pink)
//   100      lively song (blue, indigo, purple)
//   any number from 0 to 100 gives a mood in between
//   s 0..100 gradient speed, for example  s 20  (slow)  or  s 90  (fast)
//   b 1..255 brightness, for example  b 30

#include <Adafruit_NeoPixel.h>

const int LIGHT_PIN = 1;               // NeoPixel data wire on GPIO 1
const int NUM_PIXELS = 12;             // change this to how many pixels you really have
int brightness = 50;                   // 0 to 255

Adafruit_NeoPixel pixels(NUM_PIXELS, LIGHT_PIN, NEO_GRB + NEO_KHZ800);

// same colors as the main code: red, green, blue
const float CALM[3][3]   = {{255, 90, 0}, {255, 170, 20}, {255, 235, 140}};   // orange, gold, light yellow
const float MIDDLE[3][3] = {{220, 30, 90}, {170, 70, 220}, {255, 160, 200}};  // rose, violet, soft pink
const float LIVELY[3][3] = {{0, 50, 255}, {90, 60, 255}, {180, 60, 255}};      // deep blue, indigo, purple

float mood = 0;                        // 0 calm .. 1 lively
float speed = 0.3;                     // 0 slow .. 1 fast
bool autoMode = true;                  // true: the mood sweeps by itself
unsigned long lastDraw = 0;
unsigned long lastPrint = 0;
float flowPos = 0;

char buf[16];
int len = 0;

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(brightness);
  pixels.show();
  Serial.println("NeoPixel test. Type a (auto), 0 (calm), 50 (middle), 100 (lively), s 50 (speed), b 30 (brightness)");
}

void handleLine() {
  if (buf[0] == 'a') {
    autoMode = true;
    Serial.println("auto mode");
  } else if (buf[0] == 's') {
    speed = constrain(atoi(buf + 1), 0, 100) / 100.0;
    Serial.print("speed "); Serial.println((int)(speed * 100));
  } else if (buf[0] == 'b') {
    brightness = constrain(atoi(buf + 1), 1, 255);
    pixels.setBrightness(brightness);
    Serial.print("brightness "); Serial.println(brightness);
  } else {
    autoMode = false;
    mood = constrain(atoi(buf), 0, 100) / 100.0;
    Serial.print("mood "); Serial.print((int)(mood * 100));
    Serial.println(mood < 0.34 ? "  (calm, warm)" : (mood < 0.67 ? "  (in between)" : "  (lively, cool)"));
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len > 0) { buf[len] = 0; len = 0; handleLine(); }
    } else if (len < 15) {
      buf[len++] = c;
    }
  }

  unsigned long now = millis();
  if (now - lastDraw < 30) return;      // draw about 33 times a second
  float dt = (now - lastDraw) / 1000.0;
  lastDraw = now;

  if (autoMode) {                       // 0 to 1 and back, about 20 seconds for the whole round
    float t = (now % 20000) / 20000.0;
    mood = 1.0 - fabs(2.0 * t - 1.0);
    if (now - lastPrint > 2000) {
      lastPrint = now;
      Serial.print("auto, mood "); Serial.println((int)(mood * 100));
    }
  }

  flowPos += (0.05 + 0.55 * speed) * dt;
  flowPos -= floor(flowPos);

  // the three colors right now: calm to middle to lively
  float stopColor[3][3];
  for (int s = 0; s < 3; s++) {
    for (int c = 0; c < 3; c++) {
      stopColor[s][c] = (mood < 0.5) ? CALM[s][c] + (MIDDLE[s][c] - CALM[s][c]) * (mood * 2)
                                     : MIDDLE[s][c] + (LIVELY[s][c] - MIDDLE[s][c]) * ((mood - 0.5) * 2);
    }
  }

  for (int i = 0; i < NUM_PIXELS; i++) {
    float p = (float)i / NUM_PIXELS + flowPos;
    p -= floor(p);
    float tri = 1.0 - fabs(2.0 * p - 1.0);
    float x = tri * 2.0;
    int s = (int)x; if (s > 1) s = 1;
    float f = x - s;
    float r = stopColor[s][0] * (1 - f) + stopColor[s + 1][0] * f;
    float g = stopColor[s][1] * (1 - f) + stopColor[s + 1][1] * f;
    float b = stopColor[s][2] * (1 - f) + stopColor[s + 1][2] * f;
    pixels.setPixelColor(i, pixels.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));
  }
  pixels.show();
}
