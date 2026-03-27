#include <stdint.h>
#include <stddef.h>
#include "../../include/html.h"
#include "../../include/heap.h"
#include "../../include/string.h"

/* Simple HTML text extractor */
char* html_extract_text(const char* html, int html_len) {
    if (!html || html_len <= 0) return NULL;

    /* Allocate buffer for extracted text (worst case: same size) */
    char* text = (char*)kmalloc(html_len + 1);
    if (!text) return NULL;

    int text_pos = 0;
    int in_tag = 0;
    int in_script = 0;
    int in_style = 0;

    for (int i = 0; i < html_len; i++) {
        char c = html[i];

        if (c == '<') {
            /* Start of tag */
            in_tag = 1;

            if (!in_script && !in_style) {
                /* Add basic newlines around common block/line-break tags */
                if ((i + 3 < html_len && strncmp(html + i, "<br", 3) == 0) ||
                    (i + 4 < html_len && strncmp(html + i, "<hr", 3) == 0) ||
                    (i + 3 < html_len && strncmp(html + i, "<p>", 3) == 0) ||
                    (i + 4 < html_len && strncmp(html + i, "<li>", 4) == 0) ||
                    (i + 4 < html_len && strncmp(html + i, "</p>", 4) == 0) ||
                    (i + 5 < html_len && strncmp(html + i, "</h1>", 5) == 0) ||
                    (i + 5 < html_len && strncmp(html + i, "</h2>", 5) == 0) ||
                    (i + 5 < html_len && strncmp(html + i, "</h3>", 5) == 0)) {
                    if (text_pos < html_len) text[text_pos++] = '\n';
                }
            }

            /* Check for script or style tags */
            if (i + 7 < html_len && strncmp(html + i, "<script", 7) == 0) {
                in_script = 1;
            } else if (i + 6 < html_len && strncmp(html + i, "<style", 6) == 0) {
                in_style = 1;
            } else if (i + 9 < html_len && strncmp(html + i, "</script>", 9) == 0) {
                in_script = 0;
            } else if (i + 8 < html_len && strncmp(html + i, "</style>", 8) == 0) {
                in_style = 0;
            }

        } else if (c == '>') {
            /* End of tag */
            in_tag = 0;
        } else if (!in_tag && !in_script && !in_style) {
            /* Regular text content */
            if (c == '&') {
                /* Handle common HTML entities */
                if (i + 4 < html_len && strncmp(html + i, "&lt;", 4) == 0) {
                    text[text_pos++] = '<';
                    i += 3;
                } else if (i + 4 < html_len && strncmp(html + i, "&gt;", 4) == 0) {
                    text[text_pos++] = '>';
                    i += 3;
                } else if (i + 5 < html_len && strncmp(html + i, "&amp;", 5) == 0) {
                    text[text_pos++] = '&';
                    i += 4;
                } else if (i + 6 < html_len && strncmp(html + i, "&nbsp;", 6) == 0) {
                    text[text_pos++] = ' ';
                    i += 5;
                } else if (i + 6 < html_len && strncmp(html + i, "&quot;", 6) == 0) {
                    text[text_pos++] = '"';
                    i += 5;
                } else {
                    text[text_pos++] = c;
                }
            } else if (c == '\r' || c == '\n' || c == '\t') {
                /* Convert whitespace to spaces */
                text[text_pos++] = ' ';
            } else if (c >= 32) {
                /* Printable character */
                text[text_pos++] = c;
            }
        }
    }

    /* Remove trailing spaces and collapse multiple spaces */
    int final_pos = 0;
    int last_was_space = 0;
    for (int i = 0; i < text_pos; i++) {
        if (text[i] == ' ') {
            if (!last_was_space) {
                text[final_pos++] = ' ';
                last_was_space = 1;
            }
        } else {
            text[final_pos++] = text[i];
            last_was_space = 0;
        }
    }

    /* Remove trailing space */
    if (final_pos > 0 && text[final_pos - 1] == ' ') {
        final_pos--;
    }

    text[final_pos] = '\0';
    return text;
}

void html_free_text(char* text) {
    if (text) {
        kfree(text);
    }
}