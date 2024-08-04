# morsesrc
GStreamer plugin for converting morse code text to audio

## inspiration 💡🌟👩‍🎨
I needed a way to convert morse code yes:- dah dit dah dit dit dah dah into a sinewave representation to use in a GStreamer pipeline. One way is to generate using the online converters to wav file and simply use filesrc into the pipeline or alternatively use fdsrc or appsrc with both needing additional logic to implement.
filesrc with decodebin was far the better method however, required the first step to generate a wave file for each morse message. 
I thought 🤔 Why not try to convert a text string like "CQ CQ DE VK3DG" into dah's and dit's and pipe into GStreamer!

This is my first attempt with building a gstreamer plugin and by all means, could cause concern with the gstreamer developers. I'm happy for improvements and hoefully my plugin does make it's way into the GStreamer hall of plugin's
I enjoy playing with GStreamer and do similar minded "amateur radio" aficionados.  I hope others find my plugin useful in their projects.

## Features

- Converts text input to Morse code audio.
- Adjustable parameters for speed (WPM), frequency, and volume.
- Generates a sine wave for Morse code tones with fade-in and fade-out of 20ms per dah and dit to mitigate clicks.

## Usage

```bash
gst-launch-1.0 morsesrc text="CQ CQ DE VK3DG" wpm=20 frequency=880.0 volume=0.5 ! audioconvert ! autoaudiosink
```

## Requirements

- GStreamer 1.0 or later
- GStreamer Plugins Base 1.0 or later
- Glib 2.0 or later
  
## Dependencies

Ensure that GStreamer and its development libraries are installed:

```bash
sudo apt-get update
sudo apt-get install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

## Installation build tools

```bash
sudo apt-get install python3-pip
pip3 install meson ninja
```
## Building

```bash
git clone https://github.com/TVforME/morsesrc
cd morsesrc
meson setup build
meson compile -C build
sudo meson install -C build

export GST_PLUGIN_PATH=/usr/local/lib/x86_64-linux-gnu:$GST_PLUGIN_PATH

gst-inspect-1.0 morsesrc
```
