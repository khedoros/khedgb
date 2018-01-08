# khedgb
Experiments in Game Boy emulation

Current state:

CPU is nominally finished, but I suspect there are bugs (booting the DMG ROM actually uncovered one; the documentation I got opcode info from has at least 2 errors).

Memory Map is fairly fleshed-out, but not complete, and a lot of the peripheral hardware isn't being emulated yet.

ROM loader is rudimentary (no checks, for example). The on-cartridge RAM isn't even currently emulated, because I've been planning to do it along with the loader enhancements and beginning work on the mapper chips.

I've got the beginning of the interrupt architecture, but it isn't really wired in. I'm planning on predicting when the interrupts fire to make it more efficient to poll. Similarly, the timers and cycle-counter/divider are intimately tied into the interrupt system, and haven't been written yet.

There is no DMA and display is currently just a dump of the LCD RAM's state to file, as a PGM image.

My starting plan is to write this as a DMG emulator and expand to CGB either when I feel like the core emulator is solid, or I start getting bored. It should be comparatively painless to do the upgrade at a later time.

It'll use SDL2. Maybe there'll be a web-compiled version someday ;-)
