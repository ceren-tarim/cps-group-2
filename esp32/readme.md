# esp32

This folder holds the code that runs on the ESP32-S3, the small board inside the box. It receives the five strip heights and the light information from Processing over the USB cable, moves the five stepper motors to the right angles, draws the moving colour gradient on the NeoPixels, and reads the pause button.

The final code is in `esp32_s3_simplified_final_with_button/`. All the numbers you might want to change (the lowest and highest angle, the motor speed and acceleration) are at the top of the file, and every line has a comment explaining what it does. Upload it with the board set to "ESP32S3 Dev Module", and close the Serial Monitor and Processing first.
