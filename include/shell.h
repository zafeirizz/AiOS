#ifndef SHELL_H
#define SHELL_H

extern volatile int shell_mode_request; /* 0=none, 1=tty, 2=gui */

void shell_init(void);
void shell_handle_input(char c);
void shell_run_text_mode(void);

#endif
