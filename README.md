# 🌊 Sound to Sea

Sound to Sea turns songs into ocean waves. Five strips rise and fall under one stretchy sheet, with a flat moiré pattern layer above and NeoPixel lights whose colour follows the mood of the song. Strip 1 follows the music and the other four are pulled along like a wave, so a sound wave becomes an ocean wave.

This repository holds all the source code for the project, and documents the mechanics, hardware, software pipeline and machine learning behind it, so the work can be understood without an in person demo.

> Course: Cyber Physical Systems, Innovation Design Engineering (Royal College of Art & Imperial College London)

> Team: Juan, Jeonga, Ceren

<!-- TODO: add a photo of the finished box -->

---

## Contents

1. [Concept](#1-concept-ilo1)
2. [How the system works](#2-how-the-system-works-ilo1-ilo4)
3. [Repository structure](#3-repository-structure-ilo4)
4. [Mechanical design](#4-mechanical-design-ilo4)
5. [Hardware design](#5-hardware-design-ilo4)
6. [Software design](#6-software-design-ilo4)
7. [Machine learning](#7-machine-learning-ilo2-ilo3)
8. [How to run it](#8-how-to-run-it)
9. [What we tested and what we did not](#9-what-we-tested-and-what-we-did-not)
10. [Problems we hit and how we solved them](#10-problems-we-hit-and-how-we-solved-them)
11. [Credits](#11-credits)

---

## 1. Concept (ILO1)

Four rules shaped every decision:

1. There are several strips.
2. The height of each strip is controlled separately.
3. Heights imitate sound wave mechanics, so sound waves become ocean waves.
4. The piece is horizontal, low and simple. No servos.

Brief limits we designed within: the main output is mechanical movement, the product reacts within seconds, it fits inside 605 x 370 x 280 mm, interaction happens within 1 m, setup takes 15 minutes or less, there are no live plants, liquids or projectiles, and power is the provided 5V power bank (2 A max).

---

## 2. How the system works (ILO1, ILO4)

```
index.html  →  bridge.py  →  Wekinator  →  wave_to_motors_final.pde  →  ESP32-S3  →  motors and lights
 (browser)     (laptop)      (ML model)         (Processing)          (USB serial)
```

1. A song plays in the browser page through headphones, or the page listens to the room through a USB microphone.
2. The page measures loudness, low, mid, high, tempo, brightness and rhythmic activity 40 times a second (`index.html`).
3. `bridge.py` forwards these as OSC messages: `/sea` to Processing (port 12000) and `/wek/inputs` to Wekinator (port 6448). The `/sea` message has a sixth number, the rhythmic activity, which only Processing uses for the lights. Wekinator's five inputs are unchanged.
4. Wekinator returns three numbers, Speed, Fade and Height (0 to 1), to Processing on `/wek/outputs`.
5. Processing runs the wave maths, makes the height differences stronger, and sends the ESP32-S3 two kinds of lines over USB serial, 20 times a second:
   * `H a b c d e` the five strip heights, each from 0 to 1000
   * `C mood speed low mid high` the light information, each from 0 to 1000
6. The ESP32-S3 moves the five motors and draws the moving colour gradient on the NeoPixels.

A push button on the box pauses the strips (see [Button](#button)).

---

## 3. Repository structure (ILO4)

```
  README.md
  esp32/
    esp32_s3_simplified_final_with_button/
      esp32_s3_simplified_final_with_button.ino   final code for the ESP32-S3
    tests/                                         test sketches, one folder each
  processing/
    wave_to_motors_final/
      wave_to_motors_final.pde                     the wave maths and serial output
  web/
    index.html                                     audio analysis page (headphones and microphone)
    bridge.py                                      serves the page and forwards data to Wekinator and Processing
  wekinator/
    WekinatorProject_Sound_To_Sea.wekproj          the trained project
    data.arff                                      the training data
  docs/                                            photos and diagrams                                        photos and diagrams
```
Two folder rules to keep in mind: Arduino and Processing both need the sketch folder to have the same name as the sketch file, and `index.html` must stay in the same folder as `bridge.py`, because the bridge serves the page from there.

| File | What it does |
| --- | --- |
| `index.html` | The page you see in Chrome, and where the feature extraction happens. Splits the sound into low (20 to 250 Hz), mid (250 to 4000 Hz) and high (above 4000 Hz) and measures the energy in each. Loudness is the total energy, smoothed so it rises fast on a beat and falls slowly. Tempo is found by looking for repeating beats in the bass over the last 8 seconds (70 to 160 bpm). Brightness is how much of the sound is treble. Rhythmic activity is how much the bands rise from one moment to the next, averaged over about 10 seconds. Most numbers are stretched to 0 to 1 against the last 20 seconds of the song, so the page needs about 20 seconds to settle. |
| `bridge.py` | A small helper that runs on the laptop. It serves the page at `localhost:8000`, takes the numbers from the page and forwards them to Wekinator and Processing. |
| `wave_to_motors_final.pde` | The Processing sketch, the brain of the waves. Receives loudness and activity from the page and Speed, Fade and Height from Wekinator, works out how high each strip should be, shows them on screen, works out the light mood, and sends the heights and the light mood to the ESP32-S3. |
| `esp32_s3_simplified_final_with_button.ino` | The ESP32-S3 code for the five motors, the NeoPixels and the button, written in plain angles. Needs the AccelStepper and Adafruit NeoPixel libraries. |
| `tests/` | Small sketches to test one thing at a time: `test_type_angle_final_final` (type an angle for one strip, with the lights), `test_type_angle_final_final_with_button` (the same with the push button) and `test_neopixel_only` (only the lights). |

---

## 4. Mechanical design (ILO4)

Each of the five strips is moved by its own small motor using a **rack and pinion**. A motor turns a small gear (a pinion), and the gear pushes a toothed rack up and down, so the strip rises and falls. We chose this mechanism because it turns the continuous rotation of a cheap stepper motor into a straight, controllable up and down movement, with no springs or linkages to tune, and it keeps the box low and flat (rule 4).

* **Stroke:** the rack is limited by its length and its guide. The first structure had about 2 cm of travel (0 to 140 degrees of motor turn, about 7 degrees per millimetre). The current, taller structure uses 30 to 355 degrees (5 degree left for margin). The code never sends a motor outside these two limits.
* **Home position:** before power on, every strip is pushed down by hand to its lowest point. The motors have no position sensor, so the code assumes this is where they start.
* **Making the wave readable:** with a short stroke, small height differences are hard to see. Processing adds two steps after the wave maths, BOOST and SPREAD (see [Software design](#6-software-design-ilo4)). A longer rack and a bigger gear would give more travel than 2 cm.

### What we modified from the provided kit

The provided kit is built around servos and an Arduino Nano Every. We changed it in three ways:

* **Actuators:** we replaced the servos with five 28BYJ-48 stepper motors, each driving its own rack and pinion, so every strip has its own controlled up and down movement.
* **Controller:** we replaced the Arduino Nano Every with an ESP32-S3, because five motors need 20 pins and the AI part can also run on this chip.
* **Structure:** we built a new low, wide box with five strips under one stretchy sheet and a flat moiré layer, instead of an arm.

### The wave rule

Two rules decide every height:

* **Height to motor:** `angle = QUIET_ANGLE + (LOUD_ANGLE - QUIET_ANGLE) x number / 1000`, then `steps = angle x 4096 / 360`. A number of 0 is the lowest point and 1000 is the highest.
* **The wave:** strip 1 follows loudness x Height. Strips 2 to 5 are pulled by their neighbours like weights joined by springs, with friction, so the wave travels along and fades.

| Wave setting | What decides it |
| --- | --- |
| Wave speed | Song tempo, scaled by the Wekinator Speed output |
| How fast the wave fades | Wekinator Fade output |
| How tall strip 1 gets | Wekinator Height output |

---

## 5. Hardware design (ILO4)

### Components

| Component | Role |
| --- | --- |
| ESP32-S3 (board "ESP32S3 Dev Module") | Runs the motor and light code, talks to the laptop over USB. Chosen over an Arduino Nano Every because it has enough pins for 20 motor wires and can also run the AI part later. |
| 5 x 28BYJ-48 5V stepper motors (ROHS STEP MOTOR 28BYJ-48 5V DC) | One per strip. A stepper moves in exact steps, so we can set a strip to an exact height without a position sensor. |
| 5 x ULN2003 driver boards | Sit between the ESP32-S3 and each motor. The ESP32-S3 pins cannot supply enough current for the motor coils, the driver board can. |
| NeoPixel strip (12 pixels) | Colour changes with the mood of the song. |
| 330 ohm resistor | In the NeoPixel data line to protect the first pixel. |
| Push button | Pauses and resumes the strips. |
| USB lavalier microphone (optional) | Listens to the room instead of using headphones. |
| 5V power bank (2 A max) | Powers the motors and the NeoPixels. |

### Why 20 pins

A 28BYJ-48 motor has four small electromagnets. To turn it, the ESP32-S3 switches them on and off in a pattern thousands of times a second. The driver board has four inputs (IN1 to IN4), one per magnet, so each motor needs 4 wires, and 5 motors x 4 = 20. The NeoPixels use one more pin and the button one more.

### Pin table

| Strip | IN1 | IN2 | IN3 | IN4 |
| --- | --- | --- | --- | --- |
| 1 | GPIO 4 | GPIO 5 | GPIO 6 | GPIO 7 |
| 2 | GPIO 15 | GPIO 16 | GPIO 17 | GPIO 18 |
| 3 | GPIO 8 | GPIO 3 | GPIO 9 | GPIO 10 |
| 4 | GPIO 11 | GPIO 12 | GPIO 13 | GPIO 14 |
| 5 | GPIO 38 | GPIO 39 | GPIO 40 | GPIO 41 |

| Other | Pin |
| --- | --- |
| NeoPixel data (DIN) | GPIO 1, through a 330 ohm resistor |
| Button | GPIO 2 and GND (internal pull up, no resistor needed) |

In the code the pins are listed in the order IN1, IN3, IN2, IN4, which is the order the AccelStepper library needs for this motor type. GPIO 3 is a strapping pin, so move strip 3 if the board acts strangely at start up.

<img width="1200" height="1030" alt="esp32_signals" src="https://github.com/user-attachments/assets/279b1cfe-108f-4ad3-adcf-5adee0242373" />

<img width="1200" height="840" alt="esp32_power" src="https://github.com/user-attachments/assets/14d1327b-6b4d-4fb1-baf1-b55f2313a428" />

### Power and safety

* The motors and the NeoPixels are powered from the **5V power bank**, never from the ESP32-S3 5V pin. The ESP32-S3 is powered by the laptop over USB.
* The power bank GND is joined to the ESP32-S3 GND. Without a shared ground nothing works.
* Each motor needs roughly 100 to 200 mA, so five motors should stay around 1 A or less, which fits the 2 A limit of the bank (this is an estimate, we have not measured the real current yet).
* NeoPixel brightness is capped at 50 of 255 (`LIGHT_BRIGHTNESS`) to leave room for the motors.
* The code switches a motor's coils off when it has arrived (`disableOutputs`), which saves power and keeps the motors from getting hot.
* Motor speed is limited (`SPEED`, `ACCEL`), because motors that go too fast lose steps and strips drift to different heights.
* Wire and test one motor at a time, and unplug USB and the power bank before changing any wire.
* Some power banks switch off when the motors are at rest and draw almost nothing. Watch for this.
* Switch off the power bank last. Never switch it off while a song is still playing, or the strips can drop suddenly.

### Button

The button is wired between GPIO 2 and GND. It works as a pause button:

* **At power on:** the strips follow the music.
* **First press:** every strip goes back to the lowest angle and stays there. The music is ignored for the strips.
* **Second press:** the strips follow the music again.

The lights keep following the music in both states.

---

## 6. Software design (ILO4)

The full pipeline runs from the browser to the ESP32-S3, as shown in [How the system works](#2-how-the-system-works-ilo1-ilo4). Everything past the provided examples was written or changed by us.

### What we added or changed beyond the provided examples

* **The page and bridge:** our own audio analysis page and OSC bridge, with demo songs, file input, tab sharing and a radio stream.
* **Microphone option:** two welcome buttons, "I am wearing headphones" and "I am using a microphone". The microphone goes straight to the listening screen. The browser's echo cancellation, noise suppression and automatic gain are turned off because they would flatten the music, and the sound goes into the analyser only, never to the speakers. A silence gate (`MIC_GATE`) stops quiet room noise from becoming big waves.
* **Wave physics in Processing:** a chain of weights joined by springs with friction, controlled by the three Wekinator outputs.
* **BOOST and SPREAD:** after the wave maths, `BOOST` stretches the heights so the strips use more of their range, and `SPREAD` pushes every strip away from the average height. Neither changes the wave itself, only the numbers sent to the motors.
* **Stillness when there is no song:** with no song the strips settle at the bottom and stay still, and rise smoothly when a song starts. `FAKE_MUSIC` (false) brings back an invented test wave when needed.
* **Light mood from rhythmic activity:** see below.
* **Plain angle motor code on the ESP32-S3:** all limits are in degrees, `QUIET_ANGLE`, `LOUD_ANGLE`, `MIN_ANGLE`, `MAX_ANGLE`, `SPEED` and `ACCEL` are at the top of the file and every line is commented.
* **Pause button** on GPIO 2 with a 50 ms debounce.
  
### The Web UI
<img width="1248" height="652" alt="Screenshot 2026-11-09 190116" src="https://github.com/user-attachments/assets/f4f5b764-ab01-4a62-a8ac-3e320edd886a" />

### Headphones Option
<img width="1135" height="639" alt="Screenshot 2026-11-09 190124" src="https://github.com/user-attachments/assets/a3d04433-8025-41ba-86ca-0e9c292be735" />

### Microphone Option
<img width="1134" height="618" alt="Screenshot 2026-11-09 190142" src="https://github.com/user-attachments/assets/9224af83-38f8-4aa1-9f73-9b8699a473c9" />

### The lights

A colour gradient flows along the pixels, like on a gaming keyboard. Calm music gets cool colours, like still water, and lively music gets warm colours, like a sunset.

| Song feel | Gradient colours |
| --- | --- |
| Calm | cool deep sea blue, teal and seafoam |
| Lively | warm red orange, orange and golden yellow |

The mood comes from **rhythmic activity**, which tells how punchy the music is. We first tried loudness and tempo, but loudness is stretched per song so it is about the same for every song, and the tempo guess is often wrong, so calm songs sometimes got the lively colours. Rhythmic activity adds up how much the low, mid and high bands rise from one moment to the next (only increases count) and averages it over about 10 seconds. It is not stretched per song, so it can tell calm from lively. On the three demo songs it read 0.005 (Slow tide), 0.021 (Deep bass) and 0.051 (Storm beat). Processing maps it between `ACT_LOW` and `ACT_HIGH` to a mood from 0 (calm) to 1 (lively), averaged over a few seconds so the colours do not jump.

Processing sends the mood, the flow speed and the low, mid and high strength to the ESP32-S3. Low, mid and high make the three colours of the gradient glow, each group of pixels gets brighter when its strip rises, and if nothing arrives for 3 seconds the lights go back to a slow cool flow.

Limit: this is an estimate of the energy of the song from how punchy it is, not real emotion detection. A fast sad song can read as lively.

### Source code

All source code is in this repository (see [Repository structure](#3-repository-structure-ilo4)). The libraries needed are AccelStepper and Adafruit NeoPixel for the ESP32-S3, `websockets` and `python-osc` for the bridge, and Processing with the oscP5 library.

---

## 7. Machine learning (ILO2, ILO3)

### What the model does

Wekinator learns a mapping from five numbers describing the music to three numbers that describe how the waves should behave.

| Inputs (5) | Meaning |
| --- | --- |
| Low | 20 to 250 Hz energy, stretched 0 to 1 over the last 20 s |
| Mid | 250 to 4000 Hz energy |
| High | energy above 4000 Hz |
| Tempo | 70 to 160 bpm mapped to 0 to 1 |
| Brightness | how much of the sound is high |

| Outputs (3) | Meaning |
| --- | --- |
| Speed | how fast the wave travels |
| Fade | how quickly it dies down |
| Height | how tall strip 1 gets |

### Training data

We recorded examples while playing the page's three built in demo songs, which give clearly different inputs. For each song we waited about 20 seconds (so the page had settled), set the three output sliders by hand, and recorded about 17 to 18 seconds of examples. Wekinator records about 20 examples a second, which gave 1,053 examples in total.

| Song | Speed | Fade | Height | Examples |
| --- | --- | --- | --- | --- |
| Slow tide (calm) | 0.2 | 0.2 | 0.6 | 333 |
| Storm beat (energetic) | 0.8 | 0.8 | 1.0 | 355 |
| Deep bass | 0.5 | 0.2 | 1.0 | 365 |

Wekinator project type: all continuous, 5 inputs on `/wek/inputs` (port 6448), 3 outputs on `/wek/outputs` (port 12000).

### Where the data is

The training data is saved by Wekinator as an ARFF file, a plain text table. Each row is one recorded example: the 5 input numbers (low, mid, high, tempo, brightness) followed by the 3 output numbers (speed, fade, height), plus an ID, a time stamp and a training round number.

* Training data: [`wekinator/data.arff`](wekinator/data.arff)
* Trained project: [`wekinator/WekinatorProject_Sound_To_Sea.wekproj`](wekinator/WekinatorProject_Sound_To_Sea.wekproj)

To use it, open Wekinator, choose File, Open project, select the `.wekproj` file, and click Train and then Run.

We did not add real songs to the training. The model was trained only on the three demo songs and then tested by running the full chain with other songs.

### Why this model

* **We have very little data**, a few minutes of examples we recorded ourselves. Wekinator is built for small, hand made datasets and learns in a second or two.
* **The task is regression, not classification.** We do not want to label a song as "calm" or "energetic". We want smooth numbers, so a song between two types gives a movement between the two types.
* **It is interactive.** If two songs look alike, we record more examples and retrain in seconds, with no code. This fitted the short project timeline.
* **The outputs go straight into a physical system**, so predictable, smooth numbers matter more than a complex model.

The model is Wekinator's neural network, the default for continuous outputs, which we did not change. We chose it because the link from the five music numbers to our three wave settings is not a straight line, for example a song that is both loud and bright should not simply add up the effect of loudness and the effect of brightness, and a neural network can learn curved relationships like this. It still trains in a second or two on about a thousand examples, and it gives smooth outputs between the example songs.

### Limits we found

The tempo reading is unreliable on songs without a clear beat (Slow tide read about 130 bpm), so the model leans on brightness and the low, mid and high numbers to tell songs apart. Rhythmic activity is not an input to Wekinator, it is used only for the lights.

### Possible improvements

* **More data:** many real songs across genres, recorded by different people, would make the model less dependent on three demo songs and handle songs it has never seen.
* **A Mood output:** train a fourth output in Wekinator on top of rhythmic activity, so the colours are learned as well as calculated.
* **Other model types:** a model that looks at a window of the last few seconds instead of one moment (for example a small recurrent or sequence model) could understand rhythm and build up, instead of reacting to each instant.
* **Better features:** a more reliable tempo and beat tracker, or learned audio features, would replace the weak tempo input.
* **Real emotion data:** training on songs labelled by listeners would make the mood closer to how people feel the music.

---

## 8. How to run it

### Install once

* Arduino IDE with the ESP32 board package, and the **AccelStepper** and **Adafruit NeoPixel** libraries
* Processing with the **oscP5** library
* Wekinator
* Python 3, then in a terminal: `py -m pip install websockets python-osc`

### Boot sequence

1. **Hardware (with motors):** switch everything off and push all five strips down by hand to their lowest point. Plug the ESP32-S3 into the laptop with USB, then switch on the power bank.
2. **Arduino:** load the arduino code to the ESP32-S3, and close the Serial Monitor.
3. **Wekinator:** open it, load `WekinatorProject_Sound_To_Sea.wekproj` (File > Open) and click **Run**.
4. **Processing:** open `wave_to_motors_final.pde`. Set `USE_SERIAL = false` if you have no motors, or `true` with motors. Set `PORT_INDEX` to the number of your ESP32-S3 port in the printed list. Press Play. With motors, the console says "Arduino connected". Wait about 5 seconds, because the ESP32-S3 restarts.
5. **Bridge:** in a terminal, go to the \web folder and run `py bridge.py`. Leave the window open.
6. **Page:** open `http://localhost:8000` in Chrome, in one tab only. Click "I am wearing headphones" (or "I am using a microphone", which skips the next step). The terminal should print "page connected" once.
7. **Song:** click a demo song, or your own file, or share a tab. Keep the tab visible, browsers slow down hidden tabs.
8. **Wait 20 seconds.** After that the strips on the page, in Processing and (with motors) on the real box should rise and fall together, with the wave travelling from strip 1 to strip 5.
9. **Shut down in this order:** click Stop in the page, press Ctrl+C in the terminal, stop Processing, then switch off the power bank.

### Uploading the ESP32-S3 code

Board: ESP32S3 Dev Module. Choose the right port in Tools, Port. Close the Serial Monitor and stop Processing before uploading.

### Testing without Wekinator and motors

Set `USE_SERIAL = false` in Processing, run `py bridge.py`, and open the page. Everything up to Processing works on its own.

### Testing the hardware one piece at a time

Upload the test sketches in `esp32/tests/`. For example, type `1 90` in the Serial Monitor (115200 baud, New Line) and strip 1 should turn to 90 degrees. Test each strip, then upload the final code.

---

## 9. What we tested and what we did not

**Tested**

* The page, bridge and Processing preview work end to end.
* Wekinator is trained and runs with the full chain.
* All five motors turn, tested one at a time and together.
* Processing drives the motors over USB serial, and the NeoPixel gradient runs at the same time.
* The rack and pinion mechanism with five motors works with music on the real structure.
* Both light moods (warm and cool) work, and slow songs turn warm while fast songs turn cool.
* The microphone option opens the USB microphone and goes straight to the listening screen.
* With no song, all five strips settle at the bottom and stay still.
* The pause button toggles between following the music and holding the lowest angle.

**Not tested yet**

* The microphone in a noisy demo room, and the best `MIC_GATE` value there.
* Power draw from the bank over a long run, and heat after 10 minutes.
* The true millimetres moved per degree on the new structure.
* Wekinator on many different real songs.

---

## 10. Problems we hit and how we solved them

| Problem | Solution |
| --- | --- |
| Windows says Python not found | Use `py bridge.py` and `py -m pip install websockets python-osc`. |
| Processing red underlines | All tabs in one sketch run together and clash. Use one sketch with one tab. |
| "page connected" printing endlessly | The bridge crashed the connection when Wekinator was not open. Fixed by ignoring that error and allowing one page at a time. |
| Bridge error "only one usage of each socket address" (port 8765) | An old bridge is still running. Press Ctrl+C in the other terminal, or end the old process, then run `py bridge.py` again. |
| Arduino "redefinition" errors | Two `.ino` files in one sketch folder. Keep one `.ino` per folder, named like the folder. |
| Serial Monitor looks empty | "ready" prints only once at start up. Check 115200 baud, New Line and the USB CDC On Boot setting. |
| Upload fails with "no upload port" | Wrong port selected. Choose the right port and close the Serial Monitor and Processing. |
| Processing window stays grey | `PORT_INDEX` opened the wrong port. Use the number of the ESP32-S3 port in the printed list. |
| Motors and lights seem dead | The power bank was off. Also, motors only move while Processing sends heights. |
| Strips lose contact or turn too far | Motors coast after fast changes. Fixed with a limited speed and acceleration, a lower boost and a limited angle range. |
| One strip only went down and never came back up | That rack had more friction (or a loose wire) so it skipped steps on the way up. Fixed by loosening the guide and pushing the wires in firmly. |
| One strip ended at a different height than the others | Lost steps from too much speed or friction. Lower `SPEED` and check that the rack moves freely. |
| Strips use only the middle of their range | The motors are too slow to follow the music across the full stroke, so they turn back before they arrive. Raise `SPEED` and `ACCEL` a little, within the safe limit. |
| Lights always pink | Most songs had a light mood in the middle band. Fixed with two moods and, later, a mood from rhythmic activity. |
| Lights blue on calm songs | Loudness is stretched per song and the tempo guess is unreliable. Fixed by using rhythmic activity. |
| Strips moved with no song | Processing was inventing music after 2 seconds of silence. Fixed with `FAKE_MUSIC = false` and a slow fade to the bottom. |
| Height differences hard to see | The stroke was only about 2 cm. Boost and spread help, and a longer rack and bigger gear give a real fix. |

---

## 11. Credits

* Concept and mechanics: Juan & Jeonga 
* Machine learning, AI and Processing: Ceren
* Electronics and wiring: Juan & Jeonga
* Documentation: Ceren
* Built with Wekinator, Processing, Arduino, AccelStepper and Adafruit NeoPixel.
