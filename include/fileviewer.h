#ifndef FILEVIEWER_H
#define FILEVIEWER_H

/* Launch the file viewer for read-only display of a file.
 * filename: the file to display.
 * Returns 0 on success, -1 on failure. */
int fileviewer_launch(const char* filename);

#endif