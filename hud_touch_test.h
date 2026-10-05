#ifndef HUD_TOUCH_TEST_H
#define HUD_TOUCH_TEST_H
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#ifdef HUD_REGION_TEST
#include <sys/un.h>
#include "research/touch-isolation/region_policy.h"
#endif
#include "research/touch-isolation/touch_tap.h"
struct hud_touch_control {
    int fd,observer,peer,child,owned,wanted,ff,x,y,down,packet,contacts,dropped;
    long long idle;
    unsigned clicks,observed;
    struct input_absinfo ax,ay;
    struct touch_tap tap;
#ifdef HUD_REGION_TEST
    int route,owner,game_down;
#endif
};
#ifdef HUD_REGION_TEST
/* Gesture owner is fixed at down. Crossing into HUD releases the game and
 * cancels until up; crossing out never starts an accidental game gesture. */
static int region_frame(struct hud_touch_control *t,int contacts) {
    int inside=t->x>=ACCEL_X&&t->x<ACCEL_X+ACCEL_W&&t->y>=ACCEL_Y&&t->y<ACCEL_Y+ACCEL_H;
    if(contacts)t->owner=region_owner(t->owner,contacts,inside);
    int down=contacts==1&&t->owner==2;
    if(down||t->game_down) {
        struct input_event e[6]={{.type=EV_KEY,.code=BTN_TOUCH,.value=down},
            {.type=EV_ABS,.code=ABS_MT_TRACKING_ID,.value=down?0:-1},
            {.type=EV_ABS,.code=ABS_MT_POSITION_X,.value=t->x*640/639},
            {.type=EV_ABS,.code=ABS_MT_POSITION_Y,.value=t->y*480/479},
            {.type=EV_SYN,.code=SYN_MT_REPORT},{.type=EV_SYN,.code=SYN_REPORT}};
        if(send(t->route,e,sizeof(e),MSG_NOSIGNAL)!=sizeof(e)){stop=1;return 0;}
        printf("REGION GAME %s x=%d y=%d\n",down?"DOWN/MOVE":"UP",t->x,t->y);fflush(stdout);
    }
    t->game_down=down;
    int panel=t->owner==1;
    if(!contacts)t->owner=0;
    return panel;
}
#endif
static int r2_write(int fd,int value) {
    struct input_event ev[2]={{.type=EV_KEY,.code=BTN_TR2,.value=value},
                             {.type=EV_SYN,.code=SYN_REPORT}};
    return write(fd,ev,sizeof(ev))==(ssize_t)sizeof(ev)?0:-1;
}
/* The helper holds no DRM/lock/touch FDs. EOF releases R2 even if HUD is killed.
 * It owns only the synthetic latch; physical R2 remains the game's input. */
static int r2_helper(struct hud_touch_control *t,int controller_fd) {
    char path[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",controller_fd);
    int output=open(path,O_RDWR|O_NONBLOCK|O_CLOEXEC),pair[2];
    if(output<0)return -1;
    if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair)){close(output);return -1;}
    pid_t child=fork();
    if(child<0){close(output);close(pair[0]);close(pair[1]);return -1;}
    if(!child) {
        for(int fd=0;fd<1024;fd++)if(fd!=output&&fd!=pair[1])close(fd);
        signal(SIGUSR1,SIG_IGN);signal(SIGUSR2,SIG_IGN);
        int held=0,request;
        while(!stop) {
            struct pollfd p={pair[1],POLLIN,0};
            int ready=poll(&p,1,200);
            if(ready<0){if(errno==EINTR)continue;break;}
            if(!ready)continue;
            if(recv(pair[1],&request,sizeof(request),0)!=sizeof(request))break;
            int rc=-1;
            if(request==0||request==1) {
                unsigned char keys[KEY_MAX/8+1]={0};
                if(!request || held || (ioctl(output,EVIOCGKEY(sizeof(keys)),keys)>=0&&
                    !(keys[BTN_TR2/8]&(1<<(BTN_TR2%8))))) {
                    if(request)held=1; /* cleanup even if a partial press failed */
                    rc=r2_write(output,request);
                    if(!rc)held=request;
                }
            }
            if(send(pair[1],&rc,sizeof(rc),MSG_NOSIGNAL)!=sizeof(rc))break;
        }
        if(held)r2_write(output,0);
        close(output);close(pair[1]);_exit(0);
    }
    close(output);close(pair[1]);t->peer=pair[0];t->child=child;return 0;
}
static int accel_set(struct hud_touch_control *t,int on) {
    if(t->peer<0)return -1;
    if(send(t->peer,&on,sizeof(on),MSG_NOSIGNAL)!=sizeof(on))return -1;
    struct pollfd p={t->peer,POLLIN,0};int rc=-1;
    if(poll(&p,1,300)<=0||recv(t->peer,&rc,sizeof(rc),0)!=sizeof(rc)||rc)return -1;
    t->ff=on;
    printf("ACCEL R2 %s clicks=%u\n",on?"ON":"OFF",t->clicks);fflush(stdout);return 0;
}
static int touch_held(int fd) {
    unsigned char keys[KEY_MAX/8+1]={0};
    if(ioctl(fd,EVIOCGKEY(sizeof(keys)),keys)<0)return -1;
    return !!(keys[BTN_TOUCH/8]&(1<<(BTN_TOUCH%8)));
}
static void touch_packets(struct hud_touch_control *t) {
    struct input_event e;
    for(int n=0;n<4096&&read(t->fd,&e,sizeof(e))==sizeof(e);n++) {
        if(e.type==EV_SYN&&e.code==SYN_DROPPED){t->dropped=1;touch_tap_cancel(&t->tap);}
        if(t->dropped) {
            if(e.type==EV_SYN&&e.code==SYN_REPORT) {
                t->dropped=0;t->packet=t->contacts=0;t->down=touch_held(t->fd);
#ifdef HUD_REGION_TEST
                if(t->owned){region_frame(t,0);t->owner=t->down?3:0;}
#endif
            }
            continue;
        }
        if(e.type==EV_KEY&&e.code==BTN_TOUCH)t->down=e.value!=0;
        if(e.type==EV_ABS) {
            if(e.code==ABS_MT_POSITION_X){t->x=(e.value-t->ax.minimum)*639/(t->ax.maximum-t->ax.minimum);t->packet=1;}
            if(e.code==ABS_MT_POSITION_Y){t->y=(e.value-t->ay.minimum)*479/(t->ay.maximum-t->ay.minimum);t->packet=1;}
            if(e.code==ABS_MT_TRACKING_ID&&e.value>=0)t->packet=1;
        }
        if(e.type==EV_SYN&&e.code==SYN_MT_REPORT){t->contacts+=t->packet;t->packet=0;}
        if(e.type==EV_SYN&&e.code==SYN_REPORT) {
            int contacts=t->down>0?(t->contacts+t->packet):t->down;
            if(t->down>0&&!contacts)contacts=1;
            int panel=t->owned;
#ifdef HUD_REGION_TEST
            if(t->owned)panel=region_frame(t,contacts)||!contacts;
#endif
            int hit=touch_tap_frame_rect(&t->tap,panel,1,contacts,t->x,t->y,
                                        millis(),ACCEL_X,ACCEL_Y,ACCEL_X+ACCEL_W,ACCEL_Y+ACCEL_H);
            if(hit) {
                t->clicks++;
                if(accel_set(t,!t->ff))fprintf(stderr,"ACCEL R2 command failed\n");
                printf("TOUCH BUTTON click=%u observer_events=%u\n",t->clicks,t->observed);fflush(stdout);
            }
            t->packet=t->contacts=0;
        }
    }
    if(t->observer>=0)while(read(t->observer,&e,sizeof(e))==sizeof(e))if(t->owned)t->observed++;
}
static void touch_control_close(struct hud_touch_control *t) {
#ifdef HUD_REGION_TEST
    if(t->route>=0){close(t->route);t->route=-1;}
    t->owner=t->game_down=0;
#endif
    if(t->fd>=0)close(t->fd);if(t->observer>=0)close(t->observer);
    t->fd=t->observer=-1;t->owned=0;t->idle=0;t->down=0;
    touch_tap_cancel(&t->tap);
}
static int touch_control_open(struct hud_touch_control *t) {
#ifdef HUD_REGION_TEST
    struct sockaddr_un addr={.sun_family=AF_UNIX};strcpy(addr.sun_path,"/data/local/tmp/gamma-region.sock");
    t->route=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);
    if(t->route<0||connect(t->route,(void*)&addr,sizeof(addr))) {touch_control_close(t);return -1;}
#endif
    t->fd=touch_open();if(t->fd<0)return -1;
    unsigned char caps[ABS_MAX/8+1]={0};
    if(ioctl(t->fd,EVIOCGBIT(EV_ABS,sizeof(caps)),caps)<0||
       (caps[ABS_MT_SLOT/8]&(1<<(ABS_MT_SLOT%8)))||
       ioctl(t->fd,EVIOCGABS(ABS_MT_POSITION_X),&t->ax)<0||
       ioctl(t->fd,EVIOCGABS(ABS_MT_POSITION_Y),&t->ay)<0||
       t->ax.maximum<=t->ax.minimum||t->ay.maximum<=t->ay.minimum) {
        touch_control_close(t);return -1;
    }
    char path[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",t->fd);
    t->observer=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(t->observer<0){touch_control_close(t);return -1;}
    touch_tap_cancel(&t->tap);return 0;
}
#endif
