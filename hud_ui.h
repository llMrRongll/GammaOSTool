#ifndef HUD_UI_H
#define HUD_UI_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#define HUD_WIDTH 640
#define HUD_HEIGHT 480
#include "hud_cjk.h"
/* Five-bit rows, seven rows per glyph. */
static const char chars[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ .:%-/";
static const uint8_t font[][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},
 {31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},
 {16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},
 {30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},
 {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {0,0,0,0,0,0,0},{0,0,0,0,0,12,12},{0,12,12,0,12,12,0},
 {17,2,4,8,17,0,0},{0,0,0,31,0,0,0},{1,2,2,4,8,8,16}
};
static void ui_rect(void *p,uint32_t pitch,int x,int y,int w,int h,uint32_t c) {
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)
        if(xx>=0&&xx<HUD_WIDTH&&yy>=0&&yy<HUD_HEIGHT)((uint32_t*)((char*)p+yy*pitch))[xx]=c;
}
static void ui_text(void *p,uint32_t pitch,int x,int y,const char *s,int scale,uint32_t color) {
    for(;*s;) {
        if((unsigned char)*s>=128) {
            unsigned code=0;int bytes=0;
            const unsigned char *u=(const unsigned char*)s;
            if((u[0]&0xf0)==0xe0&&u[1]&&u[2]){code=((u[0]&15)<<12)|((u[1]&63)<<6)|(u[2]&63);bytes=3;}
            if(!bytes){s++;continue;}
            int factor=(scale+1)/2;
            for(size_t i=0;i<sizeof(hud_cjk)/sizeof(hud_cjk[0]);i++)if(hud_cjk[i].code==code) {
                for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++) {
                    unsigned a=hud_cjk[i].alpha[yy*16+xx];if(!a)continue;
                    for(int dy=0;dy<factor;dy++)for(int dx=0;dx<factor;dx++) {
                        int px=x+xx*factor+dx,py=y+yy*factor+dy;
                        if(px<0||px>=HUD_WIDTH||py<0||py>=HUD_HEIGHT)continue;
                        uint32_t *pixel=(uint32_t*)((char*)p+py*pitch)+px,bg=*pixel,c=0xff000000;
                        for(int shift=0;shift<=16;shift+=8)c|=((((color>>shift)&255)*a+((bg>>shift)&255)*(255-a))/255)<<shift;
                        *pixel=c;
                    }
                }
                break;
            }
            x+=16*factor;s+=bytes;continue;
        }
        const char *c=strchr(chars,toupper((unsigned char)*s));if(!c){s++;x+=6*scale;continue;}
        for(int r=0;r<7;r++)for(int col=0;col<5;col++)if(font[c-chars][r]&(1<<(4-col)))
            ui_rect(p,pitch,x+col*scale,y+r*scale,scale,scale,color);
        s++;x+=6*scale;
    }
}
/* One-pixel coverage fringe blends geometric edges into the existing background. */
static void ui_blend(void *p,uint32_t pitch,int x,int y,uint32_t color,double coverage) {
    if(x<0||x>=HUD_WIDTH||y<0||y>=HUD_HEIGHT||coverage<=0)return;
    if(coverage>=1){((uint32_t*)((char*)p+y*pitch))[x]=color;return;}
    unsigned alpha=(unsigned)(coverage*255+0.5);
    uint32_t *pixel=(uint32_t*)((char*)p+y*pitch)+x,bg=*pixel,result=0xff000000;
    for(int shift=0;shift<=16;shift+=8)
        result|=((((color>>shift)&255)*alpha+((bg>>shift)&255)*(255-alpha)+127)/255)<<shift;
    *pixel=result;
}
static void ui_round(void *p,uint32_t pitch,int x,int y,int w,int h,int radius,uint32_t color) {
    if(w<=0||h<=0)return;
    double r=radius;if(r>w/2.0)r=w/2.0;if(r>h/2.0)r=h/2.0;
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++) {
        double dx=fmax(fabs(xx+0.5-w/2.0)-(w/2.0-r),0);
        double dy=fmax(fabs(yy+0.5-h/2.0)-(h/2.0-r),0);
        double coverage=(dx==0||dy==0)?1:r+0.5-sqrt(dx*dx+dy*dy);
        ui_blend(p,pitch,x+xx,y+yy,color,coverage);
    }
}
static void ui_line(void *p,uint32_t pitch,int x0,int y0,int x1,int y1,uint32_t color) {
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;) {
        ui_rect(p,pitch,x0,y0,1,1,color);if(x0==x1&&y0==y1)break;
        int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}
    }
}
/* Outline stem and filled bulb, matching Control Center's thermal icons. */
static void ui_thermometer(void *p,uint32_t pitch,int x,int y,uint32_t color) {
    ui_round(p,pitch,x-6,y-22,12,33,6,color);
    ui_round(p,pitch,x-3,y-19,6,26,3,0xff30303e);
    ui_round(p,pitch,x-10,y+2,20,20,10,color);
    ui_rect(p,pitch,x-2,y-12,4,24,color);
    for(int i=0;i<3;i++)ui_rect(p,pitch,x+8,y-16+i*8,5,2,color);
}
static void ui_ring(void *p,uint32_t pitch,int cx,int cy,double fraction,uint32_t color) {
    enum { SIDE=121 };
    static float radius[SIDE*SIDE],angle[SIDE*SIDE];static int ready;
    if(!ready) {
        for(int y=-60;y<=60;y++)for(int x=-60;x<=60;x++) {
            int i=(y+60)*SIDE+x+60;radius[i]=sqrt(x*x+y*y);
            double a=atan2(y,x)+1.570796326794897;
            if(a<0)a+=6.283185307179586;angle[i]=a;
        }
        ready=1;
    }
    if(fraction<0)fraction=0;if(fraction>1)fraction=1;
    double end=fraction*6.283185307179586;
    double ex=52*sin(end),ey=-52*cos(end);
    for(int y=-60;y<=60;y++)for(int x=-60;x<=60;x++) {
        int i=(y+60)*SIDE+x+60;
        double base=7.5-fabs(radius[i]-52);if(base<=0)continue;
        ui_blend(p,pitch,cx+x,cy+y,0xff555564,base);
        if(fraction<=0)continue;
        double coverage=0;
        if(fraction>=1||angle[i]<=end)coverage=base;
        else {
            double d1=x*x+(y+52)*(y+52),d2=(x-ex)*(x-ex)+(y-ey)*(y-ey);
            double d=fmin(d1,d2);
            if(d<56.25)coverage=7.5-sqrt(d);
        }
        ui_blend(p,pitch,cx+x,cy+y,color,coverage);
    }
}
static void ui_history(void *p,uint32_t pitch,const long *v,int count,long low,long high,uint32_t color) {
    int px=-1,py=-1;
    for(int i=0;i<count;i++) {
        if(v[i]<0){px=py=-1;continue;}
        long t=v[i];if(t<low)t=low;if(t>high)t=high;
        int x=70+(60-count+i)*536/59,y=456-(int)((t-low)*54/(high-low));
        if(px>=0)ui_line(p,pitch,px,py,x,y,color);ui_rect(p,pitch,x,y,2,2,color);px=x;py=y;
    }
}
static void ui_status(void *p,uint32_t pitch,int capacity,int charging,const char *clock) {
    uint32_t color=charging?0xffbbe963:capacity>=0&&capacity<=20?0xffffb34f:0xffc2bdce;
    ui_text(p,pitch,20,25,clock,2,0xfff3effb);
    ui_round(p,pitch,100,22,34,20,4,color);
    ui_rect(p,pitch,134,28,3,8,color);
    ui_round(p,pitch,102,24,30,16,2,charging?0xff35422b:0xff171720);
    if(capacity>=0&&capacity<=100)ui_rect(p,pitch,104,26,26*capacity/100,12,color);
    if(charging) {
        /* A contrasting backing keeps the bolt legible at every charge level. */
        ui_rect(p,pitch,111,25,12,14,0xff35422b);
        for(int row=0;row<14;row++) {
            int left,width;
            if(row<6){left=117-row/2;width=4;}
            else if(row<8){left=113;width=9;}
            else {left=117-(row-8)/2;width=3;}
            ui_rect(p,pitch,left,25+row,width,1,0xfff3effb);
        }
    }
    char text[16];
    if(capacity>=0)snprintf(text,sizeof(text),"%d%%",capacity);
    else strcpy(text,"N/A");
    ui_text(p,pitch,148,25,text,2,color);
}
static void ui_fps(void *p,uint32_t pitch,double fps) {
    char text[24];
    if(isfinite(fps)&&fps>=0)snprintf(text,sizeof(text),"FPS %.1f",fps);
    else strcpy(text,"FPS N/A");
    ui_text(p,pitch,244,25,text,2,0xff68dcf5);
}
static void hud_render(void *p,uint32_t pitch,long soc,long gpu,long cpu_freq,long gpu_freq,int load,int gpu_load,long cpu_max,long gpu_max,const char *mode,const long *soc_history,const long *gpu_history,int count,long mem_total,long mem_available,double battery_watts,int zh) {
    const uint32_t white=0xfff3effb,muted=0xffc2bdce,orange=0xffffb34f,cyan=0xff68dcf5,pink=0xfff271b6;
    ui_rect(p,pitch,0,0,HUD_WIDTH,HUD_HEIGHT,0xff171720);
    const char *display_mode=mode;
    if(zh) {
        if(!strcmp(mode,"stock"))display_mode=zh==2?"標準模式":"标准模式";
        else if(!strcmp(mode,"performance"))display_mode="高性能";
        else if(!strcmp(mode,"powersave"))display_mode=zh==2?"省電模式":"省电模式";
        else if(!strcmp(mode,"DEFAULT"))display_mode=zh==2?"預設模式":"默认模式";
        else if(!strcmp(mode,"UNKNOWN"))display_mode="未知模式";
    }
    char b[48];snprintf(b,sizeof(b),"%.36s",display_mode);
    int mode_width=0;
    for(const unsigned char *u=(const unsigned char*)b;*u;) {
        if((*u&0xf0)==0xe0&&u[1]&&u[2]){mode_width+=16;u+=3;}
        else {mode_width+=12;u++;}
    }
    int badge_width=mode_width+24,badge_x=624-badge_width;
    ui_round(p,pitch,badge_x,12,badge_width,36,12,0xff774baa);
    ui_text(p,pitch,badge_x+12,22,b,2,white);
    ui_round(p,pitch,16,64,180,236,16,0xff30303e);
    ui_round(p,pitch,208,64,416,236,16,0xff30303e);
    ui_text(p,pitch,36,84,zh?(zh==2?"溫度":"温度"):"THERMAL",2,muted);
    ui_text(p,pitch,36,116,"CPU/SOC",2,orange);
    if(soc>=0)snprintf(b,sizeof(b),"%ld",soc/1000);else strcpy(b,"N/A");
    ui_thermometer(p,pitch,44,164,orange);
    ui_text(p,pitch,70,148,b,5,white);ui_text(p,pitch,174,170,"C",2,orange);
    ui_text(p,pitch,36,218,"GPU",2,cyan);
    if(gpu>=0)snprintf(b,sizeof(b),"%ld",gpu/1000);else strcpy(b,"N/A");
    ui_thermometer(p,pitch,44,266,cyan);
    ui_text(p,pitch,70,250,b,5,white);ui_text(p,pitch,174,272,"C",2,cyan);
    ui_text(p,pitch,232,84,zh?"性能":"PERFORMANCE",2,muted);
    if(battery_watts>=0)snprintf(b,sizeof(b),"BAT %.2f W",battery_watts);
    else strcpy(b,"BAT N/A W");
    ui_text(p,pitch,430,84,b,2,orange);
    ui_ring(p,pitch,318,170,cpu_freq>=0&&cpu_max>0?cpu_freq/(double)cpu_max:0,pink);
    ui_ring(p,pitch,514,170,gpu_freq>=0&&gpu_max>0?gpu_freq/(double)gpu_max:0,cyan);
    if(cpu_freq>=0)snprintf(b,sizeof(b),"%.2f",cpu_freq/1000000.0);else strcpy(b,"N/A");
    ui_text(p,pitch,318-(int)strlen(b)*9,153,b,3,white);ui_text(p,pitch,300,190,"GHZ",2,muted);
    if(gpu_freq>=0)snprintf(b,sizeof(b),"%ld",gpu_freq/1000000);else strcpy(b,"N/A");
    ui_text(p,pitch,514-(int)strlen(b)*9,153,b,3,white);ui_text(p,pitch,496,190,"MHZ",2,muted);
    ui_text(p,pitch,300,232,"CPU",2,pink);ui_text(p,pitch,496,232,"GPU",2,cyan);
    for(int row=0;row<2;row++) {
        int value=row?gpu_load:load,y=254+row*22;
        const char *label=zh?(zh==2?"使用率":"使用率"):"USE";
        if(value>=0)snprintf(b,sizeof(b),"%s %s %d%%",row?"GPU":"CPU",label,value);
        else snprintf(b,sizeof(b),"%s %s N/A",row?"GPU":"CPU",label);
        ui_text(p,pitch,232,y,b,2,muted);
        ui_round(p,pitch,430,y+3,170,10,5,0xff555564);
        if(value>0&&value<=100)ui_round(p,pitch,430,y+3,170*value/100,10,5,row?cyan:pink);
    }
    ui_round(p,pitch,16,308,608,48,16,0xff30303e);
    ui_text(p,pitch,36,318,zh?(zh==2?"內存":"内存"):"RAM",2,0xffbbe963);
    if(mem_total>0&&mem_available>=0&&mem_available<=mem_total) {
        long used=mem_total-mem_available;
        snprintf(b,sizeof(b),"%.2f / %.2f GIB",used/1048576.0,mem_total/1048576.0);
        ui_text(p,pitch,100,318,zh?"已用":"USED",2,muted);
        ui_text(p,pitch,160,318,b,2,white);
        snprintf(b,sizeof(b),zh?"可用 %.2f GIB":"FREE %.2f GIB",mem_available/1048576.0);
        ui_text(p,pitch,420,318,b,2,muted);
        ui_round(p,pitch,36,342,568,9,4,0xff555564);
        ui_round(p,pitch,36,342,(int)(568*used/mem_total),9,4,0xffbbe963);
    }else ui_text(p,pitch,112,318,"N/A",2,white);
    ui_round(p,pitch,16,366,608,102,16,0xff292936);
    long low=1000000,high=-1;
    for(int i=0;i<count;i++) {
        long values[2]={soc_history[i],gpu_history[i]};
        for(int j=0;j<2;j++)if(values[j]>=0){if(values[j]<low)low=values[j];if(values[j]>high)high=values[j];}
    }
    if(high<0){low=30000;high=90000;}
    else {
        low=(low/1000-1)*1000;high=(high/1000+2)*1000;
        if(high-low<6000){long middle=(high+low)/2;low=(middle/1000-3)*1000;high=low+6000;}
    }
    ui_text(p,pitch,36,378,zh?(zh==2?"溫度歷史":"温度历史"):"HISTORY",2,muted);
    snprintf(b,sizeof(b),"%ld-%ldC / 60",low/1000,high/1000);
    ui_text(p,pitch,188,378,b,2,muted);
#ifndef HUD_TOUCH_TEST
    ui_text(p,pitch,464,378,"SOC",2,orange);ui_text(p,pitch,556,378,"GPU",2,cyan);
#endif
    for(int i=0;i<3;i++) {
        int y=402+i*27;
        ui_rect(p,pitch,70,y,538,1,0xff434351);
        snprintf(b,sizeof(b),"%ldC",(high-i*(high-low)/2)/1000);
        ui_text(p,pitch,32,y-3,b,1,muted);
    }
    ui_history(p,pitch,soc_history,count,low,high,orange);
    ui_history(p,pitch,gpu_history,count,low,high,cyan);

}
#ifdef HUD_TOUCH_TEST
#define ACCEL_X 408
#define ACCEL_Y 368
#define ACCEL_W 208
#define ACCEL_H 36
static void ui_accel(void *p,uint32_t pitch,int zh,int on) {
    ui_round(p,pitch,ACCEL_X,ACCEL_Y,ACCEL_W,ACCEL_H,10,on?0xff547526:0xff55406f);
    ui_text(p,pitch,ACCEL_X+12,ACCEL_Y+10,
            zh?(on?(zh==2?"遊戲加速 已開啟":"游戏加速 已开启"):
                      (zh==2?"遊戲加速 已關閉":"游戏加速 已关闭")):
               (on?"GAME SPEED ON":"GAME SPEED OFF"),2,0xfff3effb);
}
#endif
#endif
