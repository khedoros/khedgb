# khedgb
Adventures in Game Boy emulation (Or, What Khedoros Likes To Do In His Not-so-abundant Free Time)

## Plan

This starts as a DMG (original GameBoy) emulator and expand to CGB either when I feel like the core emulator is solid, or I start getting bored. The color hardware is mostly a direct extension of the Game Boy system and is backwards compatible with its software. I'll also branch into peripheral support, as the mood strikes.

## Usage

### Building

I've built this emulator on several versions of Linux and in Cygwin on Windows. The Cygwin builds will work on any Windows machine, as long as you distribute the appropriate dll files with it. However, the Cygwin version of OpenCV seems not to like talking to webcams under Windows, so I'm afraid the camera won't work (although it's almost trivial to fake it, but giving OpenCV a video file to open, instead of a camera). I haven't done a Mac build, although I expect it would work fine, as long as you provided the libraries (i.e. it would be a bit more of a manual process; I don't have a Mac to do the dev work on).

You'll need SDL2 development libraries. I've built the emulator on a few versions of SDL2; I'd expect it to just work.

If you want camera support, you'll need OpenCV development libraries. In slightly older versions of OpenCV (2.4.9, for example), it uses the core, highgui, and imgproc modules. In newer versions (probably starting around 3.x), you also need the videoio module. The necessary linking options change between versions of OpenCV, so the Makefile currently has the correct options for OpenCV 3.2. Since OpenCV and cameras are non-core functionalities and are a pain to set up, linking in camera support is disabled by default.

Typing `make` in the root directory of the project is enough to build the emulator. Adding `CAMERA=yes` and/or `BUILD=debug` will enable camera linking and debug mode, and the names imply.

So the version with the most options turned on would currently be built by `make CAMERA=yes BUILD=debug`.

### Keys

Keys are currently statically-mapped and the keyboard's the only input method.

Key | Function
--- | --------
Q | Quit
W | Up
A | Left
S | Down
D | Right
G | Start
H | Select
K | B Button
L | A Button
E | Graphics debug timing diagram
O | Dump VRAM to `vram_dump.dat`
T | Toggle CPU and PPU trace mode


### Cammand-line Options

Command: `khedgb [options] romfile`

Option | Effect
------ | ------
--sgb | Set Super Gameboy mode, if supported by the ROM
--cgb | Set Color Gameboy mode (not written yet, just planned)
--trace | Activate CPU+PPU instruction trace (listing of what the game is doing)
--headless | Headless mode (disable graphics) (I don't test this often)
--nosound | NoSound mode (disable audio) (Sound isn't working yet anyhow...)
--fw `fw_filename` | Boot using specified bootstrap firmware (expect 256 byte files for SGB and DMG, and a 2,304 byte file for CGB)

Note about the firmware: They aren't strictly necessary. The emulator runs without them just fine. They just complete the experience.

## Current state

Most games are booting and running (I'll claim 95% booting without issue, and 70% running stably during gameplay and without major visible bugs). Most original Game Boy games are playable. Saving and loading games (not save states) is basically working, including with save files from other emulators (tested with BGB, for example). Timing issues abound. Some games may do things that I haven't tested, and crash for that reason.

### CPU

The CPU seems stable, although there might be timing issues (like the 0xCBxx instruction timing that I just fixed). Blargg cpu_inst tests pass, and so do a lot of the ones related to timing.

### Memory Map

The Memory Map is fairly fleshed-out, but not complete, and some of the peripheral hardware isn't being emulated yet (sound is mostly absent, the timer is new code, serial is only partially implemented). Undocumented registers may or may not react as on a real system. I've implemented some of them, but not all.

### ROM mapping

ROMs load, and I added the ability to load them directly from zip files (it just finds the first file in the archive, and loads it if it's between 32KB and 8MB). Onboard ROM+RAM work, and I've got the mappers I consider important written (null, mbc1, mbc2, mbc3, mbc5, camera). Null covers the simpler 32KB games, MBC1+2 cover a lot of early ones, MBC3 covers most of them that need to keep time (like the Pokemon games), and MBC5 covers later GameBoy games, and especially almost all of the Color games. MBC3 doesn't correctly support the clock chip, or saving and loading the time in save games. The Camera mapper correctly boots the game and provides access to the camera (if you specify the option to support the camera).

### Interrupts

The link cable isn't fully emulated. There's a kludge for Blargg test serial line outputs and basic support for pretending to send data. The LCD interrupts work, with approximately-correct timing (real cycle-accurate emulation relies on details that I'm not taking into account, as rendering a line takes variable amounts of time). VBlank interrupts work. Joypad interrupts work.

### Display

Display output composites the video roughly correctly, although sprite priority isn't quite right. On real hardware, it's possible to modify certain display registers during rendering. I don't support sub-line rendering changes, right now. The emulator does a partial colorization when possible, kind of like how the Game Boy Color does with non-color games. Sprites are blue and green-tinted (since there are 2 internal sprite palettes), the background is monochrome, and the "window" overlay is red/pink. In Super Game Boy mode, most games provide a themed backdrop, and this emulator supports displaying that.

### Super GameBoy

There won't ever be full support; full support would mean implementing a full SNES emulator. The emulator supports drawing custom game borders, applying palette changes to the screen, screen blanking, and could be easily enhanced to support 2/4-player input. I'm leaving out SNES audio output support, various functions that adjust settings for the SGB itself, and anything that copies/run program code on the SNES (although I'll provide printed notification that those commands were received).

### GameBoy Camera

If you specify "CAMERA=yes" when compiling, and have OpenCV dev headers installed, the Camera mapper will try to capture images from the first V4L2 device, emulating the Mitsubishi M64282FP camera used in the Game Boy Camera. It supports the dithering/quantization registers (contrast) and the exposure registers (brightness), which is enough to support the options used by the Game Boy Camera software. There are options that the camera module supports, and that the mapper provides access to, but that the Camera never used. I'm not supporting those options, for the time being.

## Far Future

- The emulator is written using SDL2. Maybe there'll be a web-compiled version someday ;-)

- I think it would be cool to support an external cartridge reader, to support running from physical cartridges in realtime, without the necessity of copying the data anywhere.

### Purpose

The purpose of this emulator is not to enable or encourage copyright infringement of intellectual property, but to give me a platform to understand more about low-level software development and the design decisions that go into a piece of hardware like the GameBoy. I want to know what makes that relic of my childhood tick, the compromises that were chosen, the little hardware bugs and timing quirks that resulted, and so on. A pleasant side-effect is the ability to run games. But if playing games was the main purpose, I'd use someone else's more polished emulator, or the actual game hardware I've still got. So, that's my recommendation. If you want to follow the development of an emulator by someone making their second attempt, cool. If you just want to play games, there are myriad better options.
