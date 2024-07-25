# morsesrc
GStreamer plugin for converting morse code text to audio

# insperation
I needed a way to convert morse code dah dit dah dit into sinewave and use in a GSTreamer pipeline. One way is to generate using the online converters to wav file and simply use filesrc into the pipeline.
However, why not try to convert a text string like "CQ CQ DE VK3DG" into dah's and dit's directly into GStreamer.

This is my first attempt of a plugin and by all means the best way to achieve what I'm doing. I'm happy for improvemnts if anyone has constructive means of making the plugin usable and used in GStreamer framework.
I do love playing with GStreamer and hopefully find others may benifit from my experience.
