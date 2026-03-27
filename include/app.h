#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "event.h"
#include "wm.h"

/* Standard app interface for GUI applications
 * 
 * Every app follows this pattern:
 *   - Create window with wm_create()
 *   - Implement app_draw() and app_event() callbacks
 *   - Store app state in window->app_data
 */

/* App state pointer type for convenience */
typedef void* app_state_t;

/* App launcher: creates a window and launches the app */
typedef int (*app_launcher_fn)(void);

/* Standard app launch functions (defined per app) */
int app_fileman_launch(void);
int app_settings_launch(void);
int app_diskmgr_launch(void);

#endif
