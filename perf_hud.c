#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/file.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <drm.h>
#include <drm_mode.h>
#include <drm_fourcc.h>

static volatile sig_atomic_t stop;
static volatile sig_atomic_t toggle;
static volatile sig_atomic_t touch_change;
static void interrupted(int sig) { (void)sig; stop = 1; }
static void toggled(int sig) { (void)sig; toggle=1; }
static void touch_changed(int sig) { (void)sig; touch_change=1; }

struct touch_probe { int fd,x,y,down,reported,count,inside; unsigned long events; };
static int touch_open(void) {
    for(int i=0;i<64;i++) {
        char path[128],name[64];snprintf(path,sizeof(path),"/sys/class/input/event%d/device/name",i);
        FILE *f=fopen(path,"r");if(!f)continue;
        int ok=fgets(name,sizeof(name),f)!=NULL;fclose(f);
        if(ok&&!strcmp(name,"gt9xx-0\n")) {
            snprintf(path,sizeof(path),"/dev/input/event%d",i);
            return open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        }
    }return -1;
}
static void touch_read(struct touch_probe *t) {
    struct input_event ev;
    for(int n=0;n<1024&&read(t->fd,&ev,sizeof(ev))==(ssize_t)sizeof(ev);n++) {
        t->events++;
        if(ev.type==EV_SYN&&ev.code==SYN_DROPPED) {
            t->down=t->reported=0;t->x=t->y=-1;continue;
        }
        if(ev.type==EV_ABS&&ev.code==ABS_MT_POSITION_X)t->x=ev.value;
        if(ev.type==EV_ABS&&ev.code==ABS_MT_POSITION_Y)t->y=ev.value;
        if(ev.type==EV_KEY&&ev.code==BTN_TOUCH)t->down=ev.value!=0;
        if(ev.type==EV_SYN&&ev.code==SYN_REPORT) {
            if(t->down&&!t->reported&&t->x>=0&&t->y>=0) {
                t->count++;t->inside=t->x>=8&&t->x<288&&t->y>=8&&t->y<96;
                printf("TOUCH DOWN x=%d y=%d hit=%d count=%d events=%lu\n",t->x,t->y,t->inside,t->count,t->events);
                t->reported=1;fflush(stdout);
            }else if(!t->down&&t->reported) {
                printf("TOUCH UP x=%d y=%d events=%lu\n",t->x,t->y,t->events);
                t->reported=0;t->x=t->y=-1;fflush(stdout);
            }
        }
    }
}

#include "hud_ui.h"
static long readnum(const char *path) {
    FILE *f=fopen(path,"r");long n=-1;
    if(f){if(fscanf(f,"%ld",&n)!=1)n=-1;fclose(f);}return n;
}
static int gpu_usage(void) {
    FILE *f=fopen("/sys/class/devfreq/fde60000.gpu/load","r");
    int value=-1;unsigned long long frequency;
    if(f){if(fscanf(f,"%d@%lluHz",&value,&frequency)!=2||value<0||value>100)value=-1;fclose(f);}
    return value;
}
/* Battery terminal power, not total system draw when externally powered.
 * current_now is signed microamps; read success must be separate from its sign. */
static double battery_power(void) {
    long voltage=0,current=0;
    FILE *f=fopen("/sys/class/power_supply/battery/voltage_now","r");
    if(!f)return -1;
    int ok=fscanf(f,"%ld",&voltage)==1;fclose(f);
    if(!ok||voltage<=0)return -1;
    f=fopen("/sys/class/power_supply/battery/current_now","r");
    if(!f)return -1;
    ok=fscanf(f,"%ld",&current)==1;fclose(f);
    if(!ok)return -1;
    return (voltage/1000000.0)*fabs(current/1000000.0);
}
static void memory_sample(long *total,long *available) {
    *total=*available=-1;FILE *f=fopen("/proc/meminfo","r");if(!f)return;
    char line[128];long value;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"MemTotal: %ld",&value)==1)*total=value;
        if(sscanf(line,"MemAvailable: %ld",&value)==1)*available=value;
    }fclose(f);
}
static int chinese_locale(void) {
    char locale[64]={0};FILE *f=popen("/system/bin/getprop persist.sys.locale","r");
    if(f){fgets(locale,sizeof(locale),f);pclose(f);}
    if(!locale[0]||locale[0]=='\n') {
        f=popen("/system/bin/getprop ro.product.locale","r");
        if(f){fgets(locale,sizeof(locale),f);pclose(f);}
    }
    if(strncmp(locale,"zh",2))return 0;
    return strstr(locale,"Hant")||strstr(locale,"TW")||strstr(locale,"HK")||strstr(locale,"MO")?2:1;
}
static void thermal_path(const char *type,char *out,size_t size) {
    out[0]=0;
    for(int i=0;i<32;i++) {
        char path[128],name[64];snprintf(path,sizeof(path),"/sys/class/thermal/thermal_zone%d/type",i);
        FILE *f=fopen(path,"r");if(!f)continue;
        int ok=fscanf(f,"%63s",name)==1;fclose(f);
        if(ok&&!strcmp(name,type)){snprintf(out,size,"/sys/class/thermal/thermal_zone%d/temp",i);return;}
    }
}
static int cpu_sample(unsigned long long *idle,unsigned long long *total) {
    unsigned long long v[8]={0};FILE *f=fopen("/proc/stat","r");if(!f)return -1;
    int n=fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",v,v+1,v+2,v+3,v+4,v+5,v+6,v+7);fclose(f);
    if(n<4)return -1;*idle=v[3]+v[4];*total=0;for(int i=0;i<8;i++)*total+=v[i];return 0;
}
static int controller(void) {
    for(int i=0;i<64;i++) {
        char path[128],name[128];snprintf(path,sizeof(path),"/sys/class/input/event%d/device/name",i);
        FILE *f=fopen(path,"r");if(!f)continue;
        int ok=fgets(name,sizeof(name),f)!=NULL;fclose(f);
        if(ok&&!strncmp(name,"Xbox Wireless Controller",24)) {
            snprintf(path,sizeof(path),"/dev/input/event%d",i);return open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        }
    }return -1;
}
static int game_alive(int pid) {
    char path[64],name[64];snprintf(path,sizeof(path),"/proc/%d/comm",pid);
    FILE *f=fopen(path,"r");if(!f)return 0;
    int ok=fgets(name,sizeof(name),f)!=NULL;fclose(f);return ok&&!strcmp(name,"drastic-nano\n");
}
static long long millis(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long long)t.tv_sec*1000+t.tv_nsec/1000000;
}
#include "hud_fps.h"
static int commit(int fd, uint32_t plane, uint32_t *props, uint64_t *values, uint32_t n, uint32_t flags) {
    struct drm_mode_atomic a = { .flags=flags, .count_objs=1,
        .objs_ptr=(uintptr_t)&plane, .count_props_ptr=(uintptr_t)&n,
        .props_ptr=(uintptr_t)props, .prop_values_ptr=(uintptr_t)values };
    return ioctl(fd, DRM_IOCTL_MODE_ATOMIC, &a);
}
static int make_buffer(int fd,uint32_t width,uint32_t height,struct drm_mode_create_dumb *d,struct drm_mode_fb_cmd2 *fb,void **pixels) {
    *d=(struct drm_mode_create_dumb){.width=width,.height=height,.bpp=32};
    if(ioctl(fd,DRM_IOCTL_MODE_CREATE_DUMB,d))return -1;
    struct drm_mode_map_dumb m={.handle=d->handle};
    if(ioctl(fd,DRM_IOCTL_MODE_MAP_DUMB,&m))return -1;
    *pixels=mmap(NULL,d->size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,m.offset);
    if(*pixels==MAP_FAILED)return -1;
    *fb=(struct drm_mode_fb_cmd2){.width=width,.height=height,.pixel_format=DRM_FORMAT_XRGB8888};
    fb->handles[0]=d->handle;fb->pitches[0]=d->pitch;
    return ioctl(fd,DRM_IOCTL_MODE_ADDFB2,fb);
}
static void free_buffer(int fd,struct drm_mode_create_dumb *d,struct drm_mode_fb_cmd2 *fb,void *pixels) {
    if(fb->fb_id)ioctl(fd,DRM_IOCTL_MODE_RMFB,&fb->fb_id);
    if(pixels!=MAP_FAILED)munmap(pixels,d->size);
    if(d->handle){struct drm_mode_destroy_dumb dd={.handle=d->handle};ioctl(fd,DRM_IOCTL_MODE_DESTROY_DUMB,&dd);}
}
/* Hardware alpha only: no framebuffer redraws during the transition. */
static int fade_panel(int fd,uint32_t plane,uint32_t alpha,int showing) {
    if(!alpha)return 0;
    long long start=millis();
    for(;;) {
        double t=(millis()-start)/180.0;if(t>1)t=1;
        double eased=t*t*(3-2*t);
        uint64_t value=(uint64_t)((showing?eased:1-eased)*65535);
        if(commit(fd,plane,&alpha,&value,1,0))return -1;
        if(t>=1||stop)break;
        usleep(8000);
    }
    return 0;
}
#ifdef HUD_TOUCH_TEST
#include "hud_touch_test.h"
#endif
int main(int argc,char **argv) {
    if(argc!=2){fprintf(stderr,"usage: perf_hud DRASTIC_PID\n");return 1;}
    int game_pid=atoi(argv[1]);if(game_pid<=0||!game_alive(game_pid)){fprintf(stderr,"Nano game not running\n");return 1;}
    int lockfd=open("/data/local/tmp/gamma-perf-hud.lock",O_RDWR|O_CREAT|O_CLOEXEC,0600);
    if(lockfd<0||flock(lockfd,LOCK_EX|LOCK_NB)){fprintf(stderr,"HUD already running or lock unavailable\n");return 1;}
    const uint32_t plane=266, crtc=74, width=HUD_WIDTH, height=HUD_HEIGHT;
    int result=1, attached=0, fade_in=0;
    struct hud_fps fps={.fd=-1,.value=-1};int fps_attempted=0;
#ifdef HUD_TOUCH_TEST
    struct hud_touch_control control={.fd=-1,.observer=-1,.peer=-1,.child=-1};
#ifdef HUD_REGION_TEST
    control.route=-1;
#endif
#endif
    void *pixels=MAP_FAILED,*back_pixels=MAP_FAILED;
    struct drm_mode_create_dumb d={.width=width,.height=height,.bpp=32};
    struct drm_mode_fb_cmd2 fb={0},back_fb={0};
    struct drm_mode_create_dumb back_d={0};
    int fd=open("/dev/dri/card0",O_RDWR|O_CLOEXEC);
    if(fd<0) {perror("open");return 1;}
    signal(SIGTERM,interrupted);signal(SIGINT,interrupted);signal(SIGHUP,interrupted);
    signal(SIGUSR1,toggled);
    signal(SIGUSR2,touch_changed);
    struct drm_set_client_cap cap={DRM_CLIENT_CAP_ATOMIC,1};
    if(ioctl(fd,DRM_IOCTL_SET_CLIENT_CAP,&cap)) goto done;
    struct drm_mode_get_plane p={.plane_id=plane};
    if(ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)||p.crtc_id||p.fb_id||!(p.possible_crtcs&1)) {
        fprintf(stderr,"target plane unavailable\n");goto done;
    }
    uint32_t allprops[128], props[12];uint64_t allvalues[128], values[12], original_zpos=0,original_alpha=65535;
    const char *names[]={"FB_ID","CRTC_ID","CRTC_X","CRTC_Y","CRTC_W","CRTC_H","SRC_X","SRC_Y","SRC_W","SRC_H","zpos","alpha"};
    uint64_t desired[]={0,crtc,0,0,width,height,0,0,width<<16,height<<16,3,65535};
#ifdef HUD_REGION_TEST
    /* Crop the existing framebuffer to the button: remaining lower screen is
     * the game's own plane, without requiring per-pixel alpha support. */
    desired[2]=ACCEL_X;desired[3]=ACCEL_Y;desired[4]=ACCEL_W;desired[5]=ACCEL_H;
    desired[6]=(uint64_t)ACCEL_X<<16;desired[7]=(uint64_t)ACCEL_Y<<16;
    desired[8]=(uint64_t)ACCEL_W<<16;desired[9]=(uint64_t)ACCEL_H<<16;
#endif
    struct drm_mode_obj_get_properties q={.obj_id=plane,.obj_type=DRM_MODE_OBJECT_PLANE};
    if(ioctl(fd,DRM_IOCTL_MODE_OBJ_GETPROPERTIES,&q)||q.count_props>128)goto done;
    q.props_ptr=(uintptr_t)allprops;q.prop_values_ptr=(uintptr_t)allvalues;
    if(ioctl(fd,DRM_IOCTL_MODE_OBJ_GETPROPERTIES,&q)||q.count_props>128)goto done;
    memset(props,0,sizeof(props));
    for(uint32_t j=0;j<q.count_props;j++) {
        struct drm_mode_get_property prop={.prop_id=allprops[j]};
        if(ioctl(fd,DRM_IOCTL_MODE_GETPROPERTY,&prop))goto done;
        for(int k=0;k<12;k++)if(!strcmp(prop.name,names[k])) {
            props[k]=prop.prop_id;
            if(k==10)original_zpos=allvalues[j];
            if(k==11)original_alpha=allvalues[j];
        }
    }
    for(int k=0;k<11;k++)if(!props[k])goto done;
    uint32_t property_count=props[11]?12:11;
    if(make_buffer(fd,width,height,&d,&fb,&pixels)||make_buffer(fd,width,height,&back_d,&back_fb,&back_pixels))goto done;
    hud_render(pixels,d.pitch,-1,-1,-1,-1,-1,-1,-1,-1,"UNKNOWN",NULL,NULL,0,-1,-1,-1,chinese_locale());
    memcpy(values,desired,sizeof(values));values[0]=fb.fb_id;
    if(commit(fd,plane,props,values,property_count,DRM_MODE_ATOMIC_TEST_ONLY))goto done;
    /* Recheck immediately before attaching; only touch this previously idle plane. */
    p.count_format_types=0;
    if(ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)||p.crtc_id||p.fb_id)goto done;
    /* Start hidden: keep the game plane unobscured until BTN_MODE. */
    int input=controller(),mode_down=0,allapps_down=0;
#ifdef HUD_TOUCH_TEST
    if(input<0||r2_helper(&control,input)){fprintf(stderr,"R2 helper unavailable\n");goto done;}
#endif
    int touch_enabled=access("/data/adb/gamma-perf/touch-probe",F_OK)==0;
#ifdef HUD_TOUCH_TEST
    touch_enabled=0;
#endif
    struct touch_probe touch={.fd=-1,.x=-1,.y=-1};
    long long last_hotkey=-1000;
    if(input>=0) {
        unsigned char keys[KEY_MAX/8+1]={0};
        if(!ioctl(input,EVIOCGKEY(sizeof(keys)),keys)) {
            mode_down=!!(keys[BTN_MODE/8]&(1<<(BTN_MODE%8)));
            allapps_down=!!(keys[KEY_ALL_APPLICATIONS/8]&(1<<(KEY_ALL_APPLICATIONS%8)));
        }
    }
    char soc[128],gpu[128];thermal_path("soc-thermal",soc,sizeof(soc));thermal_path("gpu-thermal",gpu,sizeof(gpu));
    unsigned long long idle0=0,total0=0;int have_cpu=cpu_sample(&idle0,&total0)==0;
    long long next=0;
    long soc_history[60],gpu_history[60];int history_count=0;
    int zh=0;long long locale_next=0;
    printf("HUD hidden by default; BTN_MODE or SIGUSR1 toggles; game exit stops; input=%d\n",input);fflush(stdout);
    while(!stop) {
        if(touch_change) {
            touch_change=0;touch_enabled=access("/data/adb/gamma-perf/touch-probe",F_OK)==0;
            printf("TOUCH probe %s\n",touch_enabled?"enabled":"disabled");fflush(stdout);
        }
        if((!touch_enabled||!attached)&&touch.fd>=0) {
            close(touch.fd);touch.fd=-1;touch.down=touch.reported=0;touch.x=touch.y=-1;
        }
        if(touch_enabled&&attached&&touch.fd<0) {
            touch.fd=touch_open();
            if(touch.fd<0){touch_enabled=0;fprintf(stderr,"TOUCH open failed\n");}
            else {printf("TOUCH listening: gt9xx-0, no grab\n");fflush(stdout);}
        }
        if(touch.fd>=0)touch_read(&touch);
        if(input>=0) {
            struct input_event ev;
            while(read(input,&ev,sizeof(ev))==(ssize_t)sizeof(ev)) {
#ifdef HUD_TOUCH_TEST
                if(ev.type==EV_KEY&&ev.code==BTN_TR2&&ev.value==0&&control.ff) {
                    /* A physical R2 release cancels the synthetic latch. */
                    if(accel_set(&control,0))break;
                    next=0;
                }
#endif
                if(ev.type!=EV_KEY||ev.value==2)continue;
                /* GammaOS may map physical BTN_MODE to KEY_ALL_APPLICATIONS.
                 * Handle either code, ignoring repeats and paired alias events. */
                int *pressed=NULL;
                if(ev.code==BTN_MODE)pressed=&mode_down;
                if(ev.code==KEY_ALL_APPLICATIONS)pressed=&allapps_down;
                if(!pressed)continue;
                if(ev.value==1&&!*pressed) {
                    long long now=millis();
                    if(now-last_hotkey>=250) {
                        toggle=1;last_hotkey=now;
                        printf("BTN_MODE hotkey: code=%u\n",ev.code);fflush(stdout);
                    }
                }
                *pressed=ev.value!=0;
            }
        }
#ifdef HUD_TOUCH_TEST
        if(toggle) {
            toggle=0;control.wanted=!control.wanted;touch_tap_cancel(&control.tap);
            if(control.wanted&&control.fd<0&&touch_control_open(&control)) {
                control.wanted=0;fprintf(stderr,"Interactive touch unavailable\n");
            }
        }
        if(control.fd>=0) {
#ifdef HUD_REGION_TEST
            struct pollfd route_status={control.route,0,0};
            if(control.route<0||(poll(&route_status,1,0)>0&&(route_status.revents&(POLLHUP|POLLERR|POLLNVAL)))) {
                fprintf(stderr,"Touch forwarding helper ended; stopping region test\n");break;
            }
#endif
            unsigned clicks=control.clicks;touch_packets(&control);
            if(clicks!=control.clicks)next=0;
            int held=touch_held(control.fd);long long touch_now=millis();
            if(held!=0)control.idle=0;
            else if(!control.idle)control.idle=touch_now;
            if(control.wanted!=attached&&control.idle&&touch_now-control.idle>=100) {
                if(control.wanted) {
                    if(ioctl(control.fd,EVIOCGRAB,1)) {
                        fprintf(stderr,"Touch grab failed: %s\n",strerror(errno));
                        control.wanted=0;touch_control_close(&control);
                    }else if(touch_held(control.fd)!=0) {
                        ioctl(control.fd,EVIOCGRAB,0);control.idle=0;
                    }else {
                        control.owned=1;touch_tap_cancel(&control.tap);
                        touch_packets(&control);control.observed=0;
                        control.tap=(struct touch_tap){0};control.down=0;
                        control.packet=control.contacts=0;toggle=1;
                        printf("TOUCH exclusive acquired; Type A lower panel\n");fflush(stdout);
                    }
                }else toggle=1;
            }
            if(!control.wanted&&!attached)touch_control_close(&control);
        }
#endif
        if(toggle) {
            toggle=0;p.count_format_types=0;
            if(ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p))break;
            if(attached&&p.fb_id==fb.fb_id) {
                if(fade_panel(fd,plane,props[11],0))break;
                uint32_t pp[4]={props[0],props[1],props[10],props[11]};uint64_t vv[4]={0,0,original_zpos,original_alpha};
                if(commit(fd,plane,pp,vv,props[11]?4:3,0))break;attached=0;
                have_cpu=0;printf("HUD hidden; telemetry paused\n");
                history_count=0;
            }else if(!attached&&!p.fb_id&&!p.crtc_id) {
                values[11]=props[11]?0:65535;fade_in=props[11]!=0;
                if(commit(fd,plane,props,values,property_count,DRM_MODE_ATOMIC_TEST_ONLY)||commit(fd,plane,props,values,property_count,0))break;
                attached=1;next=0;have_cpu=0;printf("HUD visible; telemetry resumed\n");
            }else {fprintf(stderr,"plane ownership changed; stopping\n");break;}
            fflush(stdout);
        }
#ifdef HUD_TOUCH_TEST
        if(!attached&&!control.wanted&&control.fd>=0) {
            touch_control_close(&control);printf("TOUCH released to game\n");fflush(stdout);
        }
#endif
        if(attached) {
            if(!fps_attempted){fps_attempted=1;if(fps_open(&fps))fprintf(stderr,"Present FPS tracing unavailable\n");}
            fps_poll(&fps);
        }else {fps_close(&fps);fps_attempted=0;}
        long long now=millis();
        if(now>=next) {
            next=now+1000;if(!game_alive(game_pid))break;
            p.count_format_types=0;
            if(ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)||(attached&&p.fb_id!=fb.fb_id))break;
            /* Hidden: keep only input and lifecycle monitoring. No sensors,
             * /proc/stat samples, getprop subprocesses, pixel writes or logs. */
            if(!attached) { usleep(50000); continue; }
            long ts=readnum(soc),tg=readnum(gpu),cf=readnum("/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq"),gf=readnum("/sys/class/devfreq/fde60000.gpu/cur_freq");
            long cpu_max=readnum("/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_max_freq");
            long gpu_max=readnum("/sys/class/devfreq/fde60000.gpu/max_freq");
            int gpu_load=gpu_usage();
            unsigned long long idle1,total1;int load=-1;
            if(!cpu_sample(&idle1,&total1)) {
                if(have_cpu&&total1>total0&&idle1>=idle0)load=(int)(100-100*(idle1-idle0)/(total1-total0));
                idle0=idle1;total0=total1;have_cpu=1;
            }
            char cpu_temp[24]="N/A",gpu_temp[24]="N/A",cpu_freq[24]="N/A",gpu_freq[24]="N/A",cpu_load[24]="N/A",mode[24]="UNKNOWN";
            if(ts>=0)snprintf(cpu_temp,sizeof(cpu_temp),"%.1fC",ts/1000.0);
            if(tg>=0)snprintf(gpu_temp,sizeof(gpu_temp),"%.1fC",tg/1000.0);
            if(cf>=0)snprintf(cpu_freq,sizeof(cpu_freq),"%ldMHZ",cf/1000);
            if(gf>=0)snprintf(gpu_freq,sizeof(gpu_freq),"%ldMHZ",gf/1000000);
            if(load>=0&&load<=100)snprintf(cpu_load,sizeof(cpu_load),"%d%%",load);
            FILE *prop=popen("/system/bin/getprop persist.gammaos.performance_mode","r");
            if(prop){if(fgets(mode,sizeof(mode),prop)){mode[strcspn(mode,"\r\n")]=0;if(!mode[0])strcpy(mode,"DEFAULT");}pclose(prop);}
            char lines[4][64];
            snprintf(lines[0],sizeof(lines[0]),"SOC %s GPU %s",cpu_temp,gpu_temp);
            snprintf(lines[1],sizeof(lines[1]),"CPU %s %s",cpu_freq,cpu_load);
            snprintf(lines[2],sizeof(lines[2]),"GPU %s USE %d%%",gpu_freq,gpu_load);
            snprintf(lines[3],sizeof(lines[3]),"MODE %.16s",mode);
            if(history_count==60) {
                memmove(soc_history,soc_history+1,59*sizeof(long));
                memmove(gpu_history,gpu_history+1,59*sizeof(long));history_count=59;
            }
            soc_history[history_count]=ts;gpu_history[history_count]=tg;history_count++;
            long mem_total,mem_available;memory_sample(&mem_total,&mem_available);
            if(now>=locale_next){zh=chinese_locale();locale_next=now+5000;}
            /* Never modify the framebuffer currently being scanned out. The
             * blocking atomic FB_ID flip completes before old front is reused. */
            hud_render(back_pixels,back_d.pitch,ts,tg,cf,gf,load,gpu_load,cpu_max,gpu_max,mode,soc_history,gpu_history,history_count,mem_total,mem_available,battery_power(),zh);
            int capacity=(int)readnum("/sys/class/power_supply/battery/capacity"),charging=0;
            FILE *battery=fopen("/sys/class/power_supply/battery/status","r");
            if(battery){char status[32];if(fgets(status,sizeof(status),battery))charging=!strncmp(status,"Charging",8);fclose(battery);}
            static char clock[16]="--:--";static long long clock_next=0;
            if(now>=clock_next){
                FILE *date=popen("/system/bin/date +%H:%M","r");
                if(date){if(fgets(clock,sizeof(clock),date))clock[strcspn(clock,"\r\n")]=0;pclose(date);}
                clock_next=now+15000;
            }
            ui_status(back_pixels,back_d.pitch,capacity,charging,clock);
            ui_fps(back_pixels,back_d.pitch,fps.value);
            printf("PRESENT FPS %.1f\n",fps.value);fflush(stdout);
#ifdef HUD_TOUCH_TEST
            ui_accel(back_pixels,back_d.pitch,zh,control.ff);
            if(!access("/data/local/tmp/gamma-hud-capture",F_OK)) {
                FILE *capture=fopen("/data/local/tmp/gamma-hud-touch-test.ppm","wb");
                if(capture) {
                    fprintf(capture,"P6\n640 480\n255\n");
                    for(int yy=0;yy<480;yy++)for(int xx=0;xx<640;xx++) {
                        uint32_t c=((uint32_t*)((char*)back_pixels+yy*back_d.pitch))[xx];
                        unsigned char rgb[3]={c>>16,c>>8,c};fwrite(rgb,1,3,capture);
                    }
                    fclose(capture);unlink("/data/local/tmp/gamma-hud-capture");
                }
            }
#endif
            uint64_t target_fb=back_fb.fb_id;
            int flip=commit(fd,plane,props,&target_fb,1,0);
            if(flip) {
                p.count_format_types=0;
                if(ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)||p.fb_id!=back_fb.fb_id) {
                    fprintf(stderr,"HUD flip failed: %s\n",strerror(errno));
                    continue;
                }
            }
            struct drm_mode_create_dumb old_d=d;d=back_d;back_d=old_d;
            struct drm_mode_fb_cmd2 old_fb=fb;fb=back_fb;back_fb=old_fb;
            void *old_pixels=pixels;pixels=back_pixels;back_pixels=old_pixels;
            values[0]=fb.fb_id;
            if(fade_in){if(fade_panel(fd,plane,props[11],1))break;fade_in=0;values[11]=65535;}
            printf("%s | %s | %s | %s\n",lines[0],lines[1],lines[2],lines[3]);fflush(stdout);
        }
        usleep(50000);
    }
    if(input>=0)close(input);
    if(touch.fd>=0)close(touch.fd);
    result=0;
done:
    fps_close(&fps);
#ifdef HUD_TOUCH_TEST
    touch_control_close(&control);
    if(control.peer>=0)close(control.peer);
    if(control.child>0)while(waitpid(control.child,NULL,0)<0&&errno==EINTR){}
#endif
    if(result)perror("HUD");
    if(attached) {
        p.count_format_types=0;
        if(!ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)&&p.fb_id==fb.fb_id) {
            uint32_t cleanup_props[4]={props[0],props[1],props[10],props[11]};
            uint64_t cleanup_values[4]={0,0,original_zpos,original_alpha};
            if(commit(fd,plane,cleanup_props,cleanup_values,props[11]?4:3,0)) {perror("remove marker");result=1;}
        }
    }
    free_buffer(fd,&d,&fb,pixels);
    free_buffer(fd,&back_d,&back_fb,back_pixels);
    close(fd);close(lockfd);printf("HUD stopped, resources released\n");return result;
}
