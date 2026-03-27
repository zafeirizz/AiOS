#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/string.h"

/* ── Button ────────────────────────────────────────────── *
 *  AiOS style: dark fill, teal corner-bracket border,  *
 *  amber highlight on hover, no rounded corners.           *
 * ─────────────────────────────────────────────────────── */
ui_button_t ui_button_create(int x,int y,int w,int h,const char* label){
    ui_button_t btn={x,y,w,h,{0},0,0,0};
    strncpy(btn.label,label,sizeof(btn.label)-1);
    return btn;
}

void ui_button_draw(const ui_button_t* btn){
    uint32_t bg, border, fg;
    if(btn->pressed){
        bg=RGB(0,50,45); border=COLOR_ACCENT; fg=COLOR_TEXT_HI;
    } else if(btn->hovered){
        bg=RGB(0,35,30); border=COLOR_ACCENT2; fg=COLOR_ACCENT2;
    } else {
        bg=RGB(12,16,26); border=COLOR_BORDER_DIM; fg=COLOR_TEXT_L;
    }
    gfx_rect_fill(btn->x,btn->y,btn->w,btn->h,bg);
    /* Corner brackets only — not full box */
    int bsz=4;
    gfx_corner_brackets(btn->x,btn->y,btn->w,btn->h,bsz,border);
    /* Label centred */
    int tx=btn->x+(btn->w-gfx_text_width(btn->label))/2;
    int ty=btn->y+(btn->h-GFX_CHAR_H)/2;
    gfx_text(tx,ty,btn->label,fg);
}

void ui_button_event(ui_button_t* btn,const event_t* e,int cx,int cy){
    int rx=e->mouse.x-cx, ry=e->mouse.y-cy;
    int in=rx>=btn->x&&rx<btn->x+btn->w&&ry>=btn->y&&ry<btn->y+btn->h;
    if(e->type==EVT_MOUSE_MOVE)      btn->hovered=in;
    else if(e->type==EVT_MOUSE_DOWN&&in) btn->pressed=1;
    else if(e->type==EVT_MOUSE_UP){
        if(btn->pressed&&in) btn->clicked=1;
        btn->pressed=0;
    }
}

/* ── List ──────────────────────────────────────────────── */
ui_list_t ui_list_create(int x,int y,int w,int h,int item_h){
    ui_list_t l={x,y,w,h,item_h,0,-1,0,-1,0,0};
    return l;
}
void ui_list_set_items(ui_list_t* l,const char** items,int count){
    l->items=items; l->count=count; l->selected=-1;
}
void ui_list_draw(const ui_list_t* l){
    /* Background */
    gfx_rect_fill(l->x,l->y,l->w,l->h,RGB(8,11,20));
    /* Border — corner brackets */
    gfx_corner_brackets(l->x,l->y,l->w,l->h,8,COLOR_BORDER_DIM);
    int vis=l->h/l->item_h;
    for(int i=0;i<vis&&i+l->scroll_y<l->count;i++){
        int idx=i+l->scroll_y;
        int iy=l->y+i*l->item_h;
        uint32_t bg=(idx==l->selected)?RGB(0,40,36):RGB(8,11,20);
        gfx_rect_fill(l->x+1,iy,l->w-2,l->item_h,bg);
        if(idx==l->selected){
            /* Left accent bar */
            gfx_vline(l->x+1,iy,l->item_h,COLOR_ACCENT);
        }
        uint32_t fg=(idx==l->selected)?COLOR_TEXT_HI:COLOR_TEXT_L;
        gfx_text(l->x+6,iy+2,l->items[idx],fg);
        /* Row separator — dim dashed */
        gfx_dashed_hline(l->x+6,iy+l->item_h-1,l->w-12,3,5,RGB(0,30,28));
    }
}
void ui_list_event(ui_list_t* l,const event_t* e,int cx,int cy){
    int rx=e->mouse.x-cx, ry=e->mouse.y-cy;
    int in=rx>=l->x&&rx<l->x+l->w&&ry>=l->y&&ry<l->y+l->h;
    if(e->type==EVT_MOUSE_DOWN&&in){
        int idx=(ry-l->y)/l->item_h+l->scroll_y;
        if(idx<l->count){l->selected=idx;l->clicked=1;l->clicked_idx=idx;}
    } else if(e->type==EVT_MOUSE_SCROLL&&in){
        if(e->mouse.dy>0){if(l->scroll_y>0)l->scroll_y--;}
        else{int ms=l->count>l->h/l->item_h?l->count-l->h/l->item_h:0;if(l->scroll_y<ms)l->scroll_y++;}
    }
}

/* ── Textbox ─────────────────────────────────────────── */
ui_textbox_t ui_textbox_create(int x,int y,int w,int h,char* buf,uint32_t size){
    ui_textbox_t b={x,y,w,h,buf,size,0,0};
    return b;
}
void ui_textbox_draw(const ui_textbox_t* box){
    uint32_t border=box->focused?COLOR_ACCENT:COLOR_BORDER_DIM;
    gfx_rect_fill(box->x,box->y,box->w,box->h,RGB(8,11,20));
    gfx_corner_brackets(box->x,box->y,box->w,box->h,5,border);
    gfx_text(box->x+4,box->y+4,box->buffer,COLOR_TEXT_L);
    if(box->focused){
        int cx=box->x+4+gfx_text_width(box->buffer);
        gfx_vline(cx,box->y+4,GFX_CHAR_H,COLOR_ACCENT);
    }
}
void ui_textbox_event(ui_textbox_t* box,const event_t* e){
    if(e->type==EVT_MOUSE_DOWN)
        box->focused=(e->mouse.x>=box->x&&e->mouse.x<box->x+box->w&&
                      e->mouse.y>=box->y&&e->mouse.y<box->y+box->h);
    if(!box->focused||e->type!=EVT_KEY_DOWN) return;
    char c=e->key.c;
    uint32_t len=(uint32_t)strlen(box->buffer);
    if(c=='\b'){if(len>0)box->buffer[len-1]='\0';}
    else if(c>=32&&c<127&&len<box->buf_size-1){box->buffer[len]=c;box->buffer[len+1]='\0';}
}

/* ── Textarea ─────────────────────────────────────────── */
ui_textarea_t ui_textarea_create(int x,int y,int w,int h,char* buf,uint32_t size){
    ui_textarea_t a={x,y,w,h,buf,size,0,0,0};
    return a;
}
void ui_textarea_draw(const ui_textarea_t* a){
    uint32_t border=a->focused?COLOR_ACCENT:COLOR_BORDER_DIM;
    gfx_rect_fill(a->x,a->y,a->w,a->h,RGB(8,11,20));
    gfx_corner_brackets(a->x,a->y,a->w,a->h,8,border);
    /* Render lines */
    int line_h=GFX_CHAR_H+2;
    int vis=(a->h-6)/line_h;
    const char* p=a->buffer;
    int line=0, row=0;
    while(*p&&row<a->scroll_y+vis){
        if(row>=a->scroll_y){
            int draw_y=a->y+4+(row-a->scroll_y)*line_h;
            /* Find end of line */
            const char* end=p;
            int col=0;
            while(*end&&*end!='\n'&&col<(a->w-8)/GFX_CHAR_W){end++;col++;}
            char linebuf[81];
            int l=(int)(end-p); if(l>80)l=80;
            for(int i=0;i<l;i++) linebuf[i]=p[i];
            linebuf[l]='\0';
            gfx_text(a->x+4,draw_y,linebuf,COLOR_TEXT_L);
            (void)line;
        }
        while(*p&&*p!='\n') p++;
        if(*p=='\n') p++;
        row++;
        line++;
    }
    (void)vis;
    /* Cursor */
    if(a->focused){
        /* Count lines/cols up to pos */
        int cr=0,cc=0;
        for(uint32_t i=0;i<a->pos&&a->buffer[i];i++){
            if(a->buffer[i]=='\n'){cr++;cc=0;}else cc++;
        }
        int draw_y=a->y+4+(cr-a->scroll_y)*line_h;
        int draw_x=a->x+4+cc*GFX_CHAR_W;
        if(draw_y>=a->y&&draw_y<a->y+a->h-line_h)
            gfx_vline(draw_x,draw_y,GFX_CHAR_H,COLOR_ACCENT);
    }
}
void ui_textarea_event(ui_textarea_t* a,const event_t* e){
    if(e->type==EVT_MOUSE_DOWN)
        a->focused=(e->mouse.x>=a->x&&e->mouse.x<a->x+a->w&&
                    e->mouse.y>=a->y&&e->mouse.y<a->y+a->h);
    if(!a->focused||e->type!=EVT_KEY_DOWN) return;
    char c=e->key.c;
    uint32_t len=(uint32_t)strlen(a->buffer);
    if(c=='\b'){if(a->pos>0){a->pos--;for(uint32_t i=a->pos;i<len;i++)a->buffer[i]=a->buffer[i+1];}}
    else if((c=='\n'||c>=32)&&len<a->buf_size-1){
        for(uint32_t i=len;i>a->pos;i--) a->buffer[i]=a->buffer[i-1];
        a->buffer[a->pos++]=c;
        a->buffer[len+1]='\0';
    }
}

/* ── Label ───────────────────────────────────────────── */
ui_label_t ui_label_create(int x,int y,const char* text,uint32_t color){
    ui_label_t l={x,y,{0},color};
    strncpy(l.text,text,255);
    return l;
}
void ui_label_draw(const ui_label_t* l){gfx_text(l->x,l->y,l->text,l->color);}

/* ── Separator / spacer ──────────────────────────────── */
void ui_draw_separator(int x,int y,int w,uint32_t color){
    gfx_dashed_hline(x,y,w,4,4,color);
}
void ui_draw_spacer(int* y,int h){*y+=h;}
