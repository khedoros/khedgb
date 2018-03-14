#include<cstdio>
#include<cstdint>
#include<SDL2/SDL.h>

int main() {
    SDL_Init(SDL_INIT_EVERYTHING);
    /* Initialize the SDL library */
    SDL_Window * screen = SDL_CreateWindow("KhedGB",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            320, 288,
            SDL_WINDOW_RESIZABLE);

    if ( screen == NULL ) {
        fprintf(stderr, "lcd::Couldn't set 320x240x8 video mode: %s\nStarting without video output.\n",
                SDL_GetError());
        //exit(1);
        return 1;
    }

    SDL_SetWindowMinimumSize(screen, 320, 288);
    int cur_x_res=160;
    int cur_y_res=144;

    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED/*|SDL_RENDERER_PRESENTVSYNC*/);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    //renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE|SDL_RENDERER_PRESENTVSYNC);
    SDL_Renderer * renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
    if(!renderer) {
        fprintf(stderr, "lcd::Couldn't create a renderer: %s\nStarting without video output.\n",
                SDL_GetError());
        SDL_DestroyWindow(screen);
        screen = NULL;
        //exit(1);
        return 2;
    }

    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,160,144);
    if(!texture) {
        fprintf(stderr, "lcd::Couldn't create a texture: %s\nStarting without video output.\n",
                SDL_GetError());
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
        SDL_DestroyWindow(screen);
        screen = NULL;
        //exit(1);
        return 3;
    }

    SDL_Surface * buffer = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

    //low and high-priority sprites
    SDL_Surface * lps = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);
    SDL_Surface * hps = SDL_CreateRGBSurface(0,160,144,32,0,0,0,0);

    //2 background maps, repeated to make wrapping easier, used for window and background
    SDL_Surface * bg1 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);
    SDL_Surface * bg2 = SDL_CreateRGBSurface(0,512,512,32,0,0,0,0);

    printf("160x144 has pitch of %d\n", buffer->pitch);
    printf("512*512 has pitch of %d\n", bg1->pitch);
    //printf("lcd::Setting render draw color to black\n");
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 0);
    //printf("lcd::Clearing rendering output\n");
    SDL_RenderClear(renderer);
    //printf("lcd::Pushing video update to renderer\n");
    SDL_RenderPresent(renderer);
    //printf("lcd::constructor reached end\n");
    SDL_Delay(1000);

    return 0;
}

