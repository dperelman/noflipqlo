#ifdef __cplusplus
    #include <cstdlib>
#else
    #include <stdlib.h>
#endif
#ifdef __APPLE__
    #include <SDL/SDL.h>
#else
    #include "SDL.h"
#endif
#include "SDL_ttf.h"
#include "SDL_syswm.h"
#include <sys/time.h>
#include <string.h>
#include <string>
#include <math.h>
#include <X11/Xlib.h>
#include <signal.h>
using namespace std;
int past_m=0;

bool twentyfourh = true;
bool fullscreen  = false;

SDL_Window *screenWindow;
SDL_Renderer *screenRenderer;
SDL_Surface *screenSurface;
SDL_PixelFormat *screenFormat;
SDL_Texture *screenTexture;


TTF_Font *FONT_TIME = NULL;
TTF_Font *FONT_MODE = NULL;
SDL_Color FONT_COLOR_WHITE = {176,176,176};

SDL_Rect hourBackground;
SDL_Rect minBackground;
int spacing = 0;

int customHeight = 0;
int customWidth = 0;
int screenWidth = 0;
int screenHeight = 0;
const int DEFAULT_WIDTH = 640;
const int DEFAULT_HEIGHT = 480;

const char* FONT_FILE_BOLD = "/usr/share/fonts/truetype/droid/DroidSans-Bold.ttf";
const char* FONT_FILE_FALLBACK = "/usr/share/fonts/truetype/ubuntu-font-family/Ubuntu-B.ttf";
const char* FONT_CUSTOM_FILE = "";

const Uint32 COLOR_FONT_RGB = 0xb7b7b7;
const Uint32 COLOR_BACKGROUND_RGB = 0x0a0a0a;

Uint32 COLOR_FONT;
Uint32 COLOR_BACKGROUND;


void checkTime(struct tm **time_i, Uint32 *ms_to_next_minute) {
    timeval tv;
    gettimeofday(&tv, NULL);
    *time_i = localtime(&tv.tv_sec);

    Uint32 seconds_to_next_minute = 60 - (*time_i)->tm_sec;
    *ms_to_next_minute = seconds_to_next_minute*1000 - tv.tv_usec/1000;
}

Uint32 checkEmit(Uint32 interval, void *param) {
    struct tm * time_i;
    Uint32 ms_to_next_minute;

    checkTime(&time_i, &ms_to_next_minute);

    if ( time_i->tm_min != past_m) {
        SDL_Event e;
        e.type = SDL_USEREVENT;
        e.user.code = 0;
        e.user.data1 = NULL;
        e.user.data2 = NULL;
        SDL_PushEvent(&e);
#ifdef DEBUG
        printf("Time is: %d : %d\n", time_i->tm_hour, time_i->tm_min);
#endif
        past_m = time_i->tm_min;
    }

    // Don't wake up until the next minute.
    interval = ms_to_next_minute;
    // Make sure interval is positive.
    // Should only matter for leap seconds.
    if ( interval <= 0 ) {
        interval = 500;
    }
    return (interval);
}
int initSDL() {
    if ( SDL_Init( SDL_INIT_VIDEO|SDL_INIT_TIMER ) < 0 ) {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return 2;
    }
    return 0;
}

Uint32 mapRGB24(const SDL_PixelFormat* format, Uint32 rgb) {
    return SDL_MapRGB(format,
                      rgb >> 16,
                      (rgb >> 8) & 0xff,
                      rgb & 0xff);
}

// From http://comments.gmane.org/gmane.comp.lib.sdl/67323
struct SDL_Window
{
        const void *magic;
        Uint32 id;
        char *title;
        SDL_Surface *icon;
        int x, y;
        int w, h;
        int min_w, min_h;
        int max_w, max_h;
        Uint32 flags;
};
typedef struct SDL_Window SDL_Window;

int initResources(bool fullscreen, int width, int height, Window wid) {
    try {
        if(wid != 0) {
#ifdef DEBUG
            printf("Before SDL_CreateWindowFrom().\n");
#endif
            screenWindow = SDL_CreateWindowFrom((void*)wid);
            screenWindow->flags |= SDL_WINDOW_OPENGL;
            SDL_GL_LoadLibrary(NULL);
#ifdef DEBUG
            printf("After SDL_CreateWindowFrom().\n");
#endif
        } else if(fullscreen) {
            screenWindow = SDL_CreateWindow("noflipqlo",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              0, 0,
                              SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else {
            screenWindow = SDL_CreateWindow("noflipqlo",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              width, height,
                              0);
        }
        SDL_GetWindowSize(screenWindow, &screenWidth, &screenHeight);
        screenRenderer = SDL_CreateRenderer(screenWindow, -1, 0);
        // From https://wiki.libsdl.org/MigrationGuide#If_your_game_wants_to_do_both
        screenSurface = SDL_CreateRGBSurface(0, screenWidth, screenHeight, 32,
                                             0x00FF0000,
                                             0x0000FF00,
                                             0x000000FF,
                                             0xFF000000);
        screenTexture = SDL_CreateTexture(screenRenderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          screenWidth, screenHeight);
        if (screenSurface == NULL) {
            printf("SDL_GetWindowSurface() failed: %s\n", SDL_GetError());
            throw 3;
        }
        //screenFormat = SDL_AllocFormat(SDL_GetWindowPixelFormat(screenWindow));
        screenFormat = screenSurface->format;
#ifdef DEBUG
        printf("Full Screen Resolution : %dx%d\n", screenWidth, screenHeight);
#endif
        if (fullscreen) {
            customHeight = screenHeight;
            customWidth = screenWidth;
        }
        if (strcmp("", FONT_CUSTOM_FILE) == 0) {
            FONT_TIME = TTF_OpenFont(FONT_FILE_BOLD, customHeight / 2);
            FONT_MODE = TTF_OpenFont(FONT_FILE_BOLD, customHeight/ 15);
        } else {
            FONT_TIME = TTF_OpenFont(FONT_CUSTOM_FILE, customHeight / 2);
            FONT_TIME = TTF_OpenFont(FONT_CUSTOM_FILE, customHeight / 15);
        }

        if (!screenSurface)
            throw 2;
        if (!FONT_TIME)
            throw 1;
    } catch (int param) {
        if (param == 1)
            printf("TTF_OpenFont: %s\n", TTF_GetError());
        else if (param == 2)
            printf("Couldn't initialize video mode to check native, fullscreen resolution.\n");

        return param;
    }

    // Get colors in proper format.
    
    COLOR_FONT = mapRGB24(screenFormat, COLOR_FONT_RGB);
    COLOR_BACKGROUND = mapRGB24(screenFormat, COLOR_BACKGROUND_RGB);

    /* CALCULATE BACKGROUND COORDINATES */
    hourBackground.y = 0.2 * customHeight;
    hourBackground.x = 0.5 * (customWidth - ((0.031)*customWidth) - (1.2 * customHeight));
    hourBackground.w = customHeight * 0.6;
    hourBackground.h = hourBackground.w;
#ifdef DEBUG
    printf(" Hour x coordinate %d\n Hour y coordinate %d\n", hourBackground.y, hourBackground.x);
#endif
    spacing = 0.031 * customWidth;
    minBackground.x = hourBackground.x + (0.6*customHeight) + spacing;
    minBackground.y = hourBackground.y;
    minBackground.h = hourBackground.h;
    minBackground.w = hourBackground.w;
#ifdef DEBUG
    printf (" Minute x coordinate %d\n Minute y coordinate %d\n", minBackground.x, minBackground.y);
#endif
    SDL_AddTimer(500, checkEmit, NULL);
    return 0;
}

void set_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {
    Uint8 *target_pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
    *(Uint32 *)target_pixel = pixel;

}

void fill_circle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 pixel) {
    static const int BPP = 4;
    double r = (double)radius;
    for (double dy = 1; dy <= r; dy += 1.0) {
        double dx = floor(sqrt((2.0 * r * dy) - (dy * dy)));
        int x = cx - dx;
        Uint8 *target_pixel_a = (Uint8 *)surface->pixels + ((int)(cy + r - dy)) * surface->pitch + x * BPP;
        Uint8 *target_pixel_b = (Uint8 *)surface->pixels + ((int)(cy - r + dy)) * surface->pitch + x * BPP;

        for (; x <= cx + dx; x++) {
            *(Uint32 *)target_pixel_a = pixel;
            *(Uint32 *)target_pixel_b = pixel;
            target_pixel_a += BPP;
            target_pixel_b += BPP;
        }
    }
}

void drawRoundedBackground(SDL_Surface * surface, SDL_Rect * coordinates) {
    int backgroundSize = customHeight * 0.6;
    int radius = 10;
#ifdef DEBUG
    printf("Background size %d\n", backgroundSize);
#endif
    for (int i=0; i<backgroundSize-radius; i++) {
        for (int j=0; j<backgroundSize-radius; j++) {
            fill_circle(surface, coordinates->x + j, coordinates->y + i, radius, COLOR_BACKGROUND);
        }
    }
}

SDL_Rect getCoordinates(SDL_Rect * background, SDL_Surface * foreground) {
    SDL_Rect cord;
    int dx = (background->w - foreground->w) * 0.5;
    cord.x = background->x + dx;
    int dy = (background->h - foreground->h)  * 0.5;
    cord.y = background->y + dy;
#ifdef DEBUG
    printf("dx = %d : dy = %d\n", dx, dy);
    printf("Text Coordinates x %d : y %d\n", cord.x, cord.y);
#endif
    return cord;
}

void drawAMPM(SDL_Surface * surface, tm * _time) {
    SDL_Rect cords;
    cords.x = (hourBackground.h * 0.024) + hourBackground.x;
    cords.y = (hourBackground.h * 0.071) + hourBackground.y;
#ifdef DEBUG
    printf("AM/PM position\n\tx %d\n\ty %d\n", cords.x, cords.y);
#endif
    char mode[3];
    strftime(mode, 3, "%p", _time);
    SDL_Surface *AMPM = TTF_RenderText_Blended(FONT_MODE, (const char *)mode, FONT_COLOR_WHITE);
    SDL_BlitSurface(AMPM, 0, surface, &cords);
}

void drawTime(SDL_Surface *surface, tm * _time) {
    try {
        char hour[3];
        if (twentyfourh)
            strftime(hour, 3, "%H", _time);
        else
            strftime(hour, 3, "%I", _time);
        char minutes[3];
        strftime(minutes, 3, "%M", _time);
        int h = atoi(hour);
#ifdef DEBUG
        printf("Current time is %s : %s\n stripped hour ? %d\n", hour, minutes,h);
#endif
        SDL_Rect coordinates;
        SDL_Surface *text = TTF_RenderText_Blended(FONT_TIME, hour, FONT_COLOR_WHITE);
        coordinates = getCoordinates(&hourBackground, text);
        SDL_BlitSurface(text, 0, screenSurface, &coordinates);
        text = TTF_RenderText_Blended(FONT_TIME, (const char *) minutes, FONT_COLOR_WHITE);
        coordinates = getCoordinates(&minBackground, text);
        SDL_BlitSurface(text, 0, screenSurface, &coordinates);
    } catch (...) {
        printf ("Problem drawing time");
    }
}

void drawAll() {
    SDL_FillRect(screenSurface, 0, SDL_MapRGB(screenFormat, 0, 0, 0));
    time_t rawTime;
    struct tm * _time;
    time(&rawTime);
    _time = localtime(&rawTime);
    drawRoundedBackground(screenSurface, &hourBackground);
    drawRoundedBackground(screenSurface, &minBackground);
    drawTime(screenSurface, _time);
    if (!twentyfourh)
        drawAMPM(screenSurface, _time);

    SDL_UpdateTexture(screenTexture, NULL, screenSurface->pixels, screenSurface->pitch);
    SDL_RenderClear(screenRenderer);
    SDL_RenderCopy(screenRenderer, screenTexture, NULL, NULL);
    SDL_RenderPresent(screenRenderer);
}

void exitImmediately(int sig) {
    // Can't use exit() because atexit calls SDL_Quit()
    //  which hangs trying to free the window.
    abort();
}

int main (int argc, char** argv ) {
    signal(SIGINT, exitImmediately);
    signal(SIGTERM, exitImmediately);

    char *wid_env;
    static char sdlwid[100];
    Uint32 wid = 0;
    Display *display;
    XWindowAttributes windowAttributes;
    windowAttributes.height = 0;
    windowAttributes.width = 0;

    /* If no window argument, check environment */
    if (wid == 0) {
        if ((wid_env = getenv("XSCREENSAVER_WINDOW")) != NULL ) {
            wid = strtol(wid_env, (char **) NULL, 0); /* Base 0 autodetects hex/dec */
        }
    }

    /* Get win attrs if we've been given a window, otherwise we'll use our own */
    if (wid != 0 ) {
        if ((display = XOpenDisplay(NULL)) != NULL) { /* Use the default display */
            XGetWindowAttributes(display, (Window) wid, &windowAttributes);
            XCloseDisplay(display);
            snprintf(sdlwid, 100, "SDL_WINDOWID=0x%X", wid);
            putenv(sdlwid); /* Tell SDL to use this window */
        }
    }

    for(int i=1; i<argc; i++) {
        if(strcmp("--help",argv[i])==0 || strcmp("-h", argv[i]) == 0) {
            printf("Usage: [OPTION...]\nOptions:\n");
            printf(" --help\t\t\t\tDisplay this\n");
            printf(" -root,--fullscreen,--root\tFullscreen\n");
            printf(" -ampm, --ampm\t\t\tTurn off 24 h system and use 12 h system instead\n");
            printf(" -w\t\t\t\tCustom Width\n");
            printf(" -h\t\t\t\tCustom Height\n");
            printf(" -r, --resolution\t\tCustom resolution in a format [Width]x[Height]\n");
            printf(" -f, --font\t\t\tPath to custom file font. Has to be Truetype font.");
            return 0;
        } else if (strcmp("-root",argv[i]) == 0 || strcmp("--root", argv[i]) == 0 || strcmp("--fullscreen", argv[i]) == 0) {
            fullscreen=true;
        } else if (strcmp("-ampm",argv[i]) == 0 || strcmp("--ampm", argv[i]) == 0) {
            twentyfourh=false;
        } else if (strcmp("-r", argv[i]) == 0 || strcmp("--resolution", argv[i]) == 0) {
            char* resolution;
            resolution = argv[i+1];
            char * value;
            value = strtok(resolution,"x");
            int i = atoi(value);
#ifdef DEBUG
            printf("Value : %d\n", i );
#endif
            value = strtok(NULL, "x");
            i = atoi(value);
#ifdef DEBUG
            printf("Value : %d\n", i );
#endif
        } else if (strcmp("-w", argv[i]) == 0) {
            customWidth = atoi(argv[i+1]);
#ifdef DEBUG
            printf ("Width : %d\n", customWidth);
#endif
        } else if (strcmp("-h", argv[i]) == 0) {
            customHeight = atoi(argv[i+1]);
#ifdef DEBUG
            printf ("Height: %d\n", customHeight);
#endif
        } else if (strcmp("-f", argv[i]) == 0 || strcmp("--font", argv[i]) == 0) {
            FONT_CUSTOM_FILE = argv[i+1];
#ifdef DEBUG
            printf("Custom font:%s", FONT_CUSTOM_FILE);
#endif
        } else {
            printf("Invalid option -- %s\n", argv[i]);
            printf("Try --help for more information.\n");
            return 0;
        }
    }
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    if (customHeight > 0) {
        height = customHeight;
        width = customWidth;
    } else {
        customHeight = DEFAULT_HEIGHT;
        customWidth = DEFAULT_WIDTH;
    }

    initSDL();
    TTF_Init();
    int err = initResources(fullscreen, width, height, (Window)wid);
    if(err != 0) {
        return err;
    }
    SDL_ShowCursor(SDL_DISABLE);

    atexit(SDL_Quit);
    atexit(TTF_Quit);


    drawAll();
    bool done = false;
    while (!done) {
        SDL_Event event;
        
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_USEREVENT:
                drawAll();
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    done = true;
                break;
            case SDL_QUIT:
                done = true;
                break;
            }
        }
        
        struct tm * time_i;
        Uint32 ms_to_next_minute;

        checkTime(&time_i, &ms_to_next_minute);
        if(ms_to_next_minute > 100) {
            // Wait a bit less to make sure event fires at the right time.
            SDL_Delay(ms_to_next_minute - 100);
        } else {
            SDL_Delay(30);
        }
    }
    SDL_FreeFormat(screenFormat);
    SDL_FreeSurface(screenSurface);
    TTF_CloseFont(FONT_TIME);
    TTF_CloseFont(FONT_MODE);
#ifdef DEBUG
    printf("Exited cleanly\n");
#endif
    return 0;
}
