#include <stdint.h>
#include <string.h>
#include "../../include/calculator.h"
#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/heap.h"
#include "../../include/string.h"

/* ── Calculator State ──────────────────────────────────── */
typedef struct {
    double  accumulator;   /* stored first operand      */
    double  display_val;   /* current display number    */
    char    op;            /* pending op: +,-,*,/,%     */
    int     fresh;         /* next keypress starts new  */
    int     error;
    char    disp_str[24];  /* rendered string           */

    /* 4x5 button grid */
    ui_button_t btns[20];
} calc_state_t;

/* Button layout (label, col, row) */
static const char* btn_labels[20] = {
    "C",  "+/-", "%",  "/",
    "7",  "8",   "9",  "*",
    "4",  "5",   "6",  "-",
    "1",  "2",   "3",  "+",
    "0",  ".",   "<-", "="
};

/* ── Simple double helpers (no libm) ──────────────────── */
static void double_to_str(double v, char* out, int maxlen) {
    if (v != v) { /* NaN */ strcpy(out, "Error"); return; }
    int neg = (v < 0);
    if (neg) v = -v;
    /* Integer part */
    long long iv = (long long)v;
    double frac = v - (double)iv;
    /* Build integer string */
    char ibuf[20]; int ii=0;
    if (iv==0) ibuf[ii++]='0';
    else { long long tmp=iv; char r[20]; int ri=0; while(tmp>0){r[ri++]='0'+(int)(tmp%10);tmp/=10;} while(ri-->0)ibuf[ii++]=r[ri]; }
    /* Fraction: up to 6 digits, strip trailing zeros */
    char fbuf[10]; int fi=0;
    if (frac > 0.0000001) {
        for (int i=0;i<6;i++){
            frac *= 10;
            int d=(int)frac;
            fbuf[fi++]='0'+d;
            frac -= d;
        }
        fbuf[fi]='\0';
        /* Strip trailing zeros */
        while(fi>1 && fbuf[fi-1]=='0') fi--;
        fbuf[fi]='\0';
    }
    /* Compose */
    int pos=0;
    if (neg && pos<maxlen-1) out[pos++]='-';
    for (int i=0;i<ii&&pos<maxlen-1;i++) out[pos++]=ibuf[i];
    if (fi>0&&pos<maxlen-1) {
        out[pos++]='.';
        for (int i=0;i<fi&&pos<maxlen-1;i++) out[pos++]=fbuf[i];
    }
    out[pos]='\0';
}

static double str_to_double(const char* s) {
    if (!s[0]) return 0.0;
    int neg=0;
    if (*s=='-'){neg=1;s++;}
    double v=0;
    while (*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;}
    if (*s=='.'){s++;double f=0.1;while(*s>='0'&&*s<='9'){v+=(*s-'0')*f;f*=0.1;s++;}}
    return neg?-v:v;
}

static void calc_update_display(calc_state_t* s) {
    if (s->error) { strcpy(s->disp_str, "Error"); return; }
    double_to_str(s->display_val, s->disp_str, 22);
}

static void calc_press(calc_state_t* s, const char* label) {
    if (s->error && strcmp(label,"C")!=0) return;

    /* Digit or decimal */
    if ((label[0]>='0'&&label[0]<='9') || label[0]=='.') {
        if (s->fresh) {
            s->disp_str[0]='\0'; s->fresh=0;
        }
        int len=(int)strlen(s->disp_str);
        if (label[0]=='.' && strchr(s->disp_str,'.')) return; /* already has dot */
        if (len<21) {
            s->disp_str[len]=label[0];
            s->disp_str[len+1]='\0';
        }
        s->display_val = str_to_double(s->disp_str);
        return;
    }

    if (strcmp(label,"<-")==0) {
        /* Backspace */
        int l=(int)strlen(s->disp_str);
        if (l>0){s->disp_str[l-1]='\0'; s->display_val=str_to_double(s->disp_str);}
        return;
    }

    if (strcmp(label,"C")==0) {
        s->accumulator=0; s->display_val=0; s->op=0;
        s->fresh=0; s->error=0;
        strcpy(s->disp_str,"0"); return;
    }

    if (strcmp(label,"+/-")==0) {
        s->display_val = -s->display_val;
        calc_update_display(s); return;
    }

    if (strcmp(label,"%")==0) {
        s->display_val /= 100.0;
        calc_update_display(s); return;
    }

    /* Operators: +  -  *  / */
    if (label[0]=='+'||label[0]=='-'||label[0]=='*'||label[0]=='/') {
        /* Commit pending op first */
        if (s->op && !s->fresh) {
            double a=s->accumulator, b=s->display_val;
            switch(s->op){
                case '+': s->display_val=a+b; break;
                case '-': s->display_val=a-b; break;
                case '*': s->display_val=a*b; break;
                case '/':
                    if(b==0.0){s->error=1;strcpy(s->disp_str,"Error");return;}
                    s->display_val=a/b; break;
            }
            calc_update_display(s);
        }
        s->accumulator=s->display_val;
        s->op=label[0]; s->fresh=1; return;
    }

    if (strcmp(label,"=")==0) {
        if (s->op) {
            double a=s->accumulator, b=s->display_val;
            switch(s->op){
                case '+': s->display_val=a+b; break;
                case '-': s->display_val=a-b; break;
                case '*': s->display_val=a*b; break;
                case '/':
                    if(b==0.0){s->error=1;strcpy(s->disp_str,"Error");return;}
                    s->display_val=a/b; break;
            }
            s->op=0; s->fresh=1;
            calc_update_display(s);
        }
        return;
    }
}

/* ── Draw ─────────────────────────────────────────────── */
#define BTN_W 48
#define BTN_H 32
#define BTN_PAD 4
#define DISP_H 42

static void calc_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)cw; (void)ch;
    calc_state_t* s = (calc_state_t*)win->app_data;
    if (!s) return;

    gfx_rect_fill(cx, cy, cw, ch, COLOR_DGRAY);

    /* Display area */
    gfx_rect_fill(cx+4, cy+4, cw-8, DISP_H, COLOR_BLACK);
    gfx_rect(cx+4, cy+4, cw-8, DISP_H, COLOR_BORDER);
    const char* dstr = s->disp_str[0] ? s->disp_str : "0";
    int dw = gfx_text_width(dstr);
    gfx_text(cx+cw-12-dw, cy+4+(DISP_H-GFX_CHAR_H)/2, dstr,
             s->error ? COLOR_RED : COLOR_GREEN);

    /* Pending op indicator */
    if (s->op) {
        char opstr[3] = {s->op, '\0', '\0'};
        gfx_text(cx+8, cy+8, opstr, COLOR_YELLOW);
    }

    /* Buttons */
    for (int i=0;i<20;i++){
        ui_button_t b = s->btns[i];
        b.x += cx; b.y += cy;
        /* Color coding */
        uint32_t bg;
        const char* lbl = btn_labels[i];
        if (strcmp(lbl,"=")==0) bg=COLOR_ACCENT;
        else if (lbl[0]=='/'||lbl[0]=='*'||lbl[0]=='-'||lbl[0]=='+') bg=RGB(80,80,100);
        else if (strcmp(lbl,"C")==0||strcmp(lbl,"+/-")==0||strcmp(lbl,"%")==0) bg=RGB(90,90,90);
        else bg=RGB(60,60,65);
        gfx_rect_fill(b.x, b.y, b.w, b.h, bg);
        gfx_rect(b.x, b.y, b.w, b.h, COLOR_BORDER);
        int tx=b.x+(b.w-gfx_text_width(lbl))/2;
        int ty=b.y+(b.h-GFX_CHAR_H)/2;
        gfx_text(tx, ty, lbl, COLOR_TEXT_D);
    }
}

/* ── Event ────────────────────────────────────────────── */
static void calc_event(window_t* win, const event_t* e) {
    calc_state_t* s = (calc_state_t*)win->app_data;
    if (!s) return;

    /* Keyboard shortcuts */
    if (e->type == EVT_KEY_DOWN) {
        char c = e->key.c;
        if (c>='0'&&c<='9'){char d[2]={c,0}; calc_press(s,d); wm_invalidate(win); return;}
        if (c=='+'){calc_press(s,"+"); wm_invalidate(win); return;}
        if (c=='-'){calc_press(s,"-"); wm_invalidate(win); return;}
        if (c=='*'){calc_press(s,"*"); wm_invalidate(win); return;}
        if (c=='/'){calc_press(s,"/"); wm_invalidate(win); return;}
        if (c=='\n'||c=='='){calc_press(s,"="); wm_invalidate(win); return;}
        if (c=='\b'){calc_press(s,"<-"); wm_invalidate(win); return;}
        if (c=='.'||c==','){calc_press(s,"."); wm_invalidate(win); return;}
        if (c=='c'||c=='C'){calc_press(s,"C"); wm_invalidate(win); return;}
    }

    for (int i=0;i<20;i++){
        ui_button_event(&s->btns[i], e, 0, 0);
        if (s->btns[i].clicked){
            s->btns[i].clicked=0;
            calc_press(s, btn_labels[i]);
            wm_invalidate(win);
        }
    }
}

/* ── Launch ───────────────────────────────────────────── */
void calculator_launch(void) {
    calc_state_t* s = (calc_state_t*)kmalloc(sizeof(calc_state_t));
    if (!s) return;
    memset(s, 0, sizeof(calc_state_t));
    strcpy(s->disp_str, "0");

    /* Create buttons in 4-column grid */
    int cols=4, start_y=DISP_H+12;
    for (int i=0;i<20;i++){
        int col=i%cols, row=i/cols;
        int bx=4+col*(BTN_W+BTN_PAD);
        int by=start_y+row*(BTN_H+BTN_PAD);
        s->btns[i]=ui_button_create(bx,by,BTN_W,BTN_H,btn_labels[i]);
    }

    int win_w = 4*BTN_W + 3*BTN_PAD + 8;
    int win_h = DISP_H+12 + 5*(BTN_H+BTN_PAD) + 4;

    window_t* win = wm_create("Calculator", 400, 200, win_w, win_h,
                              calc_draw, calc_event);
    if (win) win->app_data = s;
}
