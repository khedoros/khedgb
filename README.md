# khedgb
Adventures in Game Boy emulation (Or, What Khedoros Likes To Do In His Not-so-abundant Free Time)

## Current state:

Summary: Most games are booting and running. Most original Game Boy games are playable. Saving and loading games (not save states) is basically working, including with save files from other emulators (tested with BGB, for example).

The CPU seems stable, although there might be timing issues (like the 0xCBxx instruction timing that I just fixed). Blargg cpu_inst tests pass (except for the timer, which I haven't written yet). Actually, the biggest likely bug-haven is around HALT and STOP. I know some cases where those aren't working correctly. Now that I've got rendering working reliably, I should be able to run more of the test ROMs, usefully.

The Memory Map is fairly fleshed-out, but not complete, and some of the peripheral hardware isn't being emulated yet (sound, timer, serial, and the connected interrupts).

ROMs load, and I added the ability to load them directly from zip files (it just finds the first file in the archive, and loads it if it's between 32KB and 8MB). Onboard ROM+RAM work, and I've got the mappers I consider important written (null, mbc1, mbc2, mbc3, mbc5). Null covers the simpler 32KB games, MBC1+2 cover a lot of early ones, MBC3 covers most of them that need to keep time (like the Pokemon games), and MBC5 covers later GameBoy games, and especially almost all of the Color games. All of the mappers likely have cases where they will cause the program to crash, though (e.g. being passed an out-of-range value for the memory page to swap in).

The clock-divider works, but the other timers and their related interrupts don't work yet (the timer clocking code being absent), and the link cable isn't emulated (except for on a basic level, for Blargg test serial line outputs). The other LCD interrupts work, with approximately-correct timing (real cycle-accurate emulation relies on details that I'm not taking into account).

Display output renders the background, puts the window on top of that (if active), then the sprites on top of that (if they're active). Compositing isn't done correctly, but rendering itself should be pretty solid (timing is approximately correct, line-by-line special effects match what I see on my Game Boy Pocket, and so on). 

My starting plan is to write this as a DMG (original GameBoy) emulator and expand to CGB either when I feel like the core emulator is solid, or I start getting bored. It should be comparatively painless to do the upgrade at a later time (DMA will be annoying, I'll need some re-work for speed switching, but a lot of the rest will be pretty simple, I think).

It's using SDL2. Maybe there'll be a web-compiled version someday ;-)
