#include "util.h"
#include "memmap.h"
#include "rom.h"
#include<SDL2/SDL.h>

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
                //cpui.print_details(filename);
                //printf("%d frames rendered in %f seconds. (%f FPS)\n",ppui.get_frame(),float(clock())/float(CLOCKS_PER_SEC),float(ppui.get_frame())/(float(clock())/float(CLOCKS_PER_SEC)));
                //return 0;
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
}
