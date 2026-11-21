#include "cat3ds_menu.h"
#include "cat3ds_dualscreen.h"
#include <3ds.h>
#include <stdint.h>
#include <string.h>

extern const uint8_t g_cga_8x8TextFont[256*64];
void BE_ST_PollEvents();
int cat3dsSessionAction = CatResume;

static void line(uint16_t *fb, int stride, int x, int y, const char *s, uint16_t color)
{
    for (; *s && x+8<=400; ++s,x+=8) {
        const uint8_t *glyph=g_cga_8x8TextFont+(unsigned char)*s*64;
        for (int v=0;v<8;++v) for (int u=0;u<8;++u)
            if (glyph[v*8+u]) fb[(x+u)*stride+stride-1-y-v]=color;
    }
}

static void draw(const Cat3DSMenu &m)
{
    u16 stride, columns;
    auto *fb=(uint16_t*)gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&stride,&columns);
    if (!fb || stride<240 || columns<400) return;
    for (int x=0;x<400;++x) for (int y=0;y<240;++y)
        fb[x*stride+y]=(x>10 && x<389 && y>10 && y<229) ? 0x000a : 0;
    auto label=[&](int y,const char *s,bool selected=false){
        if (selected) line(fb,stride,22,y,">",0xffe0);
        line(fb,stride,38,y,s,selected ? 0xffe0 : 0xffff);
    };
    const char *games[]={"CATACOMB 3-D","CATACOMB ABYSS","CATACOMB ARMAGEDDON","CATACOMB APOCALYPSE"};
    line(fb,stride,24,22,games[m.game],0x07ff);
    if (m.page==CatRoot) {
        const char *items[]={"Continuar partida","Guardar partida","Cargar partida","Controles","Ayuda original","Opciones","Cambiar de juego","Salir de la aplicacion"};
        for (int i=m.dead?2:0;i<8;++i) {
            if (m.title && i == CatSave) continue;
            label(48+i*18,m.title && i == CatResume ? "Iniciar partida" : items[i],i==m.row);
        }
    } else if (m.page==CatSettings) {
        const char *sounds[]={"Sonido: apagado","Sonido: PC speaker","Sonido: AdLib"};
        label(56,sounds[m.sound],m.subrow==0);
        if (m.game==0) {
            label(80,m.music?"Musica: AdLib":"Musica: apagada",m.subrow==1);
            label(104,"Opciones y extras originales",m.subrow==2);
        }
        label(m.game==0?128:80,"Nueva partida",m.subrow==(m.game==0?3:1));
    } else if (m.page==CatGuide && !m.guide) {
        label(48,"Controles 1/2");
        label(72,"Circle Pad/cruceta/C-Stick: mover y girar");
        label(94,"B o L + izquierda/derecha: lateral");
        label(116,"A disparar    X beber pocion");
        label(138,m.game ? "Y Zapper    ZL Xterminator" : "Y lanzar bolt    ZL lanzar nuke");
        label(160,m.game ? "R: giro rapido" : "R: correr y girar rapido");
        label(182,"Cruceta derecha: pagina siguiente");
    } else if (m.page==CatGuide) {
        label(48,"Controles 2/2");
        if (m.game<2) {
            label(70,"ZR: pergaminos. Arriba 1, derecha 2,");
            label(86,"abajo 3, izquierda 4. Y5 B6 A7 X8.");
        } else {
            label(70,"ZR: arriba ayuda, derecha sonido,");
            label(86,"abajo guardar, izquierda cargar.");
        }
        label(112,"START menu    SELECT pausa/reanuda");
        label(134,"Cruceta elige. A acepta. B vuelve.");
        label(156,"Guardar y cargar: seis ranuras.");
        label(178,"Cambiar de juego: START > Cambiar.");
    } else if (m.page==CatConfirm) {
        if (!m.title) label(80,"Se perdera el progreso sin guardar.");
        label(112,m.subrow==CatNew?"Iniciar una nueva partida?":m.row==CatSwitch?"Volver al selector?":"Salir de Catacomb 3DS?");
    } else if (m.page==CatPause) label(96,"En pausa. SELECT para reanudar.");
    else label(96,"Esta edicion no incluye ayuda interna.");
    line(fb,stride,24,209,"A Aceptar    B Volver    START Cerrar",0x07ff);
    GSPGPU_FlushDataCache(fb,stride*400*2);
    gfxScreenSwapBuffers(GFX_TOP,false);
}

int Cat3DS_OpenMenu(Cat3DSMenu &m)
{
    Cat3DS_DualScreenEnterMenu();
    const u32 buttons[]={KEY_DUP,KEY_DDOWN,KEY_DLEFT,KEY_DRIGHT,KEY_A,KEY_B,KEY_START,KEY_SELECT};
    bool armed=false, dirty=true;
    int result=-1;
    while (aptMainLoop()) {
        BE_ST_PollEvents();
        // SDL's N3DS event pump scans HID before dispatching controller events.
        u32 held=hidKeysHeld();
        if (!held) {
            if (result>=0) break;
            armed=true;
        } else if (armed && result<0) {
            for (int i=0;i<8;++i) if (held & buttons[i]) {
                result=m.step((Cat3DSButton)i);
                dirty=true; break;
            }
            armed=false;
        }
        if (dirty) { draw(m); dirty=false; }
        gspWaitForVBlank();
    }
    Cat3DS_DualScreenLeaveMenu();
    return result<0 ? CatExit : result;
}

void Cat3DS_WaitForPadRelease()
{
    while (aptMainLoop()) {
        BE_ST_PollEvents();
        if (!hidKeysHeld()) return;
        gspWaitForVBlank();
    }
}
