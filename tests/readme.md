Small sketches we used to test one part of the hardware at a time, before running the whole system. They are useful when you build the box yourself, because if something does not work you can find out exactly which motor, wire, light or button is the problem.

* `test_type_angle_final_final`: type an angle in the Serial Monitor (for example `1 90` for strip 1 at 90 degrees) to check the range and the direction of each strip. It also runs the NeoPixels, so you can test the motors and the lights together.
* `test_type_angle_final_final_with_button`: the same angle tester with the push button added. Each press sends all the strips to the next position, so you can check that the button is wired correctly before using it in the main code.
* `test_neopixel_only`: tests only the lights, with no motors. You can sweep through the colours, set the mood, the speed and the brightness.
