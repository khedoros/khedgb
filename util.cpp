#include "util.h"
#include "memmap.h"
#include "rom.h"
#include<SDL2/SDL.h>
#include<minizip/unzip.h>

namespace util {
bool process_events(memmap * bus) {
    SDL_Event event;
    int newx,newy;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN:  /* Handle a KEYDOWN event */
                printf("util::Keydown\n");
                if(event.key.keysym.scancode==SDL_SCANCODE_Q||
                        (event.key.keysym.scancode==SDL_SCANCODE_C&&(event.key.keysym.mod==KMOD_RCTRL))||
                        (event.key.keysym.scancode==SDL_SCANCODE_C&&(event.key.keysym.mod==KMOD_LCTRL))) {
                    SDL_Quit();
                    return false;
                }
                else {
                    bus->keydown(event.key.keysym.scancode);
                }
                break;
            case SDL_KEYUP: /* Handle a KEYUP event*/
                printf("util::Keyup\n");
                bus->keyup(event.key.keysym.scancode);
                break;
            case SDL_WINDOWEVENT:
                printf("util::Window event: ");
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_SHOWN:
                        printf("shown\n");
                        break;
                    case SDL_WINDOWEVENT_HIDDEN:
                        printf("hidden\n");
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        printf("resized\n");
                        newx=event.window.data1;
                        newy=event.window.data2;
                        if(newx > 0 && newy > 0) {
                            //changed=true;
                        }
                        //ppui.resize(newx,newy);
                        break;
                    case SDL_WINDOWEVENT_EXPOSED:
                        printf("exposed\n");
                        //ppui.resize(1,1);
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
                        printf("closed\n");
                        SDL_Quit();
                        return false;
                        break;
                    default:
                        printf("something else (%d)\n", event.window.event);
                        break;
                }
                break;
            case SDL_QUIT:
                printf("util::SDL Quit triggered\n");
                //SDL_PauseAudio(true);
                SDL_Quit();
                return false;
                break;
            default: /* Report an unhandled event */
                printf("util::I don't know what this event is (%d)! Flushing it.\n", event.type);
                SDL_FlushEvent(event.type);
                break;
        }
    }
    return true;
}

int unzip(const std::string& zip_filename, std::vector<uint8_t>& output) {
    unzFile f = unzOpen(zip_filename.c_str());
    if(!f) {
        printf("Bleh2.\n");
        return 2;
    }

    unz_global_info gi;
    if(unzGetGlobalInfo(f,&gi) != UNZ_OK) {
        printf("Bleh3.\n");
        return 3;
    }

    printf("File seems to contain %ld file entries.\n", gi.number_entry);
    if(unzGoToFirstFile(f) != UNZ_OK) {
        printf("Bleh4.\n");
        return 4;
    }

    unz_file_info fi;
    char filename[256];
    if(unzGetCurrentFileInfo(f,&fi,filename,256,NULL,0,NULL,0) != UNZ_OK) {
        printf("Bleh5.\n");
        return 5;
    }

    printf("Filename: %s\nCompression method: %d\nCompressed size: %ld\nExtracted size: %ld\n", filename, fi.compression_method, fi.compressed_size, fi.uncompressed_size);

    if(unzOpenCurrentFile(f) != UNZ_OK) {
        printf("Bleh6.\n");
        return 6;
    }

    output.resize(fi.uncompressed_size);

    int read_bytes = 0;

    if((read_bytes = unzReadCurrentFile(f,reinterpret_cast<char *>(&output[0]),fi.uncompressed_size)) != fi.uncompressed_size) {
        printf("Bleh7. Got return of %d\n", read_bytes);
        return 7;
    }

    unzCloseCurrentFile(f);
    unzClose(f);

    return 0;
}

}
