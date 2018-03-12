#include "cpu.h"
#include "memmap.h"
#include "rom.h"
#include "util.h"
#include<SDL2/SDL.h>

#ifndef __CYGWIN__
#include<minizip/unzip.h>
#endif
#include<iostream>
#include<fstream>

namespace util {
#ifndef PPU_ONLY
bool process_events(cpu * proc, memmap * bus) {
    SDL_Event event;
    unsigned int newx,newy;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN:  /* Handle a KEYDOWN event */
                //printf("util::Keydown\n");
                if(event.key.keysym.scancode==SDL_SCANCODE_Q||
                        (event.key.keysym.scancode==SDL_SCANCODE_C&&(event.key.keysym.mod==KMOD_RCTRL))||
                        (event.key.keysym.scancode==SDL_SCANCODE_C&&(event.key.keysym.mod==KMOD_LCTRL))) {
                    SDL_Quit();
                    return false;
                }
                else if(event.key.keysym.scancode==SDL_SCANCODE_T) {
                    proc->toggle_trace();
                }
                else {
                    bus->keydown(event.key.keysym.scancode);
                }
                break;
            case SDL_KEYUP: /* Handle a KEYUP event*/
                //printf("util::Keyup\n");
                bus->keyup(event.key.keysym.scancode);
                break;
            case SDL_WINDOWEVENT:
                //printf("util::Window event: ");
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_SHOWN:
                        //printf("shown\n");
                        break;
                    case SDL_WINDOWEVENT_HIDDEN:
                        //printf("hidden\n");
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        //printf("resized\n");
                        newx=event.window.data1;
                        newy=event.window.data2;
                        if(newx > 0 && newy > 0) {
                            //changed=true;
                        }
                        //ppui.resize(newx,newy);
                        bus->win_resize(newx, newy);
                        break;
                    case SDL_WINDOWEVENT_EXPOSED:
                        //printf("exposed\n");
                        //ppui.resize(1,1);
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
                        //printf("closed\n");
                        SDL_Quit();
                        return false;
                        break;
                    default:
                        //printf("something else (%d)\n", event.window.event);
                        break;
                }
                break;
            case SDL_QUIT:
                //printf("util::SDL Quit triggered\n");
                //SDL_PauseAudio(true);
                SDL_Quit();
                return false;
                break;
            case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL:
                //printf("Mouse event\n");
                break;
            case SDL_TEXTINPUT:
                //printf("util::SDL TextInput\n");
                break;
            case SDL_KEYMAPCHANGED:
                //printf("util::SDL KeymapChanged triggered\n");
                break;
            case SDL_AUDIODEVICEADDED:
                //printf("util::SDL Audio Device Added\n");
                break;
            default: /* Report an unhandled event */
                //printf("util::I don't know what this event is (%d)! Flushing it.\n", event.type);
                SDL_FlushEvent(event.type);
                break;
        }
    }
    return true;
}
#endif

#ifndef __CYGWIN__
int unzip(const std::string& zip_filename, Vect<uint8_t>& output, size_t min_size, size_t max_size) {
    unzFile f = unzOpen(zip_filename.c_str());
    if(!f) {
        //printf("Bleh2.\n");
        return 2;
    }

    unz_global_info gi;
    if(unzGetGlobalInfo(f,&gi) != UNZ_OK) {
        //printf("Bleh3.\n");
        return 3;
    }

    //printf("File seems to contain %ld file entries.\n", gi.number_entry);
    if(unzGoToFirstFile(f) != UNZ_OK) {
        //printf("Bleh4.\n");
        return 4;
    }

    unz_file_info fi;
    char filename[256];
    if(unzGetCurrentFileInfo(f,&fi,filename,256,NULL,0,NULL,0) != UNZ_OK) {
        //printf("Bleh5.\n");
        return 5;
    }

    //printf("Filename: %s\nCompression method: %ld\nCompressed size: %ld\nExtracted size: %ld\n", filename, fi.compression_method, fi.compressed_size, fi.uncompressed_size);

    if(fi.uncompressed_size < min_size || fi.uncompressed_size > max_size) {
        unzClose(f);
        return 8;
    }

    if(unzOpenCurrentFile(f) != UNZ_OK) {
        //printf("Bleh6.\n");
        return 6;
    }

    output.resize(fi.uncompressed_size);

    size_t read_bytes = 0;

    if((read_bytes = unzReadCurrentFile(f,reinterpret_cast<char *>(&output[0]),fi.uncompressed_size)) != fi.uncompressed_size) {
        //printf("Bleh7. Got return of %ld\n", read_bytes);
        return 7;
    }

    unzCloseCurrentFile(f);
    unzClose(f);

    return 0;
}
#endif

int read(const std::string& filename, Vect<uint8_t>& output, size_t min_size, size_t max_size) {
    std::ifstream in(filename.c_str());
    if(in.is_open()) {
        size_t size = 0;
        in.seekg(0, std::ios::end);
        size = in.tellg();
        if(size < min_size || size > max_size) {
            in.close();
            return 1;
        }
        in.seekg(0, std::ios::beg);
        //printf("Opened %s, found a file of %ld bytes.\n", filename.c_str(), size);
        output.resize(size);
        in.read(reinterpret_cast<char *>(&(output[0])), size);
        in.close();
    }
    else {
        fprintf(stderr, "Couldn't open %s.\n", filename.c_str());
        return 2;
    }
    return 0;
}

bool reinit_sdl_screen(SDL_Window ** screen, SDL_Renderer ** renderer, SDL_Texture ** texture, unsigned int xres, unsigned int yres) {
    if(*screen) {
        SDL_DestroyWindow(*screen);
        *screen = NULL;
    }
    if(*renderer) {
        SDL_DestroyRenderer(*renderer);
        *renderer = NULL;
    }
    if(*texture) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }

    /* Initialize the SDL library */
    *screen = SDL_CreateWindow("KhedGB",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               xres*2, yres*2,
                               SDL_WINDOW_RESIZABLE);
    if ( *screen == NULL ) {
        fprintf(stderr, "lcd::Couldn't set %dx%dx32 video mode: %s\nStarting without video output.\n", xres*2, yres*2, SDL_GetError());
        return false;
    }

    SDL_SetWindowMinimumSize(*screen, xres, yres);

    //*renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED/*|SDL_RENDERER_PRESENTVSYNC*/);
    //*renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    //*renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE|SDL_RENDERER_PRESENTVSYNC);
    *renderer = SDL_CreateRenderer(*screen, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
    SDL_SetRenderDrawBlendMode(*renderer,SDL_BLENDMODE_BLEND);
    if(!(*renderer)) {
        fprintf(stderr, "lcd::Couldn't create a renderer: %s\nStarting without video output.\n", SDL_GetError());
        SDL_DestroyWindow(*screen);
        *screen = NULL;
        return false;
    }

    *texture = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,xres,yres);
    if(!(*texture)) {
        fprintf(stderr, "lcd::Couldn't create a texture: %s\nStarting without video output.\n", SDL_GetError());
        SDL_DestroyRenderer(*renderer);
        *renderer = NULL;
        SDL_DestroyWindow(*screen);
        *screen = NULL;
        return false;
    }
    return true;
}

void destroy_sdl_screen() {

}
int clamp(int low, int val, int high) {
    if(val < low) val = low;
    if(val > high) val = high;
    return val;
}
}

