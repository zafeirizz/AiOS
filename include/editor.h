#ifndef EDITOR_H
#define EDITOR_H

/* Launch the full-screen text editor.
 * filename: NULL = new unnamed buffer, otherwise load from FS.
 * Returns when the user exits (Ctrl+Q). */
void editor_open(const char* filename);

#endif
