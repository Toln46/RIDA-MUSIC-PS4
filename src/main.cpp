
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include "graphics.h"
#include <sstream>

std::stringstream debugLogStream;
#define W 1920
#define H 1080
#define VIDEO_MEMORY_SIZE 0x02000000
#define STICK_LOW 78
#define STICK_HIGH 178
#define NAV_REPEAT_DELAY 9

static const uint8_t font[37][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
 {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},
 {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
 {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
 {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
 {0,0,0,0,0,0,0}
};

static int glyph(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    return 36;
}

static void text(Scene2D* s, int x, int y,
                 const char* str, int scale, Color c) {
    for (int n = 0; str[n]; ++n) {
        int g = glyph(str[n]);

        for (int yy = 0; yy < 7; ++yy) {
            for (int xx = 0; xx < 5; ++xx) {
                if (font[g][yy] & (1 << (4 - xx))) {
                    s->DrawRectangle(
                        x + n * 6 * scale + xx * scale,
                        y + yy * scale,
                        scale, scale, c
                    );
                }
            }
        }
    }
}

static void rect(Scene2D* s, int x, int y, int w, int h,
                 int r, int g, int b) {
    if (!s || w <= 0 || h <= 0) return;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= W || y >= H) return;
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;

    Color c = {
        (uint8_t)r,
        (uint8_t)g,
        (uint8_t)b
    };

    s->DrawRectangle(x, y, w, h, c);
}

static void label(Scene2D* s, int x, int y, const char* t,
                  int scale, int r, int g, int b) {
    Color c = {
        (uint8_t)r,
        (uint8_t)g,
        (uint8_t)b
    };
    text(s, x, y, t, scale, c);
}

static void draw(Scene2D* s, int tab, int selected, bool queued) {
    rect(s, 0, 0, W, H, 12, 15, 14);
    rect(s, 0, 0, 330, H, 17, 22, 20);

    rect(s, 44, 48, 54, 54, 29, 185, 84);
    rect(s, 59, 61, 24, 28, 12, 15, 14);

    label(s, 116, 60, "RIDA MUSIC", 4, 245, 248, 246);
    label(s, 48, 160, "YOUR LIBRARY", 2, 125, 143, 132);

    const char* nav[4] = {
        "HOME", "SEARCH", "LIBRARY", "FAVORITES"
    };

    for (int i = 0; i < 4; i++) {
        if (tab == i) {
            rect(s, 24, 205 + i * 76, 282, 58, 29, 65, 43);
        }

        label(s, 54, 224 + i * 76, nav[i], 3,
              tab == i ? 70 : 220,
              tab == i ? 220 : 226,
              tab == i ? 125 : 222);
    }

    rect(s, 24, 590, 282, 2, 50, 60, 54);
    label(s, 48, 630, "PLAYLISTS", 2, 125, 143, 132);
    label(s, 48, 676, "YOUR MIX", 2, 225, 230, 226);
    label(s, 48, 722, "RECENTLY PLAYED", 2, 225, 230, 226);
    label(s, 48, 1000, "GOLDHEN EDITION", 2, 105, 125, 113);

    label(s, 390, 66, "GOOD EVENING", 2, 145, 164, 151);
    label(s, 390, 112, tab == 0 ? "DISCOVER MUSIC" : nav[tab],
          5, 245, 248, 246);

    rect(s, 390, 205, 1460, 310, 24, 55, 39);
    rect(s, 430, 245, 230, 230, 29, 185, 84);
    rect(s, 486, 301, 118, 118, 12, 15, 14);
    rect(s, 527, 342, 36, 36, 29, 185, 84);

    label(s, 710, 258, "YOUR SOUND", 2, 190, 224, 199);
    label(s, 710, 315, "MUSIC, YOUR WAY", 5, 250, 252, 250);
    label(s, 710, 390, "A NEW HOME FOR YOUR MUSIC", 2, 210, 222, 214);

    rect(s, 710, 438, 220, 48, 29, 185, 84);
    label(s, 742, 455, "EXPLORE", 2, 12, 15, 14);

    label(s, 390, 570, "QUICK PICKS", 3, 245, 248, 246);

    const char* titles[4] = {
        "MIDNIGHT DRIVE", "NEON LIGHTS",
        "AFTER HOURS", "GREEN SIGNAL"
    };

    const char* artists[4] = {
        "RIDA RADIO", "SYNTH WAVE",
        "NIGHT SESSION", "FUTURE BEATS"
    };

    for (int i = 0; i < 4; i++) {
        int x = 390 + i * 365;

        rect(s, x, 630, 330, 260, 25, 31, 28);

        rect(s, x + 22, 652, 286, 145,
             i == selected ? 35 : 30,
             i == selected ? 105 : 48,
             i == selected ? 62 : 38);

        rect(s, x + 115, 687, 100, 75, 12, 15, 14);
        rect(s, x + 145, 710, 40, 30, 29, 185, 84);

        label(s, x + 24, 820, titles[i], 2, 245, 248, 246);
        label(s, x + 24, 855, artists[i], 2, 145, 164, 151);

        if (i == selected) {
            rect(s, x + 22, 652, 286, 5, 29, 185, 84);
        }
    }

    rect(s, 390, 930, 1460, 2, 48, 58, 52);

    label(s, 390, 965,
          queued
          ? "TRACK QUEUED - STREAMING NOT CONNECTED"
          : "PREVIEW MODE - AUDIO STREAMING NOT CONNECTED",
          2, 160, 180, 168);

    label(s, 390, 1015,
          "DPAD / STICK NAVIGATE  X SELECT  O BACK  L1/R1 SECTION",
          2, 115, 135, 123);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    Scene2D* scene = new Scene2D(W, H, 4);

    if (!scene->Init(VIDEO_MEMORY_SIZE, 2)) {
        printf("RIDA MUSIC: VIDEO INIT FAILED\n");

        for (;;) {
            sceKernelUsleep(1000000);
        }
    }

    scene->SetActiveFrameBuffer(0);

    int pad = -1;
    int user = -1;

    const int padInitResult = scePadInit();

    // Même séquence d'initialisation que BLACKOUT, qui fonctionne sur PS4.
    // Le service utilisateur reçoit ses paramètres explicites.
    OrbisUserServiceInitializeParams userParams = {
        ORBIS_KERNEL_PRIO_FIFO_LOWEST
    };
    const int userInitResult = sceUserServiceInitialize(&userParams);

    // Toujours demander l'utilisateur actif, même si le service signale
    // qu'il était déjà initialisé.
    const int userResult = sceUserServiceGetInitialUser(&user);

    if (padInitResult == 0 && userResult == 0 && user >= 0) {
        pad = scePadOpen(user, 0, 0, NULL);

        // Récupérer le handle existant si scePadOpen indique qu'il est déjà ouvert.
        if (pad < 0) {
            pad = scePadGetHandle(
                user,
                ORBIS_PAD_PORT_TYPE_STANDARD,
                0
            );
        }
    }

    printf("RIDA MUSIC: PAD INIT=%d USER INIT=%d USER RESULT=%d USER=%d PAD=%d\n",
           padInitResult, userInitResult, userResult, user, pad);

    if (pad < 0) {
        printf("RIDA MUSIC: CONTROLLER NOT FOUND\n");
    } else {
        OrbisPadColor light = {29, 185, 84, 0};
        scePadSetLightBar(pad, &light);
    }

    OrbisPadData state;
    memset(&state, 0, sizeof(state));

    uint32_t previousButtons = 0;

    int tab = 0;
    int selected = 0;
    int frame = 0;
    int navigationCooldown = 0;

    bool queued = false;

    for (;;) {
        uint32_t buttons = 0;

        memset(&state, 0, sizeof(state));

        if (pad >= 0) {
            if (scePadReadState(pad, &state) == 0) {
                buttons = state.buttons;
            }
        }

        uint32_t pressed = buttons & ~previousButtons;
        previousButtons = buttons;

        bool up = (buttons & ORBIS_PAD_BUTTON_UP) != 0;
        bool down = (buttons & ORBIS_PAD_BUTTON_DOWN) != 0;
        bool left = (buttons & ORBIS_PAD_BUTTON_LEFT) != 0;
        bool right = (buttons & ORBIS_PAD_BUTTON_RIGHT) != 0;

        if (pad >= 0) {
            if (state.leftStick.y < STICK_LOW) up = true;
            if (state.leftStick.y > STICK_HIGH) down = true;
            if (state.leftStick.x < STICK_LOW) left = true;
            if (state.leftStick.x > STICK_HIGH) right = true;
        }

        bool navigationInput = up || down || left || right;

        if (navigationCooldown > 0) {
            navigationCooldown--;
        }

        if (navigationInput && navigationCooldown == 0) {
            if (up) {
                selected = (selected + 3) % 4;
            } else if (down) {
                selected = (selected + 1) % 4;
            } else if (left) {
                tab = (tab + 3) % 4;
            } else if (right) {
                tab = (tab + 1) % 4;
            }

            navigationCooldown = NAV_REPEAT_DELAY;
        }

        if (pressed & ORBIS_PAD_BUTTON_L1) {
            tab = (tab + 3) % 4;
        }

        if (pressed & ORBIS_PAD_BUTTON_R1) {
            tab = (tab + 1) % 4;
        }

        if (pressed & ORBIS_PAD_BUTTON_CROSS) {
            queued = !queued;
        }

        if (pressed & ORBIS_PAD_BUTTON_CIRCLE) {
            queued = false;
        }

        if (pressed & ORBIS_PAD_BUTTON_OPTIONS) {
            tab = 0;
            selected = 0;
            queued = false;
        }

        draw(scene, tab, selected, queued);

        scene->SubmitFlip(frame);
        scene->FrameWait(frame);
        scene->FrameBufferSwap();

        frame++;

        sceKernelUsleep(16000);
    }

    return 0;
}
