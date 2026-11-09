// Sound to Sea: angle test for the five motors + NeoPixel lights + a BUTTON.
// Libraries: AccelStepper and Adafruit NeoPixel.
//
// BUTTON: one leg of the button to GPIO 2, the other leg to GND. No resistor needed.
//   1st press: all five strips go to LOW_DEG (30 degrees).
//   2nd press: all five strips go to HIGH_DEG (230 degrees).
//   3rd press: back to 30, and so on. Every press switches to the other angle.
//
// SERIAL MONITOR (115200 baud, line ending "New Line"), still works as before:
//   120         all strips go to 120 degrees
//   2 120       only strip 2 goes to 120 degrees
//   m 0         lights fully warm (orange and gold)
//   m 50        lights halfway between warm and cool
//   m 100       lights fully cool (blue and purple)

#include <AccelStepper.h>
#include <Adafruit_NeoPixel.h>

// Direction of each motor: 1 = normal, -1 = flipped.
int DIR[5] = {1, 1, 1, 1, 1};

// Same pins as the strips code. Order is IN1, IN3, IN2, IN4.
AccelStepper stepper[5] = {
  AccelStepper(AccelStepper::HALF4WIRE, 4, 6, 5, 7),      // strip 1
  AccelStepper(AccelStepper::HALF4WIRE, 15, 17, 16, 18),  // strip 2
  AccelStepper(AccelStepper::HALF4WIRE, 8, 9, 3, 10),     // strip 3
  AccelStepper(AccelStepper::HALF4WIRE, 11, 13, 12, 14),  // strip 4
  AccelStepper(AccelStepper::HALF4WIRE, 38, 40, 39, 41)   // strip 5
};

// Safety: the strips can only be sent between these two angles.
const float MIN_DEG = 30;
const float MAX_DEG = 230;

// The two angles the button switches between (keep them inside the safety limits).
const float LOW_DEG  = 30;      // 1st press, 3rd press, ...
const float HIGH_DEG = 230;     // 2nd press, 4th press, ...

long degToSteps(float deg) { return (long)(4096.0 * deg / 360.0); }   // 4096 steps = one full turn

// ===== THE BUTTON =====
const int SWITCH_PIN = 2;                   // the button is connected between GPIO 2 and GND
bool nextIsHigh = false;                    // false: the next press goes to LOW_DEG. true: the next press goes to HIGH_DEG

// Debounce: a button "bounces" for a few milliseconds when pressed. We only believe a new
// state after it stayed the same for 50 ms.
int buttonState = HIGH;                     // the button state we trust right now
int lastReading = HIGH;                     // what the button said the last time we looked
unsigned long lastDebounceTime = 0;         // when the reading last changed
const unsigned long debounceDelay = 50;     // how long it must stay the same (milliseconds)

// ===== THE LIGHTS (NeoPixels) =====
const int LIGHT_PIN = 1;            // the NeoPixel data wire (through the 330 ohm resistor)
const int NUM_PIXELS = 12;          // how many pixels the strip has
const int LIGHT_BRIGHTNESS = 50;    // 0 to 255. Keep it low so the power bank can run the motors too
Adafruit_NeoPixel pixels(NUM_PIXELS, LIGHT_PIN, NEO_GRB + NEO_KHZ800);

// two moods, each with three colors {red, green, blue}
const float CALM[3][3]   = {{255, 90, 0}, {255, 170, 20}, {255, 235, 140}};   // orange, gold, light yellow
const float LIVELY[3][3] = {{0, 50, 255}, {90, 60, 255}, {180, 60, 255}};     // deep blue, indigo, purple

float lightMood = 0;                // 0 = warm .. 1 = cool. Change it by typing  m 0  to  m 100
unsigned long lastDraw = 0;         // when the lights were last drawn
float flowPos = 0;                  // where the gradient is now (moves along and loops)

void updateLights() {
  unsigned long now = millis();
  if (now - lastDraw < 30) return;            // draw about 33 times a second
  float dt = (now - lastDraw) / 1000.0;       // seconds since the last draw
  lastDraw = now;

  flowPos += 0.15 * dt;                       // slow flow
  flowPos -= floor(flowPos);                  // loop back to 0 after 1

  float stopColor[3][3];                      // the three colors right now (mix of calm and lively)
  for (int s = 0; s < 3; s++)
    for (int c = 0; c < 3; c++)
      stopColor[s][c] = (CALM[s][c] + (LIVELY[s][c] - CALM[s][c]) * lightMood) * 0.85;

  for (int i = 0; i < NUM_PIXELS; i++) {
    float p = (float)i / NUM_PIXELS + flowPos;
    p -= floor(p);                            // 0..1 along the gradient
    float x = (1.0 - fabs(2.0 * p - 1.0)) * 2.0;   // 0 up to 2 and back, so there is no hard edge
    int s = (int)x; if (s > 1) s = 1;
    float f = x - s;                          // how far toward the next color
    float r = stopColor[s][0] * (1 - f) + stopColor[s + 1][0] * f;
    float g = stopColor[s][1] * (1 - f) + stopColor[s + 1][1] * f;
    float b = stopColor[s][2] * (1 - f) + stopColor[s + 1][2] * f;
    pixels.setPixelColor(i, pixels.Color((uint8_t)r, (uint8_t)g, (uint8_t)b));
  }
  pixels.show();                              // send the colors to the strip
}
// -----------------------------------

void setup() {
  Serial.begin(115200);
  pinMode(SWITCH_PIN, INPUT_PULLUP);          // the button pin reads HIGH normally and LOW when pressed
  pixels.begin();                             // wake up the NeoPixels
  pixels.setBrightness(LIGHT_BRIGHTNESS);
  pixels.show();
  for (int i = 0; i < 5; i++) {
    stepper[i].setMaxSpeed(800.0);        // same speed as the music code, for an accurate test
    stepper[i].setAcceleration(600.0);    // same acceleration as the music code
    stepper[i].setCurrentPosition(0);     // where the strips are now = 0 degrees
  }
  Serial.println("Ready. Where the strips are now is 0 degrees.");
  Serial.println("Press the button: 1st press goes to 30 degrees, 2nd press goes to 230 degrees, and so on.");
  Serial.println("Or type an angle and press Enter, for example  120  or  2 120");
  Serial.println("Lights: type  m 0  (warm)  to  m 100  (cool)");
}

char buf[24];
int len = 0;

void goTo(int i, float deg) {
  stepper[i].enableOutputs();                           // switch the coils on before moving
  stepper[i].moveTo(degToSteps(deg) * DIR[i]);
}

// sends all five strips to the same angle
void goAll(float deg) {
  for (int i = 0; i < 5; i++) goTo(i, deg);
}

// true when all five strips have arrived at their target
bool allArrived() {
  for (int i = 0; i < 5; i++) if (stepper[i].distanceToGo() != 0) return false;
  return true;
}

void handleLine() {
  // a line that starts with m sets the light mood, for example  m 70
  if (buf[0] == 'm' || buf[0] == 'M') {
    float v = constrain(atof(buf + 1), 0, 100);
    lightMood = v / 100.0;
    Serial.print("light mood ");
    Serial.println(v, 0);
    return;
  }

  char *a = strtok(buf, " ");
  char *b = strtok(NULL, " ");
  if (!a) return;

  int strip = 0;                          // 0 = all strips
  float deg;
  if (b) { strip = atoi(a); deg = atof(b); }
  else   { deg = atof(a); }

  if (deg < MIN_DEG || deg > MAX_DEG) {
    Serial.print("Too far. Please type between ");
    Serial.print(MIN_DEG, 0);
    Serial.print(" and ");
    Serial.println(MAX_DEG, 0);
    return;
  }
  if (strip < 0 || strip > 5) {
    Serial.println("Strip must be 1 to 5 (or leave it out for all strips)");
    return;
  }

  if (strip == 0) { goAll(deg); }
  else            { goTo(strip - 1, deg); }

  Serial.print("going to ");
  Serial.print(deg, 1);
  Serial.print(" degrees");
  if (strip == 0) Serial.println(" (all strips)");
  else { Serial.print(" (strip "); Serial.print(strip); Serial.println(")"); }
}

// looks at the button and reacts to a press
void checkButton() {
  int reading = digitalRead(SWITCH_PIN);                 // HIGH = not pressed, LOW = pressed

  if (reading != lastReading) lastDebounceTime = millis();   // it changed: start the 50 ms timer again

  if ((millis() - lastDebounceTime) > debounceDelay) {   // it stayed the same for 50 ms, so we believe it
    if (reading != buttonState) {                        // and it is a real change
      buttonState = reading;

      if (buttonState == LOW) {                          // the button was just pressed
        if (!nextIsHigh) {
          goAll(LOW_DEG);                                // 1st, 3rd, 5th press: go to the low angle
          Serial.print("button: going to ");
          Serial.print(LOW_DEG, 0);
          Serial.println(" degrees");
        } else {
          goAll(HIGH_DEG);                               // 2nd, 4th, 6th press: go to the high angle
          Serial.print("button: going to ");
          Serial.print(HIGH_DEG, 0);
          Serial.println(" degrees");
        }
        nextIsHigh = !nextIsHigh;                        // the next press does the other one
      }
    }
  }
  lastReading = reading;
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len > 0) { buf[len] = 0; len = 0; handleLine(); }
    } else if (len < 23) {
      buf[len++] = c;
    }
  }

  checkButton();                              // did someone press the button?

  for (int i = 0; i < 5; i++) stepper[i].run();   // take the steps that are due

  if (allArrived()) {                         // all five strips have arrived: switch the coils off
    for (int i = 0; i < 5; i++) stepper[i].disableOutputs();   // saves power and heat
  }

  updateLights();                             // keep the gradient flowing
}