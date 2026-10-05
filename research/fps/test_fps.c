#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
static long long millis(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;
}
#include "../../hud_fps.h"
int main(int argc,char **argv) {
    (void)argv;
    struct hud_fps f={.fd=-1,.value=-1};
    fps_line(&f,"drm_vblank_event_delivered: file=0000000000000000, crtc=0, seq=1");assert(f.count==0);
    fps_line(&f,"drm_vblank_event_delivered: file=abcd, crtc=1, seq=1");assert(f.count==0);
    fps_line(&f,"drm_vblank_event_delivered: file=abcd, crtc=0, seq=1");assert(f.count==1);
    fps_line(&f,"drm_vblank_event_delivered: file=abcd, crtc=0, seq=2");assert(f.count==2);
    fps_line(&f,"drm_vblank_event_delivered: file=ef01, crtc=0, seq=3");assert(f.conflict&&f.count==2);
    fps_line(&f,"CPU: 0 [LOST 1 EVENTS]");assert(f.lost);
    puts("PASS null/HUD exclusion, screen filter, completion count, conflicting client and loss");
    if(argc<2)return 0;
    f=(struct hud_fps){.fd=-1,.value=-1};
    if(fps_open(&f)){perror("fps_open");return 1;}
    for(int i=0;i<160;i++){fps_poll(&f);if(i%20==19)printf("PRESENT FPS %.1f\n",f.value);usleep(50000);}
    fps_close(&f);puts("trace closed");return 0;
}
