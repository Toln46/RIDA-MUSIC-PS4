
#include <stdio.h>
#include <cstdio>
#include <string.h>
#include <stdint.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include "graphics.h"
#include <sstream>
#include <wchar.h>
#include "audius.h"
#include <orbis/ImeDialog.h>

std::stringstream debugLogStream;

static void write_status(const char* message) {
    FILE* log = std::fopen("/data/rida_music_diag.txt", "w");
    if (log) { std::fprintf(log, "%s\\n", message); std::fclose(log); }
}
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

static void rounded(Scene2D* s, int x, int y, int w, int h,
                        int radius, int r, int g, int b) {
    if (radius < 1) { rect(s,x,y,w,h,r,g,b); return; }
    for (int row=0; row<radius; ++row) {
        int inset = radius-row;
        rect(s,x+inset,y+row,w-2*inset,1,r,g,b);
        rect(s,x+inset,y+h-1-row,w-2*inset,1,r,g,b);
    }
    rect(s,x,y+radius,w,h-2*radius,r,g,b);
}

static AudiusTrack audiusTracks[8];
static int audiusCount = 0;
static int audiusSelected = 0;
static int audiusState = 0; // 0 idle, 1 results, 2 selected, -1 error
static char audiusQuery[128] = "";

static void draw(Scene2D* s, int tab, int selected, int notice) {
    const int whiteR=244, whiteG=247, whiteB=245;
    rect(s,0,0,W,H,12,15,14);
    rect(s,0,0,286,H,18,23,21);
    rect(s,286,0,2,H,37,46,42);

    rounded(s,36,38,48,48,12,29,185,84);
    rounded(s,49,49,22,26,7,12,15,14);
    label(s,100,47,"RIDA MUSIC",3,whiteR,whiteG,whiteB);
    label(s,38,128,"YOUR SPACE",2,120,143,132);

    const char* nav[4]={"HOME","SEARCH","LIBRARY","FAVORITES"};
    for(int i=0;i<4;i++) {
        int y=168+i*66;
        if(tab==i) {
            rounded(s,20,y,246,52,12,34,69,51);
            rounded(s,20,y+10,4,32,2,29,185,84);
        } else if(selected==i) {
            rounded(s,20,y,246,52,12,31,40,36);
        }
        label(s,48,y+17,nav[i],2,
              tab==i?92:(selected==i?235:218),
              tab==i?229:(selected==i?239:225),
              tab==i?153:(selected==i?236:220));
    }

    rect(s,36,462,214,1,52,63,57);
    label(s,38,492,"PLAYLISTS",2,125,145,133);
    label(s,38,535,"YOUR MIX",2,177,190,181);
    label(s,38,580,"RECENTLY PLAYED",2,177,190,181);
    label(s,38,1012,"GOLDHEN EDITION",1,105,125,113);

    label(s,338,45,"RIDA MUSIC  /  PS4",1,125,145,133);
    const char* headings[4]={"WELCOME BACK","SEARCH MUSIC","YOUR LIBRARY","YOUR FAVORITES"};
    const char* subtitles[4]={
        "A CLEANER HOME FOR YOUR MUSIC",
        "FIND ARTISTS, ALBUMS AND TRACKS",
        "YOUR SAVED MUSIC IN ONE PLACE",
        "THE TRACKS YOU WANT TO KEEP"
    };
    label(s,338,88,headings[tab],4,whiteR,whiteG,whiteB);
    label(s,340,139,subtitles[tab],2,145,164,151);

    if(tab==0) {
        rounded(s,338,196,1538,292,24,27,62,45);
        rounded(s,378,232,8,220,4,29,185,84);
        label(s,420,238,"YOUR MUSIC",2,176,224,194);
        label(s,420,286,"LISTEN YOUR WAY",5,250,252,250);
        label(s,422,356,"DISCOVER A WORLD OF MUSIC",2,215,229,220);
        label(s,422,399,"SEARCH, SAVE AND ENJOY YOUR COLLECTION",2,185,207,194);
        rounded(s,420,435,218,34,15,29,185,84);
        label(s,446,446,"EXPLORE LIBRARY",1,12,18,15);

        label(s,338,538,"GETTING STARTED",2,whiteR,whiteG,whiteB);
        rounded(s,338,584,742,224,18,29,36,33);
        rounded(s,366,612,46,46,13,29,185,84);
        label(s,382,627,"1",2,12,18,15);
        label(s,430,616,"CONNECT A MUSIC SOURCE",2,whiteR,whiteG,whiteB);
        label(s,430,655,"Spotify catalogue access is not configured.",2,164,181,170);
        label(s,430,687,"A developer account with Premium is required.",2,164,181,170);
        rounded(s,1110,584,766,224,18,29,36,33);
        rounded(s,1138,612,46,46,13,48,91,72);
        label(s,1154,627,"2",2,whiteR,whiteG,whiteB);
        label(s,1202,616,"AUDIO PLAYER",2,whiteR,whiteG,whiteB);
        label(s,1202,655,"The streaming engine will be connected",2,164,181,170);
        label(s,1202,687,"to a provider that permits PS4 playback.",2,164,181,170);
    } else if(tab==1) {
        rounded(s,338,196,1538,88,18,29,36,33);
        rounded(s,362,215,48,48,14,45,57,51);
        label(s,378,230,"Q",2,220,229,223);
        label(s,430,216,"AUDIUS MUSIC CATALOGUE",2,whiteR,whiteG,whiteB);
        label(s,430,246,"TRIANGLE: SEARCH   UP / DOWN: SELECT TRACK   CROSS: SELECT",1,157,176,164);
        if (audiusQuery[0]) {
            char queryText[128];
            snprintf(queryText,sizeof(queryText),"QUERY: %s",audiusQuery);
            label(s,1450,231,queryText,1,115,206,151);
        }
        if (audiusCount == 0) {
            rounded(s,338,316,1538,270,18,29,36,33);
            label(s,382,360,"SEARCH THE OPEN MUSIC CATALOGUE",3,whiteR,whiteG,whiteB);
            label(s,382,422,"Press TRIANGLE to enter an artist, title or genre.",2,174,190,180);
            label(s,382,464,"Audius public search uses the official HTTPS API.",2,174,190,180);
            label(s,382,526,audiusState<0 ? audius_last_error() : "No search has been performed yet.",1,115,206,151);
        } else {
            label(s,338,320,"LIVE SEARCH RESULTS",2,whiteR,whiteG,whiteB);
            for (int i=0; i<audiusCount && i<6; ++i) {
                int y=366+i*82;
                if (i==audiusSelected) rounded(s,338,y,1538,68,13,39,79,58);
                else rounded(s,338,y,1538,68,13,25,31,29);
                char titleLine[128];
                char artistLine[96];
                snprintf(titleLine,sizeof(titleLine),"%d. %s",i+1,audiusTracks[i].title);
                snprintf(artistLine,sizeof(artistLine),"%s",audiusTracks[i].artist[0]?audiusTracks[i].artist:"UNKNOWN ARTIST");
                label(s,372,y+12,titleLine,2,whiteR,whiteG,whiteB);
                label(s,372,y+42,artistLine,1,157,176,164);
                if (audiusTracks[i].duration>0) {
                    char duration[24];
                    snprintf(duration,sizeof(duration),"%d:%02d",audiusTracks[i].duration/60,audiusTracks[i].duration%60);
                    label(s,1780,y+27,duration,1,157,176,164);
                }
            }
            if (audiusState==2)
                label(s,338,888,"TRACK SELECTED - AUDIO DECODER / STREAM PLAYBACK NOT CONNECTED YET",1,255,190,105);
            else
                label(s,338,888,"CATALOGUE LOADED - SELECT A TRACK OR PRESS TRIANGLE TO SEARCH AGAIN",1,115,206,151);
        }
    } else if(tab==2) {
        rounded(s,338,205,1538,386,18,29,36,33);
        rounded(s,900,270,112,112,32,39,79,58);
        label(s,938,307,"M",4,29,185,84);
        label(s,790,425,"YOUR LIBRARY IS EMPTY",3,whiteR,whiteG,whiteB);
        label(s,735,474,"Your saved albums and tracks will appear here.",2,166,183,172);
        label(s,848,526,"Connect a supported music source to begin.",2,166,183,172);
    } else {
        rounded(s,338,205,1538,386,18,29,36,33);
        rounded(s,900,270,112,112,32,39,79,58);
        label(s,938,307,"F",4,29,185,84);
        label(s,805,425,"NO FAVORITES YET",3,whiteR,whiteG,whiteB);
        label(s,760,474,"Your favorite tracks will be collected here.",2,166,183,172);
        label(s,850,526,"Select a track to add it to favorites.",2,166,183,172);
    }

    // Bottom player: deliberately reports the real state, never fake playback.
    rect(s,0,910,W,2,47,57,52);
    rect(s,0,912,W,168,17,22,20);
    rounded(s,36,942,58,58,13,38,49,44);
    label(s,57,961,"R",3,29,185,84);
    label(s,116,946,"NOTHING PLAYING",2,whiteR,whiteG,whiteB);
    label(s,116,978,"Choose a supported audio stream",1,145,164,151);
    rounded(s,830,950,42,42,14,34,44,40);
    label(s,846,964,"|",2,180,194,186);
    rounded(s,900,944,54,54,18,29,185,84);
    label(s,919,962,">",3,12,18,15);
    rounded(s,982,950,42,42,14,34,44,40);
    label(s,997,964,"||",1,180,194,186);
    label(s,1510,951,"READY",2,112,206,151);
    label(s,338,1042,"UP / DOWN SELECT   LEFT / RIGHT SECTIONS   X SELECT   O BACK   L1 / R1 TABS",
          1,119,139,128);

    if(notice) {
        rounded(s,1250,830,626,56,14,54,72,62);
        label(s,1280,850,"MUSIC SOURCE NOT CONNECTED",2,220,233,224);
    }
}
static bool audius_open_search(int userId) {
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG) < 0) {
        audiusState = -1;
        return false;
    }

    wchar_t input[128] = {0};
    for (size_t i=0; audiusQuery[i] && i<120; ++i)
        input[i] = (wchar_t)(unsigned char)audiusQuery[i];

    OrbisImeDialogSetting setting;
    memset(&setting,0,sizeof(setting));
    setting.userId = (uint32_t)userId;
    setting.type = ORBIS_TYPE_DEFAULT;
    setting.enterLabel = ORBIS_BUTTON_LABEL_SEARCH;
    setting.maxTextLength = 120;
    setting.inputTextBuffer = input;
    setting.posx = 0.0f;
    setting.posy = 0.0f;
    setting.horizontalAlignment = ORBIS_H_CENTER;
    setting.verticalAlignment = ORBIS_V_CENTER;
    setting.placeholder = L"Artist, track or genre";
    setting.title = L"Search Audius";

    int rc = sceImeDialogInit(&setting,NULL);
    if (rc < 0) {
        audiusState = -1;
        return false;
    }

    while (sceImeDialogGetStatus() == ORBIS_DIALOG_STATUS_RUNNING)
        sceKernelUsleep(10000);

    OrbisDialogResult result;
    memset(&result,0,sizeof(result));
    sceImeDialogGetResult(&result);
    sceImeDialogTerm();

    if (result.endstatus != ORBIS_DIALOG_OK)
        return false;

    size_t out=0;
    while (input[out] && out+1<sizeof(audiusQuery)) {
        wchar_t ch=input[out];
        audiusQuery[out]=(ch>=32 && ch<127)?(char)ch:' ';
        ++out;
    }
    audiusQuery[out]=0;
    if (!audiusQuery[0]) return false;

    audiusCount = audius_search(audiusQuery,audiusTracks,8);
    audiusSelected = 0;
    audiusState = audiusCount>0 ? 1 : -1;
    write_status(audiusCount>0 ? "RIDA_AUDIUS_SEARCH_OK" : "RIDA_AUDIUS_SEARCH_FAILED");
    return audiusCount>0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    write_status("RIDA_MUSIC_MAIN_ENTERED");

    Scene2D scene(W, H, 4);
    write_status("RIDA_MUSIC_SCENE_CREATED");

    if (!scene.Init(VIDEO_MEMORY_SIZE, 2)) {
        printf("RIDA MUSIC: VIDEO INIT FAILED\n");

        for (;;) {
            sceKernelUsleep(1000000);
        }
    }

    write_status("RIDA_MUSIC_VIDEO_INIT_OK");

    int pad = -1;
    int user = -1;

    write_status("RIDA_MUSIC_BEFORE_PAD_INIT");
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

    int notice = 0;

    write_status("RIDA_MUSIC_ENTERING_MAIN_LOOP");
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
            if (tab == 1 && audiusCount > 0 && (up || down)) {
                if (up) audiusSelected = (audiusSelected + audiusCount - 1) % audiusCount;
                else audiusSelected = (audiusSelected + 1) % audiusCount;
            } else if (up) {
                selected = (selected + 3) % 4;
            } else if (down) {
                selected = (selected + 1) % 4;
            } else if (left) {
                tab = (tab + 3) % 4;
                selected = 0;
            } else if (right) {
                tab = (tab + 1) % 4;
                selected = 0;
            }
            navigationCooldown = NAV_REPEAT_DELAY;
        }

        if (tab == 1 && (pressed & ORBIS_PAD_BUTTON_TRIANGLE)) {
            audius_open_search(user);
            navigationCooldown = 20;
        }

        if (pressed & ORBIS_PAD_BUTTON_L1) {
            tab = (tab + 3) % 4;
            selected = 0;
        }

        if (pressed & ORBIS_PAD_BUTTON_R1) {
            tab = (tab + 1) % 4;
            selected = 0;
        }

        if (pressed & ORBIS_PAD_BUTTON_CROSS) {
            if (tab == 1 && audiusCount > 0) {
                audiusState = 2;
            } else {
                tab = selected;
            }
            notice = 0;
        }

        if (pressed & ORBIS_PAD_BUTTON_CIRCLE) {
            notice = 0;
            if (tab != 0) { tab = 0; selected = 0; }
        }

        if (pressed & ORBIS_PAD_BUTTON_OPTIONS) {
            tab = 0;
            selected = 0;
            notice = 0;
        }

        scene.SetActiveFrameBuffer(frame % 2);
        draw(&scene, tab, selected, notice);

        scene.SubmitFlip(frame);
        scene.FrameWait(frame);
        if (frame == 0) write_status("RIDA_MUSIC_FIRST_FRAME_SUBMITTED");

        frame++;

        sceKernelUsleep(16000);
    }

    return 0;
}
