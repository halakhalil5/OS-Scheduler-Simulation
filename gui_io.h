// gui_io.h
#ifndef GUI_IO_H
#define GUI_IO_H

#include <glib.h>

gboolean gui_request_input(const char *prompt, char *destination_buffer, int dest_buffer_size);

void gui_log_message(const char *format, ...);

void gui_request_dashboard_refresh(void);

extern volatile gboolean please_stop_simulation;



#endif // GUI_IO_H