# khedgb
Adventures in Game Boy emulation (Or, What Khedoros Likes To Do In His Not-so-abundant Free Time). Honestly, it's working better than I would have thought. A lot of these ideas would be good to carry over to my NES emulator to fix some longstanding bugs, while simultaneously improving compatibility and code quality.

## Plan

This started as a DMG (original GameBoy) emulator, has expanded to cover some accessories, and now support CGB (GameBoy Color) games. The remaining plan is to quash timing bugs that still cause problems in some games, to improve audio quality, and maybe add a GUI to make the program feel more comfortable.

## Usage

### Building

If you're hoping for decent, step-by-step instructions: Sorry, I haven't written them. The following is mostly a high-level description of the requirements to compile the program; I expect that it would be good enough for most developers to get the idea. Then again, I also expect that I'm the only one reading this; it's not like I've been advertising.

I've built this emulator on several versions of Linux and in Cygwin on Windows. The Cygwin builds will work on any Windows machine, as long as you distribute the appropriate dll files with it. However, the Cygwin version of OpenCV seems not to like talking to webcams under Windows, so I'm afraid the camera won't work (although it's almost trivial to fake it, but giving OpenCV a video file to open, instead of a camera). I haven't done a Mac build, although I expect it would work fine, as long as you provided the libraries (i.e. it would be a bit more of a manual process; I don't have a Mac to do the dev work on).

You'll need SDL2 development libraries. I've built the emulator on a few versions of SDL2; I'd expect it to just work.

If you want camera support, you'll need OpenCV development libraries. In slightly older versions of OpenCV (2.4.9, for example), it uses the core, highgui, and imgproc modules. In newer versions (probably starting around 3.x), you also need the videoio module. The necessary linking options change between versions of OpenCV, so the Makefile currently has the correct options for OpenCV 3.2. Since OpenCV and cameras are non-core functionalities and are a pain to set up, linking in camera support is disabled by default.

The printer support requires libpng, and libpng requires libz (again, the development packages for those libraries). It would be pretty easy to modify the emulator's code to not actually output images, but I haven't bothered, because libpng and libz are much more likely to already be installed than opencv is.

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
--cgb | Set Color Gameboy mode
--trace | Activate CPU+PPU instruction trace (listing of what the game is doing)
--headless | Headless mode (disable graphics) (I don't test this often)
--nosound | NoSound mode (disable audio) (Sound is working now, but I don't think this option is yet)
--fw `fw_filename` | Boot using specified bootstrap firmware (expect 256 byte files for SGB and DMG, and a 2,304 byte file for CGB)

Note about the firmware: They aren't strictly necessary. The emulator runs without them just fine. They just complete the experience.

## Current state

Most GameBoy and Color games are booting and running (all of the ones I test work nearly-perfectly). Saving and loading of games (not save states) is basically working, including with save files from other emulators (tested with BGB, for example). Timing issues cause some minor graphical issues in the games that I test (usually split-second flashes and such). Games that I don't test often are a little more "iffy" (and that applies doubly to color games, since I've only recently added any support). Some games may do things that I haven't tested, and crash for that reason (although when I find cases like that, I try to fix them by determining where the inaccuracy is in my emulator).

### CPU

The CPU seems stable, although there are still subtle timing issues (the kind that don't affect most games, but completely cripple others). Blargg cpu_inst tests pass, and so do a lot of the ones related to timing. Speed switching (for CGB) "works", but I'm not convinced yet that it's quite correct (although it seems reliable, and doesn't seem to be a point of difficulty for most games).

### Memory Map

The Memory Map is complete, but undocumented registers may or may not react as on a real system, and there might be cases where some things act like an original GameBoy and some act like a GameBoy Color.

### ROM mapping

The following mappers are supported: null, mbc1, mbc2, mbc3, mbc5, and the camera. Null covers the simpler 32KB games, MBC1+2 cover a lot of early ones, MBC3 covers most of them that need to keep time (like the Pokemon games), and MBC5 covers later GameBoy games, and especially almost all of the Color games. The Camera mapper correctly boots the game and provides access to the camera (if you specify the option to support the camera, when compiling). MBC3 doesn't correctly support the clock chip, or saving and loading the time in save games.

### Interrupts

All of the interrupts basically "work", but I'm guessing that the specifics are one of the last remaining sources of bugs (i.e. minor timing errors that cause games to malfunction).

### Display

Working; CGB and DMG use separate rendering functions. DMG has all the framework necessary to support custom palettes; I just need to wire them in. Currently, original Game Boy games use a kind of sepia-tone palette, but the previous semi-colorized one is still present in the source code.

### Super GameBoy

There won't ever be full support; full support would mean implementing a full SNES emulator. The emulator supports drawing custom game borders, applying palette changes to the screen, screen blanking, and could be easily enhanced to support 2/4-player input. I'm leaving out SNES audio output support, various functions that adjust settings for the SGB itself, and anything that copies/run program code on the SNES (although I'll provide printed notification that those commands were received).

### GameBoy Camera

If you specify "CAMERA=yes" when compiling, and have OpenCV dev headers installed, the Camera mapper will try to capture images from the first V4L2 device, emulating the Mitsubishi M64282FP camera used in the Game Boy Camera. It supports the dithering/quantization registers (contrast) and the exposure registers (brightness), which is enough to support the options used by the Game Boy Camera software. There are options that the camera module supports, and that the mapper provides access to, but that the Camera never used. I'm not supporting those options, for the time being.

### GameBoy Printer

Can't have the camera without the printer, right? When the printer receives the appropriate commands, it will output a PNG file that should contain the image that would come out of an actual GameBoy printer (it just numbers the file, but it tells you the number of the file that it output). I haven't added image decompression yet (so games that require it will behave unpredictably), but most games write their pictures as decompressed data anyhow.

## Far Future

- The emulator is written using SDL2. Maybe there'll be a web-compiled version someday ;-)

- I think it would be cool to support an external cartridge reader, to support running from physical cartridges in realtime, without the necessity of copying the data anywhere.

### Purpose

The purpose of this emulator is not to enable or encourage copyright infringement of intellectual property, but to give me a platform to understand more about low-level software development and the design decisions that go into a piece of hardware like the GameBoy. I want to know what makes that relic of my childhood tick, the compromises that were chosen, the little hardware bugs and timing quirks that resulted, and so on. A pleasant side-effect is the ability to run games. But if playing games was the main purpose, I'd use someone else's more polished emulator, or the actual game hardware I've still got. So, that's my recommendation. If you want to follow the development of an emulator by someone making their second attempt, cool. If you just want to play games, there are myriad better options.
