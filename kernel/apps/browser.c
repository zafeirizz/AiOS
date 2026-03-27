#include "../../include/wm.h"
#include "../../include/ui.h"
#include "../../include/gfx.h"
#include "../../include/heap.h"
#include "../../include/string.h"
#include "../../include/ethernet.h"
#include "../../include/http.h"
#include "../../include/html.h"

/* Browser State */
typedef struct {
    ui_textbox_t url_box;
    ui_button_t btn_go;
    ui_textarea_t content;
    char url_buffer[128];
    char page_buffer[4096];
} browser_state_t;

/* Real HTTP fetcher with HTML parsing */
static void browser_fetch_url(browser_state_t* state) {
    const char* url = state->url_buffer;
    
    /* Clear content */
    state->page_buffer[0] = '\0';
    state->content.pos = 0;
    
    /* Handle special URLs */
    if (strcmp(url, "about:blank") == 0) {
        strcpy(state->page_buffer, "");
        state->content.pos = 0;
        return;
    } else if (strcmp(url, "http://aios.local") == 0) {
        strcpy(state->page_buffer, 
            "<html>\n"
            "  <h1>Welcome to AiOS Web!</h1>\n"
            "  <p>This is a simulated web page running on your custom kernel.</p>\n"
            "  <p>Network status: ONLINE</p>\n"
            "</html>"
        );
        state->content.pos = strlen(state->page_buffer);
        return;
    }
    
    /* Try to fetch real HTTP content */
    http_response_t response;
    int result = http_get(url, &response);
    
    if (result == 0) {
        /* Success - parse HTML if it's HTML content */
        if (strstr(response.content_type, "text/html")) {
            char* text = html_extract_text(response.body, response.body_len);
            if (text) {
                int text_len = strlen(text);
                if ((size_t)text_len < sizeof(state->page_buffer)) {
                    strcpy(state->page_buffer, text);
                    state->content.pos = text_len;
                } else {
                    strcpy(state->page_buffer, "Error: Page too large to display");
                    state->content.pos = strlen(state->page_buffer);
                }
                html_free_text(text);
            } else {
                strcpy(state->page_buffer, "Error: Failed to parse HTML");
                state->content.pos = strlen(state->page_buffer);
            }
        } else {
            /* Not HTML - show raw content if it's text */
            if (strstr(response.content_type, "text/")) {
                int content_len = response.body_len;
                if ((size_t)content_len < sizeof(state->page_buffer)) {
                    memcpy(state->page_buffer, response.body, content_len);
                    state->page_buffer[content_len] = '\0';
                    state->content.pos = content_len;
                } else {
                    strcpy(state->page_buffer, "Error: Content too large to display");
                    state->content.pos = strlen(state->page_buffer);
                }
            } else {
                snprintf(state->page_buffer, sizeof(state->page_buffer),
                    "HTTP %d %s\nContent-Type: %s\nContent-Length: %d\n\n(Binary content not displayed)",
                    response.status_code, response.status_text,
                    response.content_type, response.content_length);
                state->content.pos = strlen(state->page_buffer);
            }
        }
        http_free_response(&response);
    } else {
        /* HTTP request failed */
        const char* error_msg;
        switch (result) {
            case -1: error_msg = "Invalid URL format"; break;
            case -2: error_msg = "DNS resolution failed (use IP addresses for now)"; break;
            case -3: error_msg = "Failed to create TCP socket"; break;
            case -4: error_msg = "Failed to connect to server"; break;
            case -5: error_msg = "Request too long"; break;
            case -6: error_msg = "Failed to send request"; break;
            default: error_msg = "Unknown HTTP error"; break;
        }
        snprintf(state->page_buffer, sizeof(state->page_buffer),
            "Error loading %s\n%s", url, error_msg);
        state->content.pos = strlen(state->page_buffer);
    }
}

static void browser_draw(window_t* win, int cx, int cy, int cw, int ch) {
    browser_state_t* state = (browser_state_t*)win->app_data;
    if (!state) return;
    
    /* Draw background */
    gfx_rect_fill(cx, cy, cw, ch, COLOR_WINBG);
    
    /* Draw Address Bar area */
    gfx_rect_fill(cx, cy, cw, 30, RGB(220, 220, 220));
    gfx_hline(cx, cy + 30, cw, COLOR_BORDER);
    
    /* Draw URL Box */
    ui_textbox_t box = state->url_box;
    box.x += cx; box.y += cy;
    ui_textbox_draw(&box);
    
    /* Draw Go Button */
    ui_button_t btn = state->btn_go;
    btn.x += cx; btn.y += cy;
    ui_button_draw(&btn);
    
    /* Draw Content Area */
    ui_textarea_t content = state->content;
    content.x += cx; content.y += cy;
    ui_textarea_draw(&content);
}

static void browser_event(window_t* win, const event_t* e) {
    browser_state_t* state = (browser_state_t*)win->app_data;
    if (!state) return;
    
    /* Events from WM are already relative to the window client area,
       so we pass 0,0 as offsets to UI components. */
    ui_textbox_event(&state->url_box, e);
    ui_button_event(&state->btn_go, e, 0, 0);
    ui_textarea_event(&state->content, e);
    
    if (state->btn_go.clicked) {
        state->btn_go.clicked = 0;
        browser_fetch_url(state);
    }
    
    /* Allow Enter key in URL box to trigger Go */
    if (e->type == EVT_KEY_DOWN && e->key.c == '\n' && state->url_box.focused) {
        browser_fetch_url(state);
    }
}

void browser_launch(void) {
    browser_state_t* state = (browser_state_t*)kmalloc(sizeof(browser_state_t));
    if (!state) return;
    
    memset(state, 0, sizeof(browser_state_t));
    strcpy(state->url_buffer, "http://aios.local");
    state->url_box = ui_textbox_create(5, 5, 300, 20, state->url_buffer, 128);
    state->btn_go = ui_button_create(310, 5, 40, 20, "Go");
    
    state->content = ui_textarea_create(5, 35, 390, 260, state->page_buffer, 4096);
    state->content.h = 260; 
    
    browser_fetch_url(state); // Load default
    
    window_t* win = wm_create("Web Browser", 50, 50, 400, 300, browser_draw, browser_event);
    if (win) win->app_data = state;
}