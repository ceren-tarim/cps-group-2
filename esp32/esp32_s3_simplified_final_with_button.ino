// Sound to Sea: ESP32 S3, five 28BYJ 48 stepper motors (ULN2003 boards) and NeoPixels.
// Libraries needed: AccelStepper and Adafruit NeoPixel.
//
// Processing sends two kinds of messages over the USB cable:
//   "H a b c d e"            the five heights, each from 0 to 1000
//   "C mood speed low mid high"   the light information, each from 0 to 1000
//
// BUTTON (one leg to GPIO 2, the other leg to GND, no resistor needed):
//   It works like a pause button. Every press switches between two steps:
//   1. MUSIC     the strips follow the music (this is the mode at power on)
//   2. PAUSE     all strips go to QUIET_ANGLE (30) and stay there, the music is ignored
//   The lights keep following the music in both steps.

#include <AccelStepper.h>        // library that moves stepper motors smoothly
#include <Adafruit_NeoPixel.h>   // library that controls the NeoPixel light strip

// ===== 1. THE ONLY NUMBERS YOU NEED TO CHANGE =====
// All angles are in degrees. The code never goes outside MIN_ANGLE and MAX_ANGLE.

const float QUIET_ANGLE = 20;    // the angle of every strip when there is silence (the rest position)
const float LOUD_ANGLE  = 355;   // the angle of a strip at the loudest moment
const float MIN_ANGLE   = 20;    // the code will never send a motor below this angle
const float MAX_ANGLE   = 355;   // the code will never send a motor above this angle

const int   SPEED = 700;         // how fast the motors turn (steps per second). Higher is faster
const int   ACCEL = 800;        // how gently the motors speed up and slow down. Lower is gentler

const int N = 5;                 // how many strips (motors) we have
// ==================================================

// This small function turns an angle (in degrees) into motor steps.
// A 28BYJ 48 motor needs 4096 steps for one full turn (360 degrees).
long angleToSteps(float deg) {
  deg = constrain(deg, MIN_ANGLE, MAX_ANGLE);   // keep the angle between the two safety limits, whatever comes in
  return (long)(deg * 4096.0 / 360.0);          // angle divided by 360 is the part of a turn, times 4096 gives steps
}

// ===== 2. THE FIVE MOTORS =====
// Each motor uses four pins. The order must be IN1, IN3, IN2, IN4 for this motor type.
// HALF4WIRE means the motor moves in half steps, which is smoother.
AccelStepper st[N] = {
  AccelStepper(AccelStepper::HALF4WIRE, 4, 6, 5, 7),         // strip 1 uses GPIO 4, 5, 6, 7
  AccelStepper(AccelStepper::HALF4WIRE, 15, 17, 16, 18),     // strip 2 uses GPIO 15, 16, 17, 18
  AccelStepper(AccelStepper::HALF4WIRE, 8, 9, 3, 10),        // strip 3 uses GPIO 8, 3, 9, 10
  AccelStepper(AccelStepper::HALF4WIRE, 11, 13, 12, 14),     // strip 4 uses GPIO 11, 12, 13, 14
  AccelStepper(AccelStepper::HALF4WIRE, 38, 40, 39, 41)      // strip 5 uses GPIO 38, 39, 40, 41
};

long target[N];                  // where each strip should go, remembered in steps
unsigned long lastMsg = 0;       // the time (in milliseconds) when the last height message arrived
char line[64];                   // a box that collects the letters of one incoming message
int len = 0;                     // how many letters are in the box right now

// ===== 2B. THE BUTTON =====
const int SWITCH_PIN = 2;                // the button is connected between GPIO 2 and GND

// The two steps the button switches between. Each press goes to the other one.
const int MODE_MUSIC = 0;                // the strips follow the music
const int MODE_PAUSE = 1;                // all strips hold at QUIET_ANGLE
int mode = MODE_MUSIC;                   // the step we are in right now

// Debounce: a button "bounces" for a few milliseconds when pressed. We only believe a new
// state after it stayed the same for 50 ms.
int buttonState = HIGH;                  // the button state we trust right now (HIGH = not pressed)
int lastReading = HIGH;                  // what the button said the last time we looked
unsigned long lastDebounceTime = 0;      // when the reading last changed
const unsigned long debounceDelay = 50;  // how long it must stay the same (milliseconds)

// looks at the button and switches between MUSIC and PAUSE when it is pressed
void checkButton() {
  int reading = digitalRead(SWITCH_PIN);                     // HIGH = not pressed, LOW = pressed
  if (reading != lastReading) lastDebounceTime = millis();   // it changed: start the 50 ms timer again
  if ((millis() - lastDebounceTime) > debounceDelay) {       // it stayed the same for 50 ms, so we believe it
    if (reading != buttonState) {                            // and it is a real change
      buttonState = reading;
      if (buttonState == LOW) {                              // the button was just pressed
        if (mode == MODE_MUSIC) {
          mode = MODE_PAUSE;
          Serial.println("button: PAUSE, all strips go to the quiet angle");
        } else {
          mode = MODE_MUSIC;
          Serial.println("button: MUSIC, the strips follow the music again");
        }
      }
    }
  }
  lastReading = reading;
}

// ===== 3. THE LIGHTS =====
const int LIGHT_PIN = 1;                 // the NeoPixel data wire is connected to GPIO 1
const int NUM_PIXELS = 12;               // how many pixels the light strip has
const int LIGHT_BRIGHTNESS = 50;         // brightness from 0 to 255. Low keeps the power bank happy
Adafruit_NeoPixel pixels(NUM_PIXELS, LIGHT_PIN, NEO_GRB + NEO_KHZ800);   // creates the light strip object

// Each mood has three colors, written as {red, green, blue}, each from 0 to 255.
const float CALM[3][3]   = {{0, 70, 160}, {0, 150, 170}, {120, 220, 190}};   // cool: deep sea blue, teal, seafoam
const float LIVELY[3][3] = {{255, 60, 0}, {255, 130, 10}, {255, 200, 60}};   // warm: red orange, orange, golden yellow

const float MOOD_LOW  = 0.4;     // a song mood below this number gives fully cool colors
const float MOOD_HIGH = 0.6;     // a song mood above this number gives fully warm colors

float lightMood = 0;                     // the song mood we last received (0 calm, 1 lively)
float lightSpeed = 0.1;                  // how fast the colors should flow (0 slow, 1 fast)
float lightBand[3] = {0.5, 0.5, 0.5};    // how strong the bass, mids and highs are
unsigned long lastLight = 0;             // the time when the last light message arrived
unsigned long lastDraw = 0;              // the time when we last updated the pixels
float flowPos = 0;                       // where the color gradient is now (moves along and loops)

// This function paints the pixels. The loop below calls it again and again.
void updateLights() {
  unsigned long now = millis();                  // the current time in milliseconds
  if (now - lastDraw < 30) return;               // if less than 30 ms passed, do nothing (about 33 updates per second)
  float dt = (now - lastDraw) / 1000.0;          // seconds since the last update
  lastDraw = now;                                // remember this update time

  bool lit = (now - lastLight) < 3000;           // true if we heard from Processing in the last 3 seconds
  float mood  = lit ? lightMood : 0;             // with no news, use the calm mood
  float speed = lit ? lightSpeed : 0.1;          // with no news, flow slowly
  flowPos += (0.05 + 0.55 * speed) * dt;         // move the gradient forward, faster when the song is faster
  flowPos -= floor(flowPos);                     // when it passes 1, start again from 0

  // blend is 0 for fully warm and 1 for fully cool, based on the mood and the two limits
  float blend = constrain((mood - MOOD_LOW) / (MOOD_HIGH - MOOD_LOW), 0.0, 1.0);
  float stopColor[3][3];                         // the three colors we use right now
  for (int s = 0; s < 3; s++) {                  // go through the three colors
    float glow = 0.55 + 0.45 * (lit ? lightBand[s] : 0.5);   // louder band means a brighter color
    for (int c = 0; c < 3; c++)                  // go through red, green, blue
      stopColor[s][c] = (CALM[s][c] + (LIVELY[s][c] - CALM[s][c]) * blend) * glow;   // mix warm and cool, then apply glow
  }

  for (int i = 0; i < NUM_PIXELS; i++) {         // go through every pixel one by one
    float p = (float)i / NUM_PIXELS + flowPos;   // the pixel's place along the gradient, shifted by the flow
    p -= floor(p);                               // keep it between 0 and 1
    float x = (1.0 - fabs(2.0 * p - 1.0)) * 2.0; // goes 0 up to 2 and back down, so the gradient has no hard edge
    int s = (int)x; if (s > 1) s = 1;            // which of the first two colors we are starting from
    float f = x - s;                             // how far we are toward the next color (0 to 1)
    float r = stopColor[s][0] * (1 - f) + stopColor[s + 1][0] * f;   // mix the red of the two neighbor colors
    float g = stopColor[s][1] * (1 - f) + stopColor[s + 1][1] * f;   // mix the green
    float b = stopColor[s][2] * (1 - f) + stopColor[s + 1][2] * f;   // mix the blue

    // pixels get brighter when their strip is higher
    int k = i * N / NUM_PIXELS;                  // which strip this pixel belongs to
    float h = (float)(st[k].currentPosition() - angleToSteps(QUIET_ANGLE)) /
              (float)(angleToSteps(LOUD_ANGLE) - angleToSteps(QUIET_ANGLE));   // how high the strip is, from 0 to 1
    h = constrain(h, 0.0, 1.0);                  // keep it between 0 and 1
    float m = 0.5 + 0.5 * h;                     // low strip gives half brightness, high strip gives full
    pixels.setPixelColor(i, pixels.Color((uint8_t)(r * m), (uint8_t)(g * m), (uint8_t)(b * m)));   // set this pixel's color
  }
  pixels.show();                                 // send all the colors to the light strip
}

// ===== 4. START UP =====
// This runs once when the board switches on.
// Before power on, the strips should be sitting at the QUIET angle position.
void setup() {
  Serial.begin(115200);                          // open the USB connection at 115200 speed
  pinMode(SWITCH_PIN, INPUT_PULLUP);             // the button pin reads HIGH normally and LOW when pressed
  pixels.begin();                                // wake up the NeoPixels
  pixels.setBrightness(LIGHT_BRIGHTNESS);        // set how bright they can be
  pixels.show();                                 // start with the lights showing nothing yet
  for (int k = 0; k < N; k++) {                  // do this for each of the five motors
    st[k].setMaxSpeed(SPEED);                    // top speed of this motor
    st[k].setAcceleration(ACCEL);                // how smoothly it speeds up and slows down
    st[k].setCurrentPosition(angleToSteps(QUIET_ANGLE));   // tell the motor "you are at the QUIET angle right now"
    target[k] = angleToSteps(QUIET_ANGLE);       // and the first goal is to stay there
  }
  lastMsg = millis();                            // start the "last message" timer now
  Serial.println("ready");                       // print ready so you can see it started
}

// ===== 5. READING A MESSAGE =====
// This runs every time a full line has arrived from Processing.
void parseLine() {
  if (line[0] == 'H') {                          // a line starting with H holds the five heights
    char *p = strtok(line + 1, " ");             // cut out the first number after the H
    int k = 0;                                   // counts how many numbers we have read
    long t[N];                                   // temporary list of the five new targets
    while (p != NULL && k < N) {                 // keep going while there are numbers left (max 5)
      int val = constrain(atoi(p), 0, 1000);     // turn the text into a number from 0 to 1000
      float angle = QUIET_ANGLE + (LOUD_ANGLE - QUIET_ANGLE) * val / 1000.0;   // 0 gives QUIET angle, 1000 gives LOUD angle
      t[k] = angleToSteps(angle);                // turn that angle into motor steps
      p = strtok(NULL, " ");                     // move on to the next number
      k++;                                       // count it
    }
    if (k == N && mode == MODE_MUSIC) {          // only use the message if all five numbers arrived, and only in MUSIC mode
      for (int i = 0; i < N; i++) target[i] = t[i];   // save the five new targets
      lastMsg = millis();                        // remember that we just heard from Processing
    }
  } else if (line[0] == 'C') {                   // a line starting with C holds the light information
    int m, s, lo, mi, hi;                        // mood, speed, low, mid, high
    if (sscanf(line + 1, "%d %d %d %d %d", &m, &s, &lo, &mi, &hi) == 5) {   // read the five numbers, check all arrived
      lightMood = constrain(m, 0, 1000) / 1000.0;      // mood turned into 0 to 1
      lightSpeed = constrain(s, 0, 1000) / 1000.0;     // speed turned into 0 to 1
      lightBand[0] = constrain(lo, 0, 1000) / 1000.0;  // bass strength
      lightBand[1] = constrain(mi, 0, 1000) / 1000.0;  // mids strength
      lightBand[2] = constrain(hi, 0, 1000) / 1000.0;  // highs strength
      lastLight = millis();                      // remember that we just heard light news
    }
  }
}

// ===== 6. THE LOOP =====
// This runs over and over, very fast, for as long as the board is on.
void loop() {
  while (Serial.available()) {                   // while there are letters waiting on the USB cable
    char c = Serial.read();                      // take one letter
    if (c == '\r') continue;                     // ignore this invisible letter
    if (c == '\n') {                             // a "new line" letter means the message is finished
      line[len] = 0;                             // close the text
      if (len > 0) parseLine();                  // if the message is not empty, understand it
      len = 0;                                   // empty the box for the next message
    }
    else if (len < (int)sizeof(line) - 1) line[len++] = c;   // otherwise add the letter to the box
    else len = 0;                                // the message was too long, throw it away
  }

  checkButton();                                 // did someone press the button?

  bool alive = (millis() - lastMsg) < 3000;      // true if Processing sent heights in the last 3 seconds

  for (int k = 0; k < N; k++) {                  // do this for each of the five motors
    if (mode == MODE_PAUSE) st[k].moveTo(angleToSteps(QUIET_ANGLE));    // PAUSE: hold at the quiet angle
    else st[k].moveTo(alive ? target[k] : angleToSteps(QUIET_ANGLE));   // MUSIC: go to the target, or back to rest if the laptop is quiet
    st[k].run();                                 // take one tiny step if one is due
    if (st[k].distanceToGo() == 0) st[k].disableOutputs();   // arrived: switch the coils off to save power and heat
  }
  updateLights();                                // update the NeoPixel colors
}