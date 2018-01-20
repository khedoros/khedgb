# khedgb
Adventures in Game Boy emulation (Or, What Khedoros Likes To Do In His Not-so-abundant Free Time)

## Current state:

Summary: Some games boot and show start screens/demos. Some just show garbled crap or nothing at all. None of my test ROMs are currently crashing the emulator, which is cool.

CPU is nominally finished, but I suspect there are bugs (timing, edge cases, etc). Blargg cpu_inst tests pass (except for the timer, which I haven't written yet).

Memory Map is fairly fleshed-out, but not complete, and some of the peripheral hardware isn't being emulated yet.

ROM loader is basic (not many checks, for example). Onboard ROM+RAM work, and I've got a couple mappers written.

Timers and their related interrupts don't work yet (the timer clocking code being absent), controller input isn't read from SDL, and the link cable isn't emulated (except for on a basic level, for Blargg test serial line outputs). The other LCD interrupts are at least approximately working.

OAM DMA is written, but OAM is in the memmap, instead of the LCD controller. TODO: move it. Display output is currently just a dump of the LCD RAM's state to file, as a PGM image. It renders the background, but not the window or sprites.

My starting plan is to write this as a DMG emulator and expand to CGB either when I feel like the core emulator is solid, or I start getting bored. It should be comparatively painless to do the upgrade at a later time.

It'll use SDL2. Maybe there'll be a web-compiled version someday ;-)
