# khedgb
Adventures in Game Boy emulation (Or, What Khedoros Likes To Do In His Not-so-abundant Free Time)

## Plan

This starts as a DMG (original GameBoy) emulator and expand to CGB either when I feel like the core emulator is solid, or I start getting bored. The color hardware is mostly a direct extension of the Game Boy system and is backwards compatible with its software. I'll also branch into peripheral support, as the mood strikes.

## Current state

Most games are booting and running (I'll claim 95% booting without issue, and 70% running stably during gameplay and without major visible bugs). Most original Game Boy games are playable. Saving and loading games (not save states) is basically working, including with save files from other emulators (tested with BGB, for example). Timing issues abound. Some games may do things that I haven't tested, and crash for that reason.

### CPU

The CPU seems stable, although there might be timing issues (like the 0xCBxx instruction timing that I just fixed). Blargg cpu_inst tests pass, and so do a lot of the ones related to timing.

### Memory Map

The Memory Map is fairly fleshed-out, but not complete, and some of the peripheral hardware isn't being emulated yet (sound is mostly absent, the timer is new code, serial is only partially implemented). Undocumented registers may or may not react as on a real system. I've implemented some of them, but not all.

### ROM mapping

ROMs load, and I added the ability to load them directly from zip files (it just finds the first file in the archive, and loads it if it's between 32KB and 8MB). Onboard ROM+RAM work, and I've got the mappers I consider important written (null, mbc1, mbc2, mbc3, mbc5, camera). Null covers the simpler 32KB games, MBC1+2 cover a lot of early ones, MBC3 covers most of them that need to keep time (like the Pokemon games), and MBC5 covers later GameBoy games, and especially almost all of the Color games. MBC3 doesn't correctly support the clock chip, or saving and loading the time in save games. The Camera mapper has just enough to let the cartridge boot and avoid crashing.

### Interrupts

The link cable isn't fully emulated. There's a kludge for Blargg test serial line outputs and basic support for pretending to send data. The LCD interrupts work, with approximately-correct timing (real cycle-accurate emulation relies on details that I'm not taking into account, as rendering a line takes variable amounts of time). VBlank interrupts work. Joypad interrupts work.

### Display

Display output composites the video roughly correctly, although sprite priority isn't quite right. On real hardware, it's possible to modify certain display registers during rendering. I don't support sub-line rendering changes, right now.

### Super GameBoy

There won't ever be full support; full support would mean implementing a full SNES emulator. The emulator supports drawing custom game borders, applying palette changes to the screen, screen blanking, and could be easily enhanced to support 2/4-player input. I'm leaving out SNES audio output support, various functions that adjust settings for the SGB itself, and anything that copies/run program code on the SNES (although I'll provide printed notification that those commands were received).

### GameBoy Camera

I plan to eventually support using a webcam, doing my best to adapt the behavior to be as similar to the Mitsubishi M64282FP as possible. Currently, I think I know enough to write a kind of kludgy image capture, but I'd like to support things like edge detection and other features that were specific to that camera module.

## Far Future

- The emulator is written using SDL2. Maybe there'll be a web-compiled version someday ;-)

- I think it would be cool to support an external cartridge reader, to support running from physical cartridges in realtime, without the necessity of copying the data anywhere.

### Purpose

The purpose of this emulator is not to enable or encourage copyright infringement of intellectual property, but to give me a platform to understand more about low-level software development and the design decisions that go into a piece of hardware like the GameBoy. I want to know what makes that relic of my childhood tick, the compromises that were chosen, the little hardware bugs and timing quirks that resulted, and so on. A pleasant side-effect is the ability to run games. But if playing games was the main purpose, I'd use someone else's more polished emulator, or the actual game hardware I've still got. So, that's my recommendation. If you want to follow the development of an emulator by someone making their second attempt, cool. If you just want to play games, there are myriad better options.
