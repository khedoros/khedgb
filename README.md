# khedgb
Adventures in Game Boy emulation (Or, What Khedoros Likes To Do In His Not-so-abundant Free Time)

## Current state:

Summary: Some games boot and show start screens/demos. Some just show garbled crap or nothing at all. None of my test ROMs are currently crashing the emulator, which is cool.

CPU is nominally finished, but I suspect there are bugs (timing, edge cases, etc). Blargg cpu_inst tests pass (except for the timer, which I haven't written yet). Actually, the biggest likely bug-haven is around HALT and STOP. I know some cases where those aren't working correctly.

The Memory Map is fairly fleshed-out, but not complete, and some of the peripheral hardware isn't being emulated yet (sound, timer, serial, and the connected interrupts).

ROMs load, and I added the ability to load them directly from zip files (it just finds the first file in the archive, and loads it if it's between 32KB and 8MB). Onboard ROM+RAM work, and I've got the mappers I consider important written (null, mbc1, mbc2, mbc3, mbc5). Null covers the simpler 32KB games, MBC1+2 cover a lot of early ones, MBC3 covers most of them that need to keep time (like the Pokemon games), and MBC5 covers later GameBoy games, and especially almost all of the Color games.

Timers and their related interrupts don't work yet (the timer clocking code being absent), and the link cable isn't emulated (except for on a basic level, for Blargg test serial line outputs). The other LCD interrupts are at least approximately working.

Display output renders the background, puts the window on top of that (if active), then the sprites on top of that (if they're active). Compositing isn't correct, and the rendering itself is pretty problematic (and I've gotta figure out why!)

My starting plan is to write this as a DMG (original GameBoy) emulator and expand to CGB either when I feel like the core emulator is solid, or I start getting bored. It should be comparatively painless to do the upgrade at a later time.

It's using SDL2. Maybe there'll be a web-compiled version someday ;-)
