#ifndef HUD_FPS_H
#define HUD_FPS_H
#include <sys/stat.h>
#include <dirent.h>
struct hud_fps {
    int fd,used,count,conflict,lost;
    long long window;
    double value;
    char path[192],client[32],line[512];
};
static int fps_control(const char *base,const char *leaf,const char *value) {
    char path[384];snprintf(path,sizeof(path),"%s/%s",base,leaf);
    int fd=open(path,O_WRONLY|O_CLOEXEC);if(fd<0)return -1;
    size_t n=strlen(value);int rc=write(fd,value,n)==(ssize_t)n?0:-1;close(fd);return rc;
}
static void fps_close(struct hud_fps *f) {
    if(f->fd>=0)close(f->fd);f->fd=-1;
    if(f->path[0]) {
        fps_control(f->path,"tracing_on","0");
        fps_control(f->path,"events/drm/drm_vblank_event_delivered/enable","0");
        rmdir(f->path);f->path[0]=0;
    }
    f->value=-1;f->used=f->count=f->conflict=f->lost=0;f->client[0]=0;
}
static int fps_open(struct hud_fps *f) {
    const char *root="/sys/kernel/tracing/instances";
    if(access(root,F_OK))root="/sys/kernel/debug/tracing/instances";
    /* Remove only our orphaned instances after an earlier uncatchable exit. */
    DIR *instances=opendir(root);struct dirent *entry;
    if(instances) {
        while((entry=readdir(instances))) {
            int owner=0;char extra;
            if(sscanf(entry->d_name,"gamma-hud-fps-%d%c",&owner,&extra)!=1||owner<=0)continue;
            char proc[64];snprintf(proc,sizeof(proc),"/proc/%d",owner);
            if(!access(proc,F_OK))continue;
            struct hud_fps orphan={.fd=-1};
            if(snprintf(orphan.path,sizeof(orphan.path),"%s/%s",root,entry->d_name)>=(int)sizeof(orphan.path))continue;
            fps_close(&orphan);
        }
        closedir(instances);
    }
    snprintf(f->path,sizeof(f->path),"%s/gamma-hud-fps-%d",root,getpid());
    if(mkdir(f->path,0700)){f->path[0]=0;return -1;}
    if(fps_control(f->path,"tracing_on","0")||fps_control(f->path,"buffer_size_kb","64")||
       fps_control(f->path,"trace_clock","mono")||
       fps_control(f->path,"events/drm/drm_vblank_event_delivered/filter","crtc == 0")||
       fps_control(f->path,"events/drm/drm_vblank_event_delivered/enable","1"))goto fail;
    char path[256];snprintf(path,sizeof(path),"%s/trace_pipe",f->path);
    f->fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(f->fd<0)goto fail;
    f->window=millis();f->value=-1;f->client[0]=0;
    if(fps_control(f->path,"tracing_on","1"))goto fail;
    return 0;
fail:fps_close(f);return -1;
}
static void fps_line(struct hud_fps *f,const char *line) {
    if(strstr(line,"LOST")||strstr(line,"MISSED")){f->lost=1;return;}
    const char *p=strstr(line,"drm_vblank_event_delivered: file=");if(!p)return;
    unsigned long long file;int crtc;unsigned seq;
    if(sscanf(p,"drm_vblank_event_delivered: file=%llx, crtc=%d, seq=%u",&file,&crtc,&seq)!=3||crtc!=0||!file)return;
    /* file=NULL is the driver's internal atomic completion, including HUD.
     * Require one non-null client while Nano owns the lower primary plane. */
    char client[32];snprintf(client,sizeof(client),"%llx",file);
    if(!f->client[0])strcpy(f->client,client);
    if(strcmp(f->client,client)){f->conflict=1;return;}
    f->count++;
}
static int fps_game_plane(void) {
    FILE *in=fopen("/sys/kernel/debug/dri/0/state","r");if(!in)return 0;
    char line[256],name[64];unsigned id;int primary=0,owned=0,lower=0;
    while(fgets(line,sizeof(line),in)) {
        if(!strncmp(line,"plane[",6)) {
            if(primary&&owned&&lower)break;
            primary=sscanf(line,"plane[%u]: %63s",&id,name)==2&&
                (!strcmp(name,"Smart0-win0")||!strcmp(name,"Cluster0-win0"));
            owned=lower=0;
        }
        if(primary&&strstr(line,"crtc=video_port0"))lower=1;
        if(primary&&strstr(line,"allocated by = drastic-nano"))owned=1;
    }
    fclose(in);return primary&&owned&&lower;
}
static void fps_poll(struct hud_fps *f) {
    if(f->fd<0)return;
    char buffer[4096];ssize_t n;
    for(int pass=0;pass<16&&(n=read(f->fd,buffer,sizeof(buffer)))>0;pass++) {
        for(ssize_t i=0;i<n;i++) {
            if(buffer[i]=='\n') {f->line[f->used]=0;fps_line(f,f->line);f->used=0;}
            else if(f->used<(int)sizeof(f->line)-1)f->line[f->used++]=buffer[i];
            else {f->lost=1;f->used=0;}
        }
    }
    long long now=millis(),elapsed=now-f->window;
    if(elapsed>=1000) {
        f->value=!f->conflict&&!f->lost&&f->client[0]&&fps_game_plane()?f->count*1000.0/elapsed:-1;
        f->window=now;f->count=f->conflict=f->lost=0;
    }
}
#endif
