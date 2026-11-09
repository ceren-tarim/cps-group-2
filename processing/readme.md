# processing

This folder holds the Processing sketch `wave_to_motors_final`, the brain of the waves. It receives the loudness and rhythmic activity from the web page and the three numbers (Speed, Fade and Height) that Wekinator predicts. From these it works out how high each of the five strips should be, so that strip 1 follows the music and the other four are pulled along like a wave. It also draws the strips on the screen, works out the light mood, and sends the heights and the light information to the ESP32-S3 over USB serial.

Two settings at the top matter when you run it: `USE_SERIAL` (false to test with no motors, true to drive the real box) and `PORT_INDEX` (the number of the ESP32-S3 in the list of serial ports that Processing prints). You also need the oscP5 library.
