#ifndef SHELL_GUI_H
#define SHELL_GUI_H

typedef void (*shell_putchar_fn)(char c);
void shell_init_gui(shell_putchar_fn fn);
void shell_handle_input(char c);

#endif
