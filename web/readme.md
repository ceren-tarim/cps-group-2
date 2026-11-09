# web

This folder holds the two programs that listen to the music.

`index.html` is the page you open in Chrome. It plays a song to your headphones, or listens to the room through a USB microphone, and measures the music 40 times a second: how strong the bass, mids and highs are, the loudness, the tempo, the brightness and the rhythmic activity. It also shows the five strips and level meters on screen.

`bridge.py` is a small helper that runs on the laptop. It serves the page at `localhost:8000`, takes the numbers from the page and sends them on to Wekinator and Processing as OSC messages. The two files must stay in the same folder, because the bridge serves the page from its own folder.

To run it: `py -m pip install websockets python-osc` once, then `py bridge.py`, then open `http://localhost:8000` in Chrome.
