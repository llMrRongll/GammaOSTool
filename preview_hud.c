#include "hud_ui.h"
int main(int argc,char **argv) {
    uint32_t pixels[HUD_WIDTH*HUD_HEIGHT];long soc[60],gpu[60];
    for(int i=0;i<60;i++){soc[i]=48000+i*170+(i%7)*150;gpu[i]=soc[i]-2200+(i%5)*90;}
    int locale=argc<2?1:!strcmp(argv[1],"en")?0:!strcmp(argv[1],"zh-Hant")?2:1;
    hud_render(pixels,HUD_WIDTH*4,59000,57000,1416000,600000000,54,34,1992000,800000000,"stock",soc,gpu,60,3027720,2267856,1.20,locale);
    int charging=argc<4||strcmp(argv[3],"battery");
    ui_status(pixels,HUD_WIDTH*4,20,charging,"18:30");
    ui_fps(pixels,HUD_WIDTH*4,59.8);
#ifdef HUD_TOUCH_TEST
    ui_accel(pixels,HUD_WIDTH*4,locale,argc>2&&!strcmp(argv[2],"on"));
#endif
    FILE *f=fopen("/tmp/gamma-hud-preview.ppm","wb");if(!f)return 1;
    fprintf(f,"P6\n%d %d\n255\n",HUD_WIDTH,HUD_HEIGHT);
    for(int i=0;i<HUD_WIDTH*HUD_HEIGHT;i++){unsigned char rgb[3]={pixels[i]>>16,pixels[i]>>8,pixels[i]};fwrite(rgb,1,3,f);}fclose(f);return 0;
}
