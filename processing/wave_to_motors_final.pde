// ===== SECTION 1: TOOLS =====
/* 
It loads the three tools the sketch needs: one to receive messages from the bridge 
and wekinator, one for networking, and one to talk to Arduino over USB.
*/
import oscP5.*;
import netP5.*;
import processing.serial.*;

OscP5 osc;
Serial port;
// -----------------------------

// ===== SECTION 2: SETTINGS AND MEMORY =====
/*
Everything the sketch needs to remember while it runs: how many strips there are,
which mode is on, whether to use Arduino, the current loudness, bands and tempo, 
the five strip heights and speeds, and Wekinator's three numbers. 
The lines we might change are the MODE, USE_SERIAL and PORT_INDEX lines. 
The rest is memory for the wave simulation.
*/
final int N = 5;
int MODE = 3;                           // press 1, 2 or 3 while running to switch
boolean USE_SERIAL = true;              // false = no Arduino needed, preview on screen only
int PORT_INDEX = 8;                     // which port in the printed list is your Arduino (0 is the first one). On Windows look for the one that appears when you plug the Arduino in.
// FAKE_MUSIC: what happens when no song is playing.
// false = the strips settle at the bottom and stay completely still (use this for the demo).
// true  = Processing invents a moving wave so you can test without a song.
boolean FAKE_MUSIC = false;
float live = 0;                         // 0 = no song (strips at the bottom), 1 = song playing. It fades slowly between the two
long lastSea = 0;                       // when the last /sea message arrived
long lastWek = 0;                       // when the last Wekinator message arrived

float drive = 0;                        // loudness 0..1  (strip 1 listens to this)
float[] band3 = new float[3];           // low, mid, high, stretched 0..1
float[] bandIn = new float[N];          // the same three, spread over five strips
float bpm = 100;
float act = 0;                          // rhythmic activity from the page (how punchy the music is), about 0.004 calm to 0.05 lively

float[] h = new float[N];               // strip heights 0..1
float[] v = new float[N];               // strip speeds (MODE 3)
float[] hOut = new float[N];            // the heights that are really sent and drawn (after the height boost)

// Height boost: makes the strips use more of their range, so the differences are easier to see.
// 0 = off (as before), 1 = full stretch. Try 0.5, 0.8, 1.0.
float BOOST = 0.5;
float hiSeen = 0.6, loSeen = 0.1;       // the highest and lowest height seen lately (slow memory)

// Spread: pushes every strip away from the average height of all five, so the strips
// look MORE DIFFERENT from each other. 1 = off (as before), 1.5 = clearly different, 2 = very different.
float SPREAD = 1.6;

// Wave character, set by Wekinator (outputs 0, 1, 2). Start in the middle.
float moodSpeed = 0.5, moodFade = 0.5, moodHeight = 1.0;

float rest = 0.2;                       // calm sea level
float g = 0.85;                         // MODE 1: each strip keeps this share
float[] hist = new float[400];
int histPos = 0;
float FPS = 20;

// NeoPixel light mood (slow averages, so the colors drift instead of jumping)
// The mood now comes from rhythmic activity (how punchy the music is), not from loudness and tempo.
// ACT_LOW and ACT_HIGH set the range: a song at ACT_LOW or below reads as calm (0), at ACT_HIGH or above as lively (1).
// TO TUNE: play a slow song and a fast song and read the "activity" number in the window. Put ACT_LOW a little
// below the slow song's number and ACT_HIGH a little above the fast song's number.
float ACT_LOW = 0.005, ACT_HIGH = 0.04;
float actAvg = 0.02;                    // activity averaged over a few seconds
float moodLight = 0;                    // 0 = calm (warm gold/orange), 1 = lively (blue)
float speedLight = 0;                   // 0 = slow gradient flow, 1 = fast
// -----------------------------

// ===== SECTION 3: START-UP (SETUP) =====
/*
Runs once when you press Play. It opens the window, starts listening on port 12000,
prints the list of USB ports, tries to open the Arduino's port (and swithces to
preview only if that fails), and puts every strip at the calm level.)
*/
void setup() {
  size(600, 260);
  frameRate(FPS);
  osc = new OscP5(this, 12000);
  String[] ports = Serial.list();
  for (int i = 0; i < ports.length; i++) println("port " + i + " = " + ports[i]);
  if (USE_SERIAL) {
    try {
      port = new Serial(this, ports[PORT_INDEX], 115200);
      println("Arduino connected on " + ports[PORT_INDEX]);
    } catch (Exception e) {
      println("Could not open port number " + PORT_INDEX + ". Close the Arduino Serial Monitor, check the list above, and change PORT_INDEX. Showing the preview only.");
      USE_SERIAL = false;
    }
  }
  for (int k = 0; k < N; k++) h[k] = rest;
}
// -----------------------------

// ===== SECTION 4: LISTENING (oscEvent) =====
/*
Runs by itself every time a message arrives. Messages named /sea carry the music
numbers (loudness, low, mid, high, tempo). Messages named /wek/outputs carry 
the Wekinator's Speed, Fade and Height and are stored too. Every number is kept
in a safe range.
*/
void oscEvent(OscMessage m) {
  if (m.checkAddrPattern("/sea") && m.arguments().length >= 5) {
    lastSea  = millis();
    drive    = constrain(m.get(0).floatValue(), 0, 1);
    band3[0] = constrain(m.get(1).floatValue(), 0, 1);
    band3[1] = constrain(m.get(2).floatValue(), 0, 1);
    band3[2] = constrain(m.get(3).floatValue(), 0, 1);
    bpm      = constrain(m.get(4).floatValue(), 60, 180);
    if (m.arguments().length >= 6) act = max(0, m.get(5).floatValue());   // the 6th number is the rhythmic activity
  } else if (m.checkAddrPattern("/wek/outputs") && m.arguments().length >= 3) {
    lastWek    = millis();
    moodSpeed  = constrain(m.get(0).floatValue(), 0, 1);
    moodFade   = constrain(m.get(1).floatValue(), 0, 1);
    moodHeight = constrain(m.get(2).floatValue(), 0, 1);
  }
}
// -----------------------------

// ===== SECTION 5: DRAWING (draw), in five parts =====
//draw() runs 20 times per second and is the heart of the sketch. 
//Sections 5A and 5B all sit inside the same draw(), one after another.
//Section 5A opens it and Section 5E closes it. 

// Part 5A: Fake music and preparing the numbers 
/*
If no music has arrived for 2 seconds, it invents a loudness so you can test
without a song. Then it saves the loudness in the memory (for Mode 1), spreads
the low,mid and high bands over the five strips, and calculates two helper
values: the length of one beat and the Height gain.
*/
void draw() {
  background(250, 247, 240);
  // no music numbers for 2 seconds means there is no song
  boolean noSong = millis() - lastSea > 2000;
  if (noSong && FAKE_MUSIC) {          // only when FAKE_MUSIC is true: make up a loudness so you can test without a song
    float t = millis() / 1000.0;
    drive = constrain(0.5 + 0.3 * sin(t * 1.1) + 0.2 * sin(t * 2.7), 0, 1);
    for (int k = 0; k < 3; k++) band3[k] = 0.5 + 0.4 * sin(t * (0.6 + 0.3 * k) + k);
    act = 0.5 * (ACT_LOW + ACT_HIGH);                       // fake music gets a middle activity
  } else if (noSong) {                 // no song and no fake music: silence, so nothing makes the waves move
    drive = 0;
    for (int k = 0; k < 3; k++) band3[k] = 0;
    act = 0;
  }
  hist[histPos] = drive; histPos = (histPos + 1) % hist.length;

  // spread the 3 bands over the 5 strips: S1 low ... S3 mid ... S5 high
  for (int k = 0; k < N; k++) {
    float p = k * 2.0 / (N - 1);
    int a = floor(p), b = min(a + 1, 2);
    bandIn[k] = lerp(band3[a], band3[b], p - a);
  }

  float beat = 60.0 / bpm;                                   // seconds per beat
  float gain = map(moodHeight, 0, 1, 0.6, 1.0);
  // -----------------------------

// Part 5B: Mode 1 and Mode 2 (the simple ways)
/*
Mode 1 makes each strip repeat strip 1's loudness from earlier and a bit smaller,
like echoes. Mode 2 lets each strip just copy its own sound band. You only see
these if you press 1 or 2. 
*/
  if (MODE == 1) {
    int delayFrames = max(1, round(beat * FPS));             // one beat between strips
    for (int k = 0; k < N; k++) {
      int idx = (histPos - 1 - k * delayFrames + 10 * hist.length) % hist.length;
      h[k] = constrain(pow(g, k) * hist[idx] * gain, 0, 1);
    }
  } else if (MODE == 2) {
    for (int k = 0; k < N; k++) h[k] = bandIn[k];
  } else {
    // Part 5C: Mode 3 (the wave simulation)
    /*
    Strip 1 is set straight from the loudness. The other four are like weights 
    joined by springs: each one is pulled by its neighbors (so the wave travels),
    slowed by friction, drawn back to the calm level, and nudged a little by its 
    own sound band. Wekinator's Speed changes how strongly neighbors pull, and 
    Fade changes the friction. All of this is calculated in 10 tiny steps per
    frame to keep it smooth.
    */
    // wave speed follows the tempo (faster song, faster wave), the Wekinator speed output scales it 0.6x to 1.4x
    float waveSpeed = constrain(map(bpm, 60, 160, 5, 12) * map(moodSpeed, 0, 1, 0.6, 1.4), 2, 16);
    float friction  = map(moodFade, 0, 1, 0.6, 3.0) * 0.6;   // lower = waves travel further along the strips
    float ownBand   = 0.8;                                    // how much each strip follows its own band
    int sub = 10; float d = (1.0 / FPS) / sub;
    for (int s = 0; s < sub; s++) {
      h[0] = constrain(drive * gain, 0, 1);
      for (int k = 1; k < N; k++) {
        float next = (k < N - 1) ? h[k + 1] : h[k];          // free far end
        float acc = waveSpeed * (h[k - 1] + next - 2 * h[k])
                  - friction * v[k]
                  - 0.6 * (h[k] - rest)
                  + ownBand * (0.2 + 0.5 * bandIn[k] - h[k]) * 0.3;
        v[k] += acc * d;
      }
      for (int k = 1; k < N; k++) h[k] = constrain(h[k] + v[k] * d, 0, 1);
    }
  }

  // Part 5C+: the height boost
  /*
  Looks at the highest and lowest strip of the last few seconds and stretches everything
  between them to fill the whole range 0 to 1. The strips keep the same order and shape,
  but the highs get higher and the lows get lower. The wave calculation above is not
  touched, only the numbers that are sent to the Arduino and drawn on screen.
  */
  float topNow = 0, botNow = 1;
  for (int k = 0; k < N; k++) { topNow = max(topNow, h[k]); botNow = min(botNow, h[k]); }
  hiSeen = (topNow > hiSeen) ? lerp(hiSeen, topNow, 0.3) : lerp(hiSeen, topNow, 0.01);
  loSeen = (botNow < loSeen) ? lerp(loSeen, botNow, 0.3) : lerp(loSeen, botNow, 0.01);
  float span = max(hiSeen - loSeen, 0.25);                  // never stretch tiny differences too much
  for (int k = 0; k < N; k++) {
    float stretched = constrain((h[k] - loSeen) / span, 0, 1);
    hOut[k] = lerp(h[k], stretched, BOOST);
  }

  // Part 5C++: the spread (makes the strips differ more from each other)
  /*
  Finds the average height of the five strips, then moves each strip further away from that
  average: strips that are above it go higher, strips below it go lower. The order of the
  strips stays the same. The result is kept between 0 and 1, so the motors never leave their limits.
  */
  float mean = 0;
  for (int k = 0; k < N; k++) mean += hOut[k] / N;
  for (int k = 0; k < N; k++) hOut[k] = constrain(mean + (hOut[k] - mean) * SPREAD, 0, 1);

  // Part 5C+++: stillness when there is no song
  /*
  When no song is playing, live slowly falls to 0 and every height is multiplied by it, so all five strips
  settle at the bottom and stay still. When a song starts, live slowly rises back to 1, so the strips
  rise smoothly instead of jumping. With FAKE_MUSIC = true this does nothing (live stays 1).
  */
  live = lerp(live, (noSong && !FAKE_MUSIC) ? 0 : 1, 0.08);
  for (int k = 0; k < N; k++) hOut[k] *= live;

  // Part 5D: Sending to Arduino
  /*
  It builds one line of text like H 820 450 300 260 210 (five heights from 1 to 
  1000) and sends it to Arduino. The \n at the end tells Arduino the line is
  finished.
  */
  // send "H a b c d e" with heights 0..1000
  String msg = "H";
  for (int k = 0; k < N; k++) msg += " " + int(constrain(hOut[k], 0, 1) * 1000);
  if (USE_SERIAL) port.write(msg + "\n");

  // Part 5D (continued): the light mood for the NeoPixels
  /*
  Works out how lively the song feels from the rhythmic activity (how punchy the music is),
  averaged over a few seconds so the colors change slowly. 0 means calm (warm orange and gold),
  1 means lively (blue and purple). It sends one line like C 350 400 600 500 300 to the Arduino with the mood,
  the flow speed and the low, mid and high levels. The Arduino draws the moving gradient.
  */
  actAvg = lerp(actAvg, act, 0.02);                                      // slowly follow the activity
  float energy = constrain(map(actAvg, ACT_LOW, ACT_HIGH, 0, 1), 0, 1);  // activity turned into 0 (calm) to 1 (lively)
  moodLight  = lerp(moodLight, energy, 0.03);                            // the mood moves slowly toward that energy
  speedLight = constrain(moodLight, 0, 1);                               // the gradient flows faster for livelier songs
  if (USE_SERIAL) {
    port.write("C " + int(moodLight * 1000) + " " + int(speedLight * 1000) + " "
               + int(band3[0] * 1000) + " " + int(band3[1] * 1000) + " " + int(band3[2] * 1000) + "\n");
  }

  // Part 5E: The picture on screen
  /*
  Draws five orange bars that rise and fall with the heights, labels them S1 to S5,
  and writes the mode, loudness and tempo at the top. This is what we see in the
  Processing window. The final } closes the draw() function.
  */
  // preview
  for (int k = 0; k < N; k++) {
    fill(180, 90, 40); noStroke();
    float bh = hOut[k] * 160;
    rect(60 + k * 100, 220 - bh, 60, bh, 6);
    fill(40); text("S" + (k + 1), 82 + k * 100, 240);
  }
  fill(40);
  text("MODE " + MODE + "   loudness " + nf(drive, 1, 2) + "   bpm " + int(bpm), 20, 20);
  boolean wekOn = millis() - lastWek < 2000;   // heard from Wekinator in the last 2 seconds?
  text(wekOn ? "Wekinator ON   speed " + nf(moodSpeed, 1, 2) + "   fade " + nf(moodFade, 1, 2) + "   height " + nf(moodHeight, 1, 2) : "Wekinator OFF (middle values)", 20, 38);
  text("light mood " + nf(moodLight, 1, 2) + "  (0 = calm gold, 1 = lively blue)   activity " + nf(actAvg, 1, 3), 20, 56);}

// ===== SECTION 6: KEYBOARD (keyPressed) =====
/*
Lets you switch between the three modes by pressing 1, 2 or 3 while the sketch
 is running. The new mode is shown at the top of the window and used immediately.
*/
void keyPressed() {
  if (key == '1') MODE = 1;
  if (key == '2') MODE = 2;
  if (key == '3') MODE = 3;
}