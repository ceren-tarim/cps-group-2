"""
Bridge.py: Sound to Sea
What it does: 
The middle of our chain. It sits between the webpage and the programs that make
things move. 

The chain:
index.htlml -> bridge.py -> Wekinator & Processing -> ESP32-S3 -> five sterips &
neopixels

It does two  jobs: 
1. Shows the page at http://localhost:8000, because the browser only lets you 
share a tab from a page that is opened from localhost.
2. Takes the numbers from the page and sends them on as OSC messages:
    /sea   [loudness, low, mid, high, bpm, activity]
            goes to Processing (port 12000)
    /wek/inputs  [low, mid, high, tempo, brightness]
                 goes to Wekinator (port 6448)

How to run it: 
py bridge.py, then open http://localhost:8000 in Chrome.

Install once: 
py -m pip install websockets python-osc

Only one bridge can run at a time, because port 8765 can
only be used by one program. If you see Errno 10048, an old
bridge is still open. Close it with Ctrl+C and try again.
"""
import asyncio, json, threading, os, http.server, socketserver, functools
import websockets
from pythonosc.udp_client import SimpleUDPClient

# =====================================================
# PART 1: SETTINGS AND OSC CLIENTS
# =====================================================
# WHAT IT DOES:
# Finds the folder where this file lives (so it can show
# index.html from there) and opens two "mailboxes" for
# sending OSC messages: one to Processing, one to Wekinator.
# Both programs run on this same laptop, so the address is
# 127.0.0.1.
#
# TO CHANGE A PORT:
# Edit the number below, and make sure the other program
# listens on the same number.
#
# clip() keeps any number between 0 and 1, so a strange
# value from the page can never break Wekinator.
# =====================================================

HERE = os.path.dirname(os.path.abspath(__file__))
proc = SimpleUDPClient("127.0.0.1", 12000)   # Processing
wek = SimpleUDPClient("127.0.0.1", 6448)     # Wekinator inputs
clip = lambda x: max(0.0, min(1.0, float(x)))

# =====================================================
# PART 2: SAFE SENDING
# =====================================================
# WHAT IT DOES:
# One small function that sends an OSC message. Without it,
# the bridge could crash when Wekinator or Processing is
# not open yet. Now it just skips the message quietly and
# keeps going.
# =====================================================

def send(client, addr, values):
    # Windows can raise an error when nobody listens on the port yet (e.g. Wekinator not open). Ignore it.
    try:
        client.send_message(addr, values)
    except OSError:
        pass

# =====================================================
# PART 3: THE PAGE CONNECTION (WEBSOCKET)
# =====================================================
# WHAT IT DOES:
# Listens to the web page and passes its numbers on. This
# is the heart of the file.
#
# THE RULES:
# 1. Only one page may be connected at a time. If a second
#    page tries, it is refused and a message is printed
#    (on the 1st, 10th, 100th and 1000th try, so the
#    screen does not fill up).
# 2. Each message from the page is JSON with: loud, low,
#    mid, high, bright and bpm.
# 3. Every message is sent twice:
#      to Processing as /sea
#      to Wekinator as /wek/inputs
#    For Wekinator the tempo is changed to (bpm - 60) / 100,
#    so it also lives between 0 and 1.
# 4. If one message is broken, it is skipped and the next
#    one still works.
#
# THE PRINTS YOU SEE IN THE TERMINAL:
# "page connected from ..."  means the page found the bridge
# "page version: 4"          means the page is the new one
# "page disconnected"        means the tab was closed
# =====================================================

active = None      # only one page may be connected at a time
refused = 0

async def handler(ws):
    global active, refused
    if active is not None:
        refused += 1
        if refused in (1, 10, 100, 1000):
            try:
                who = ws.request.headers.get("Origin", "?")
            except Exception:
                who = "?"
            print(f"another page tried to connect ({refused} times) and was refused. It comes from: {who}")
        await ws.close(1013, "busy")
        return
    active = ws
    try:
        origin = ws.request.headers.get("Origin", "?")
    except Exception:
        origin = "?"
    print("page connected from", origin)
    first = True
    try:
        async for msg in ws:
            try:
                d = json.loads(msg)
                if first:
                    print("page version:", d.get("v", "OLD"))
                    first = False
                bpm = float(d["bpm"])
                send(proc, "/sea", [clip(d["loud"]), clip(d["low"]), clip(d["mid"]), clip(d["high"]), bpm, float(d.get("act",0))])
                send(wek, "/wek/inputs", [clip(d["low"]), clip(d["mid"]), clip(d["high"]), clip((bpm - 60) / 100), clip(d["bright"])])
            except Exception as e:
                print("skipped one message:", e)
    finally:
        active = None
        print("page disconnected")

# =====================================================
# PART 4: SHOWING THE PAGE (WEB SERVER)
# =====================================================
# WHAT IT DOES:
# A tiny web server that shows index.html at
# http://localhost:8000. It runs in the background while the
# rest of the bridge keeps working.
#
# WHY IT EXISTS:
# Chrome only allows "share a browser tab" on a page that
# comes from localhost, not from a file on the disk.
#
# The Quiet class just hides the long list of log lines the
# server would print for every file it sends.
# =====================================================

def serve_page():
    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *a): pass
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", 8000), functools.partial(Quiet, directory=HERE)) as s:
        s.serve_forever()

# =====================================================
# PART 5: START EVERYTHING
# =====================================================
# WHAT IT DOES:
# Starts the web server in the background, then opens the
# WebSocket door on port 8765, where the page connects.
# It then waits forever, until you press Ctrl+C.
#
# TO CHECK THE VERSION:
# The line "bridge version 4" tells you which copy of the
# file is running. It should match "page version: 4" from
# the page.
# =====================================================

async def main():
    threading.Thread(target=serve_page, daemon=True).start()
    print("bridge version 4. Open http://localhost:8000 in Chrome. Press Ctrl+C to stop.")
    async with websockets.serve(handler, "127.0.0.1", 8765):
        await asyncio.Future()

asyncio.run(main())
