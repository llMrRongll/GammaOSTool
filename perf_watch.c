#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

static volatile sig_atomic_t stopped;
static void stop(int sig) { (void)sig; stopped=1; }
static int game(void) {
    DIR *dir=opendir("/proc");if(!dir)return 0;
    struct dirent *entry;int found=0;
    while((entry=readdir(dir))) {
        char *end;long pid=strtol(entry->d_name,&end,10);
        if(*end||pid<=0)continue;
        char path[64],name[64];snprintf(path,sizeof(path),"/proc/%ld/comm",pid);
        FILE *f=fopen(path,"r");if(!f)continue;
        int ok=fgets(name,sizeof(name),f)!=NULL;fclose(f);
        if(ok&&!strcmp(name,"drastic-nano\n")){found=(int)pid;break;}
    }
    closedir(dir);return found;
}
static void end_child(pid_t child) {
    if(child<=0)return;
    kill(child,SIGTERM);
    /* HUD exits its 50ms loop and releases its own plane. Never SIGKILL it. */
    while(waitpid(child,NULL,0)<0&&errno==EINTR){}
}
int main(int argc,char **argv) {
    const char *base="/data/adb/gamma-perf";
    const char *hud=argc==2?argv[1]:"/data/adb/gamma-perf/perf_hud";
    char path[128];snprintf(path,sizeof(path),"%s/watch.lock",base);
    int lock=open(path,O_RDWR|O_CREAT|O_CLOEXEC,0600);
    if(lock<0||flock(lock,LOCK_EX|LOCK_NB))return 1;
    snprintf(path,sizeof(path),"%s/watch.log",base);
    int log=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);
    if(log<0)return 1;
    dup2(log,STDOUT_FILENO);dup2(log,STDERR_FILENO);setvbuf(stdout,NULL,_IONBF,0);
    signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGHUP,stop);
    snprintf(path,sizeof(path),"%s/watch.pid",base);
    FILE *pidfile=fopen(path,"w");if(!pidfile)return 1;
    fprintf(pidfile,"%ld\n",(long)getpid());fclose(pidfile);
    printf("Automatic Nano HUD watcher started\n");
    int previous=0,settle=0,attempts=0,retry=0;
    pid_t child=0;
    while(!stopped) {
        struct stat s;if(!fstat(log,&s)&&s.st_size>512*1024)ftruncate(log,0);
        int current=game();
        if(current!=previous) {
            end_child(child);child=0;
            previous=current;settle=0;attempts=0;retry=0;
            printf("Nano game PID: %d\n",current);
        }
        if(child>0) {
            int status;pid_t done=waitpid(child,&status,WNOHANG);
            if(done==child){printf("HUD exited, status=%d\n",status);child=0;retry=3;}
        }
        if(current&&settle>=2&&!child&&attempts<10&&retry==0) {
            attempts++;
            child=fork();
            if(child==0) {
                char pid[24];snprintf(pid,sizeof(pid),"%d",current);
                execl(hud,"perf_hud",pid,(char*)NULL);
                perror("exec HUD");_exit(127);
            }
            if(child<0){child=0;retry=3;}
            else printf("HUD launch %d for game %d\n",attempts,current);
        }
        if(retry>0)retry--;settle++;
        for(int i=0;i<10&&!stopped;i++)usleep(100000);
    }
    end_child(child);unlink(path);printf("Automatic watcher stopped\n");
    close(log);close(lock);return 0;
}
