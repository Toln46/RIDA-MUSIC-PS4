#pragma once
#include <stddef.h>

struct AudiusTrack {
    char id[64];
    char title[128];
    char artist[96];
    int duration;
};

bool audius_init();
int audius_search(const char* query, AudiusTrack* tracks, int capacity);
const char* audius_last_error();
