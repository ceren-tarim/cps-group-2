# wekinator

This folder holds the machine learning part. Wekinator learns from examples we recorded: five numbers that describe the music (low, mid, high, tempo and brightness) go in, and three numbers come out (Speed, Fade and Height), which tell Processing how the waves should behave.

* `WekinatorProject_Sound_To_Sea.wekproj` is the trained project. Open it in Wekinator (File, Open project), then click Run.
* The file of `.arff` is the training data. ARFF files are plain text tables: each row is one example we recorded, with 5 input numbers followed by 3 output numbers.

We recorded about 15 seconds of examples for each of three demo songs, and set the three output sliders by hand for each type of song. The full training table and the reasons for choosing this model are in the main README, in the machine learning section.
