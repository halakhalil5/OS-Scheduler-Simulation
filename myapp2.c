// myapp2.c

#include <gtk/gtk.h>
#include <stdlib.h>  // For atoi
#include <stdio.h>   // For printf/g_print debugging, perror
#include <string.h>  // For strncpy, strcmp, memset
#include <pthread.h> // For threading
#include <stdarg.h>  // For gui_log_message's va_list
#include <time.h>    // For clock_gettime in timed wait
#include <ctype.h>   // For isdigit (if you parse numbers from input)

#include "testm22.h" // Your backend header
#include "gui_io.h"  // Include the new header for GUI interaction functions

volatile gboolean please_stop_simulation = FALSE;

// --- Static Globals for Widget Access ---
static GtkWidget *status_label;
static GtkWidget *algo_combo;
static GtkWidget *quantum_entry;
static GtkWidget *quantum_box;
static GtkWidget *start_button;
static GtkWidget *step_button;
static GtkWidget *execute_step;
static GtkWidget *auto_button;
static GtkWidget *stop_button;
static GtkWidget *reset_button;
static GtkWidget *overview_num_processes_label;
static GtkWidget *overview_clock_cycle_label;
static GtkWidget *overview_active_algo_label;
static GtkWidget *process_list_treeview;
static GtkListStore *process_list_store;
static GtkWidget *ready_queue_label;
static GtkWidget *running_process_label;
static GtkWidget *blocked_queue_label;
static GtkWidget *output_textview;
static GtkTextBuffer *output_textbuffer;
static GtkWidget *arrival_entry_p1;
static GtkWidget *arrival_entry_p2;
static GtkWidget *arrival_entry_p3;
// static GtkWidget *add_process_button; // Original line, moved to be with other new widgets
static GtkWindow *main_app_window = NULL;

// Widgets for integrated input area
static GtkWidget *input_area_box;
static GtkWidget *input_prompt_label;
static GtkWidget *input_entry_field;
static GtkWidget *input_submit_button;

// NEW: Widgets for Memory Viewer
static GtkWidget *memory_view_textview;
static GtkTextBuffer *memory_view_textbuffer;
static GtkWidget *resource_input_status_label;
static GtkWidget *resource_output_status_label;
static GtkWidget *resource_file_status_label;

// NEW: Widget for Add Process feature
// static GtkWidget *add_process_file_button; 
static GtkWidget *add_process_file1; 
static GtkWidget *add_process_file2; 
static GtkWidget *add_process_file3; 

// --- Simulation State & Threading ---
static gboolean simulation_running = FALSE;
static pthread_t simulation_thread_id;


// --- ENUMS for GtkTreeView Columns (Process List) ---
enum {
    COL_PROCESS_ID = 0, COL_PROCESS_STATE, COL_PROCESS_PRIORITY,
    COL_PROCESS_MEM_LOWER, COL_PROCESS_MEM_UPPER, COL_PROCESS_PC,
    NUM_PROCESS_COLS
};

// --- Data Structure for Passing Log Messages ---
typedef struct { char *message; } LogMessage;

// --- Function to Append Log Message (GUI Thread) ---
static gboolean append_log_message_idle(gpointer user_data) {
    LogMessage *msg_data = (LogMessage *)user_data;
    GtkTextIter end_iter;
    if (!output_textbuffer || !msg_data || !msg_data->message) {
        if(msg_data) g_free(msg_data->message);
        g_free(msg_data);
        return G_SOURCE_REMOVE;
    }
    gtk_text_buffer_get_end_iter(output_textbuffer, &end_iter);
    gtk_text_buffer_insert(output_textbuffer, &end_iter, msg_data->message, -1);
    gtk_text_buffer_insert(output_textbuffer, &end_iter, "\n", -1);
    GtkTextMark *mark = gtk_text_buffer_get_insert(output_textbuffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(output_textview), mark, 0.0, TRUE, 0.0, 1.0);
    g_free(msg_data->message);
    g_free(msg_data);
    return G_SOURCE_REMOVE;
}

// --- Global function to log messages to GUI (called from any thread) ---
void gui_log_message(const char *format, ...) {
    if (!output_textbuffer) return;
    va_list args;
    char *buffer;
    LogMessage *msg_data;
    va_start(args, format);
    buffer = g_strdup_vprintf(format, args);
    va_end(args);
    if (buffer) {
        msg_data = g_new(LogMessage, 1);
        msg_data->message = buffer;
        g_main_context_invoke(NULL, append_log_message_idle, msg_data);
    }
}

// --- Function to update the memory view display ---
static void update_memory_view() {
    if (!memory_view_textbuffer) return;

    // Call the backend function to get the formatted memory string
    char *formatted_memory_string = get_formatted_memory_string_for_gui(); // From testm22.c

    if (formatted_memory_string) {
        gtk_text_buffer_set_text(memory_view_textbuffer, formatted_memory_string, -1);
        // IMPORTANT: Free the string that was allocated by the backend
        g_free(formatted_memory_string); // Use g_free because the backend used GString
    } else {
        // Handle case where backend might return NULL (e.g., if GString allocation failed)
        gtk_text_buffer_set_text(memory_view_textbuffer, "Error: Could not retrieve memory data from backend.", -1);
    }
}

static void update_resource_management_view() {
    char status_buffer[256];
    GString *waiting_pids_str;

    if (resource_input_status_label) {
        waiting_pids_str = g_string_new("");
        for (int i = 0; i < blocking_queue1.size; ++i) {
            Process *p = blocking_queue1.queue[(blocking_queue1.front + i) % MAX_PROCESSES];
            if (p) g_string_append_printf(waiting_pids_str, "P%d ", p->pid);
        }

        if (semUserInput.holder_pid > 0) {
             snprintf(status_buffer, sizeof(status_buffer), "User Input: Held by P%d%s%s",
                      semUserInput.holder_pid,
                      (waiting_pids_str->len > 0) ? " (Waiting: " : "",
                      waiting_pids_str->str);
             if (waiting_pids_str->len > 0) strcat(status_buffer, ")");
        } else {
            snprintf(status_buffer, sizeof(status_buffer), "User Input: Free (Val: %d)%s%s",
                     semUserInput.value,
                     (waiting_pids_str->len > 0) ? " (Waiting: " : "",
                     waiting_pids_str->str);
            if (waiting_pids_str->len > 0) strcat(status_buffer, ")");
        }
        gtk_label_set_text(GTK_LABEL(resource_input_status_label), status_buffer);
        g_string_free(waiting_pids_str, TRUE);
    }

     if (resource_output_status_label) {
        waiting_pids_str = g_string_new("");
        for (int i = 0; i < blocking_queue2.size; ++i) {
            Process *p = blocking_queue2.queue[(blocking_queue2.front + i) % MAX_PROCESSES];
            if (p) g_string_append_printf(waiting_pids_str, "P%d ", p->pid);
        }

        if (semUserOutput.holder_pid > 0) {
             snprintf(status_buffer, sizeof(status_buffer), "User Output: Held by P%d%s%s",
                      semUserOutput.holder_pid,
                      (waiting_pids_str->len > 0) ? " (Waiting: " : "",
                      waiting_pids_str->str);
             if (waiting_pids_str->len > 0) strcat(status_buffer, ")");
        } else {
            snprintf(status_buffer, sizeof(status_buffer), "User Output: Free (Val: %d)%s%s",
                     semUserOutput.value,
                     (waiting_pids_str->len > 0) ? " (Waiting: " : "",
                     waiting_pids_str->str);
             if (waiting_pids_str->len > 0) strcat(status_buffer, ")");
        }
        gtk_label_set_text(GTK_LABEL(resource_output_status_label), status_buffer);
        g_string_free(waiting_pids_str, TRUE);
    }

     if (resource_file_status_label) {
        waiting_pids_str = g_string_new("");
        for (int i = 0; i < blocking_queue3.size; ++i) {
            Process *p = blocking_queue3.queue[(blocking_queue3.front + i) % MAX_PROCESSES];
            if (p) g_string_append_printf(waiting_pids_str, "P%d ", p->pid);
        }

        if (semFile.holder_pid > 0) {
             snprintf(status_buffer, sizeof(status_buffer), "File Access: Held by P%d%s%s",
                      semFile.holder_pid,
                      (waiting_pids_str->len > 0) ? " (Waiting: " : "",
                      waiting_pids_str->str);
            if (waiting_pids_str->len > 0) strcat(status_buffer, ")");
        } else {
            snprintf(status_buffer, sizeof(status_buffer), "File Access: Free (Val: %d)%s%s",
                     semFile.value,
                     (waiting_pids_str->len > 0) ? " (Waiting: " : "",
                     waiting_pids_str->str);
             if (waiting_pids_str->len > 0) strcat(status_buffer, ")");
        }
        gtk_label_set_text(GTK_LABEL(resource_file_status_label), status_buffer);
        g_string_free(waiting_pids_str, TRUE);
    }
}


// --- Dashboard Update Functions ---
static void update_dashboard_overview() {
    char buffer[100];
    int active_processes = 0;
    
    // Count active processes based on pcbs array directly
    int actual_process_count = 0;
    for(int i=0; i < MAX_PROCESSES; ++i) { // Iterate through all possible slots
        if(pcbs[i] != NULL) {
            actual_process_count++; // Count any non-NULL PCB as an existing process
            if(pcbs[i]->State != TERMINATED && pcbs[i]->State != NEW) {
                active_processes++;
            }
        }
    }
    // Update the label for total *defined* processes, not just active
    // snprintf(buffer, sizeof(buffer), "Total Defined Processes: %d", actual_process_count);
    // Or stick to "active" if that's more meaningful for the user
    snprintf(buffer, sizeof(buffer), "Total Active Processes: %d", active_processes);

    gtk_label_set_text(GTK_LABEL(overview_num_processes_label), buffer);
    snprintf(buffer, sizeof(buffer), "Clock Cycle: %d", cycle);
    gtk_label_set_text(GTK_LABEL(overview_clock_cycle_label), buffer);
    gchar *selected_algo_str = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(algo_combo));
    if (selected_algo_str) {
        snprintf(buffer, sizeof(buffer), "Active Algorithm: %s", selected_algo_str);
        gtk_label_set_text(GTK_LABEL(overview_active_algo_label), buffer);
        g_free(selected_algo_str);
    } else {
        gtk_label_set_text(GTK_LABEL(overview_active_algo_label), "Active Algorithm: None");
    }
}

static void update_process_list_view() {
    gtk_list_store_clear(process_list_store);
    GtkTreeIter iter;
    for (int i = 0; i < MAX_PROCESSES; ++i) { // Iterate up to MAX_PROCESSES
        if (pcbs[i] != NULL) { // Check if PCB exists at this slot
            gtk_list_store_append(process_list_store, &iter);
            gtk_list_store_set(process_list_store, &iter,
                               COL_PROCESS_ID, pcbs[i]->Pid,
                               COL_PROCESS_STATE, process_state_to_string(pcbs[i]->State),
                               COL_PROCESS_PRIORITY, pcbs[i]->current_priority,
                               COL_PROCESS_MEM_LOWER, pcbs[i]->memory_lower_bound,
                               COL_PROCESS_MEM_UPPER, pcbs[i]->memory_upper_bound,
                               COL_PROCESS_PC, pcbs[i]->PC,
                               -1);
        }
    }
}


static void append_queue_info(GString *str, const char *queue_name, const Queue *q) {
    g_string_append_printf(str, "%s: ", queue_name);
    if (q->size == 0) {
        g_string_append(str, "Empty. ");
    } else {
        for (int i = 0; i < q->size; ++i) {
            Process *p = q->queue[(q->front + i) % MAX_PROCESSES]; // MAX_PROCESSES is used as queue capacity
            if (p && p->pid > 0 && p->pid <= MAX_PROCESSES && pcbs[p->pid - 1] != NULL) {
                PCB *pcb = pcbs[p->pid - 1];
                int time_in_q = cycle - p->time_entered_queue; 

                const char *instr_preview = "N/A";
                if (pcb->PC >= pcb->memory_lower_bound && pcb->PC < pcb->memory_upper_bound &&
                    Memory[pcb->PC].Value != NULL && strlen(Memory[pcb->PC].Value) > 0) { // check strlen
                    instr_preview = Memory[pcb->PC].Value;
                }
                char display_instr[21]; 
                strncpy(display_instr, instr_preview, 20);
                display_instr[20] = '\0';
                if (strlen(instr_preview) > 20) {
                    strcat(display_instr, "...");
                }
                g_string_append_printf(str, "P%d(TinQ:%d, PC:%d, Ins:\"%s\") ",
                                       p->pid, time_in_q, pcb->PC, display_instr);
            } else if (p) {
                g_string_append_printf(str, "P%d(No PCB/Invalid) ", p->pid);
            }
        }
    }
    g_string_append(str, "\n"); 
}

static void update_queues_display() {
    GString *rq_str = g_string_new("");
    append_queue_info(rq_str, "ReadyQ", &ready_queue); 
    // Add other ready queues for MLFQ if they exist and are used
    // append_queue_info(rq_str, "ReadyQ2 (MLFQ)", &ready_queue2);
    // append_queue_info(rq_str, "ReadyQ3 (MLFQ)", &ready_queue3);
    // append_queue_info(rq_str, "ReadyQ4 (MLFQ)", &ready_queue4);
    gtk_label_set_text(GTK_LABEL(ready_queue_label), rq_str->str);
    gtk_label_set_justify(GTK_LABEL(ready_queue_label), GTK_JUSTIFY_LEFT); 
    gtk_label_set_wrap(GTK_LABEL(ready_queue_label), TRUE); 
    gtk_label_set_wrap_mode(GTK_LABEL(ready_queue_label), PANGO_WRAP_WORD_CHAR);
    g_string_free(rq_str, TRUE);

    char running_p_text[512] = "Running Process: "; 
    if (running_queue.size > 0 && running_queue.queue[running_queue.front] != NULL) {
       Process *p_run = running_queue.queue[running_queue.front];
       if (p_run->pid > 0 && p_run->pid <= MAX_PROCESSES && pcbs[p_run->pid - 1] != NULL) {
           PCB *pcb_run = pcbs[p_run->pid - 1];
           int time_as_running = cycle - p_run->time_entered_queue;
           const char *instr_str = "N/A";

           if (pcb_run->PC >= pcb_run->memory_lower_bound && pcb_run->PC < pcb_run->memory_upper_bound &&
               Memory[pcb_run->PC].Value != NULL && strlen(Memory[pcb_run->PC].Value) > 0) { // check strlen
               instr_str = Memory[pcb_run->PC].Value;
           }
           char display_instr_long[51]; 
           strncpy(display_instr_long, instr_str, 50);
           display_instr_long[50] = '\0';
           if (strlen(instr_str) > 50) {
               strcat(display_instr_long, "...");
           }

           snprintf(running_p_text + strlen(running_p_text), sizeof(running_p_text) - strlen(running_p_text),
                    "P%d (State: RUNNING, PC:%d, TimeInState:%d)\n  Instruction: \"%s\"", // Changed to TimeInState
                    p_run->pid, pcb_run->PC, time_as_running, display_instr_long);
       } else {
            strcat(running_p_text, "P? (PCB missing or invalid PID)");
       }
    } else {
        strcat(running_p_text, "None");
    }
    gtk_label_set_text(GTK_LABEL(running_process_label), running_p_text);
    gtk_label_set_justify(GTK_LABEL(running_process_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_wrap(GTK_LABEL(running_process_label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(running_process_label), PANGO_WRAP_WORD_CHAR);

    GString *bq_str = g_string_new("");
    append_queue_info(bq_str, "BlockedQ1(UserIn)", &blocking_queue1);
    append_queue_info(bq_str, "BlockedQ2(UserOut)", &blocking_queue2); 
    append_queue_info(bq_str, "BlockedQ3(File)", &blocking_queue3); 
    gtk_label_set_text(GTK_LABEL(blocked_queue_label), bq_str->str);
    gtk_label_set_justify(GTK_LABEL(blocked_queue_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_wrap(GTK_LABEL(blocked_queue_label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(blocked_queue_label), PANGO_WRAP_WORD_CHAR);
    g_string_free(bq_str, TRUE);
}

static void refresh_all_dashboard_views() {
    update_dashboard_overview();
    update_process_list_view();
    update_queues_display();
    update_memory_view(); 
    update_resource_management_view();
}

static gboolean refresh_all_dashboard_views_idle(gpointer user_data) {
    refresh_all_dashboard_views();
    return G_SOURCE_REMOVE;
}

void gui_request_dashboard_refresh(void) {
    g_main_context_invoke(NULL, refresh_all_dashboard_views_idle, NULL);
}

// --- GUI Input Request System ---
typedef struct {
    char prompt[256];
    char input_buffer[512];
    gboolean input_requested_by_backend;
    gboolean input_provided_by_gui;
    pthread_mutex_t mutex;
    pthread_cond_t gui_provided_input_cond;
} GuiInputRequestSystem;

static GuiInputRequestSystem g_input_system;

static void initialize_input_request_system() {
    memset(&g_input_system, 0, sizeof(GuiInputRequestSystem));
    pthread_mutex_init(&g_input_system.mutex, NULL);
    pthread_cond_init(&g_input_system.gui_provided_input_cond, NULL);
    g_input_system.input_requested_by_backend = FALSE;
    g_input_system.input_provided_by_gui = FALSE;
}

// --- Callbacks for Integrated GUI Input ---
static void on_gui_input_submitted(GtkButton *button, gpointer user_data) {
    pthread_mutex_lock(&g_input_system.mutex);

    if (g_input_system.input_requested_by_backend && !g_input_system.input_provided_by_gui) {
        const char *text = gtk_editable_get_text(GTK_EDITABLE(input_entry_field));
        strncpy(g_input_system.input_buffer, text, sizeof(g_input_system.input_buffer) - 1);
        g_input_system.input_buffer[sizeof(g_input_system.input_buffer) - 1] = '\0';

        g_input_system.input_provided_by_gui = TRUE;
        pthread_cond_signal(&g_input_system.gui_provided_input_cond);

        gtk_editable_set_text(GTK_EDITABLE(input_entry_field), "");
        gtk_widget_set_visible(input_area_box, FALSE);

        if (!simulation_running) {
            gtk_widget_set_sensitive(GTK_WIDGET(start_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(reset_button), TRUE);
            gtk_widget_set_sensitive(GTK_WIDGET(algo_combo), TRUE);
            gchar *selected_algo = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(algo_combo));
            if (selected_algo && g_strcmp0(selected_algo, "Round Robin") == 0) {
                 gtk_widget_set_sensitive(quantum_entry, TRUE);
            }
            g_free(selected_algo);
            gtk_widget_set_sensitive(arrival_entry_p1, TRUE);
            gtk_widget_set_sensitive(arrival_entry_p2, TRUE);
            gtk_widget_set_sensitive(arrival_entry_p3, TRUE);
            // gtk_widget_set_sensitive(add_process_file_button, TRUE);
            gtk_widget_set_sensitive(add_process_file1, TRUE); 
            gtk_widget_set_sensitive(add_process_file2, TRUE);
            gtk_widget_set_sensitive(add_process_file3, TRUE);// Re-enable add process button
        }
    }
    pthread_mutex_unlock(&g_input_system.mutex);
}

static gboolean prepare_gui_for_input_idle(gpointer data) {
    char current_prompt[256];
    gboolean show_input_ui = FALSE;

    pthread_mutex_lock(&g_input_system.mutex);
    if (g_input_system.input_requested_by_backend && !g_input_system.input_provided_by_gui) {
        strncpy(current_prompt, g_input_system.prompt, sizeof(current_prompt) - 1);
        current_prompt[sizeof(current_prompt) - 1] = '\0';
        show_input_ui = TRUE;
    }
    pthread_mutex_unlock(&g_input_system.mutex);

    if (show_input_ui) {
        if (!input_area_box || !input_prompt_label || !input_entry_field) {
            g_warning("Integrated input UI elements are not initialized!");
            pthread_mutex_lock(&g_input_system.mutex);
            g_input_system.input_buffer[0] = '\0';
            g_input_system.input_provided_by_gui = TRUE;
            pthread_cond_signal(&g_input_system.gui_provided_input_cond);
            pthread_mutex_unlock(&g_input_system.mutex);
            return G_SOURCE_REMOVE;
        }

        gtk_label_set_text(GTK_LABEL(input_prompt_label), current_prompt);
        gtk_editable_set_text(GTK_EDITABLE(input_entry_field), "");
        gtk_widget_set_visible(input_area_box, TRUE);
        gtk_widget_grab_focus(input_entry_field);

        gtk_widget_set_sensitive(GTK_WIDGET(start_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(reset_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(algo_combo), FALSE);
        gtk_widget_set_sensitive(quantum_entry, FALSE);
        gtk_widget_set_sensitive(arrival_entry_p1, FALSE);
        gtk_widget_set_sensitive(arrival_entry_p2, FALSE);
        gtk_widget_set_sensitive(arrival_entry_p3, FALSE);
        // gtk_widget_set_sensitive(add_process_file_button, FALSE);
        gtk_widget_set_sensitive(add_process_file1, FALSE);
        gtk_widget_set_sensitive(add_process_file2, FALSE);
        gtk_widget_set_sensitive(add_process_file3, FALSE); // Disable add process button
    }
    return G_SOURCE_REMOVE;
}

gboolean gui_request_input(const char *prompt, char *destination_buffer, int dest_buffer_size) {
    if (destination_buffer == NULL || dest_buffer_size <= 0) return FALSE;
    destination_buffer[0] = '\0';

    pthread_mutex_lock(&g_input_system.mutex);

    if (please_stop_simulation) {
        pthread_mutex_unlock(&g_input_system.mutex);
        return FALSE;
    }

    strncpy(g_input_system.prompt, prompt, sizeof(g_input_system.prompt) - 1);
    g_input_system.prompt[sizeof(g_input_system.prompt) - 1] = '\0';
    g_input_system.input_buffer[0] = '\0';
    g_input_system.input_requested_by_backend = TRUE;
    g_input_system.input_provided_by_gui = FALSE;

    g_main_context_invoke(NULL, prepare_gui_for_input_idle, NULL);

    struct timespec ts;
    gboolean success = FALSE;

    while (!g_input_system.input_provided_by_gui && !please_stop_simulation) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&g_input_system.gui_provided_input_cond, &g_input_system.mutex, &ts);
    }

    if (g_input_system.input_provided_by_gui) {
        if (g_input_system.input_buffer[0] != '\0') {
            strncpy(destination_buffer, g_input_system.input_buffer, dest_buffer_size - 1);
            destination_buffer[dest_buffer_size - 1] = '\0';
            success = TRUE;
        } else {
            success = FALSE; // Input was empty, consider this not successful for backend that expects data
        }
    } else if (please_stop_simulation) {
        success = FALSE;
    }
    g_input_system.input_requested_by_backend = FALSE;
    // Do not hide input area or re-enable controls here, on_gui_input_submitted or stop handles it.
    pthread_mutex_unlock(&g_input_system.mutex);
    return success;
}

// --- Simulation Thread Logic ---
typedef struct {
    char selected_algo[50];
    int quantum;
    int arrival_times[3]; // For initial processes, can be extended or handled differently
} SimThreadArgs;

static gboolean simulation_finished_on_gui_thread(gpointer user_data) {
    gui_log_message("Simulation has finished processing.");
    gtk_label_set_text(GTK_LABEL(status_label), "Status: Simulation finished.");
    simulation_running = FALSE;

    gtk_widget_set_visible(input_area_box, FALSE);

    gtk_widget_set_sensitive(GTK_WIDGET(algo_combo), TRUE);
    gchar *selected_algo = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(algo_combo));
    if (selected_algo && g_strcmp0(selected_algo, "Round Robin") == 0) {
        gtk_widget_set_sensitive(quantum_entry, TRUE);
    } else {
        gtk_widget_set_sensitive(quantum_entry, FALSE); 
    }
    g_free(selected_algo);

    gtk_widget_set_sensitive(arrival_entry_p1, TRUE);
    gtk_widget_set_sensitive(arrival_entry_p2, TRUE);
    gtk_widget_set_sensitive(arrival_entry_p3, TRUE);
    // gtk_widget_set_sensitive(add_process_file_button, TRUE);
    gtk_widget_set_sensitive(add_process_file1, TRUE);
    gtk_widget_set_sensitive(add_process_file2, TRUE);
    gtk_widget_set_sensitive(add_process_file3, TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(start_button), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(reset_button), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(stop_button), FALSE);
    refresh_all_dashboard_views();
    return G_SOURCE_REMOVE;
}

static void* simulation_thread_func(void *arg) {
    SimThreadArgs *args = (SimThreadArgs *)arg;
    // Note: The arrival_times in args are for the *initial* 3 processes set up by default.
    // Processes added via "Add Process" button are already in pcbs[] with their own arrival times.
    // The execute_... functions should be aware of all processes in pcbs[], not just the initial three.
    // The backend's initialize_common_state handles the initial 3 processes.
    // Subsequent calls to schedulers will work on the full pcbs list.

    gui_log_message("Simulation thread started for %s (Initial P1@%d, P2@%d, P3@%d).",
                    args->selected_algo, args->arrival_times[0], args->arrival_times[1], args->arrival_times[2]);
    if (strcmp(args->selected_algo, "Round Robin") == 0) {
        execute_RR(args->arrival_times, args->quantum);
    } else if (strcmp(args->selected_algo, "FCFS") == 0) {
        execute_FCFS(args->arrival_times);
    } else if (strcmp(args->selected_algo, "MLFQ") == 0) {
        execute_MLFQ(args->arrival_times);
    } else {
        gui_log_message("Error: Unknown algorithm in thread: %s", args->selected_algo);
    }
    gui_log_message("Simulation thread completing for %s.", args->selected_algo);
    g_main_context_invoke(NULL, (GSourceFunc)simulation_finished_on_gui_thread, NULL);
    g_free(args);
    pthread_exit(NULL);
    return NULL;
}



// --- Button Callbacks ---
static void on_algo_changed(GtkComboBox *combo_box, gpointer user_data) {
    gchar *selected_algo = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_box));
    if (selected_algo == NULL) return;

    gboolean is_rr = (g_strcmp0(selected_algo, "Round Robin") == 0);
    gtk_widget_set_visible(quantum_box, is_rr);
    if (gtk_widget_get_sensitive(GTK_WIDGET(algo_combo))) { // If combo itself is sensitive
        gtk_widget_set_sensitive(quantum_entry, is_rr);
    } else { 
        gtk_widget_set_sensitive(quantum_entry, FALSE);
    }

    char algo_text[100];
    snprintf(algo_text, sizeof(algo_text), "Active Algorithm: %s", selected_algo);
    gtk_label_set_text(GTK_LABEL(overview_active_algo_label), algo_text);
    g_free(selected_algo);
}

static void on_step_clicked(GtkButton *button, gpointer user_data) {

    wait=1;
}

static void on_execute_step(GtkButton *button, gpointer user_data) {

    check=0;
}


static void on_auto_run_clicked(GtkButton *button, gpointer user_data) {
 
    wait=0;
}

static void on_start_clicked(GtkButton *button, gpointer user_data) {
    if (simulation_running) {
        gui_log_message("Attempted to start while already running.");
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Already running!");
        return;
    }
    // Clear previous logs only if explicitly desired, or append.
    // gtk_text_buffer_set_text(output_textbuffer, "", -1); // Clears log, done by reset if needed

    gchar *selected_algo_str = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(algo_combo));
    if (selected_algo_str == NULL) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Error - No algorithm selected.");
        return;
    }
    SimThreadArgs *thread_args = g_new(SimThreadArgs, 1);
    strncpy(thread_args->selected_algo, selected_algo_str, sizeof(thread_args->selected_algo) - 1);
    thread_args->selected_algo[sizeof(thread_args->selected_algo) - 1] = '\0';

    // These arrival times are for the *initial default* processes P1, P2, P3.
    // The backend `initialize_common_state` (called by execute_... functions) will use these
    // to create/recreate these specific processes. Other processes added via "Add Process"
    // are already in pcbs[] and will be picked up by the simulation loop.
    const char *at1_str = gtk_editable_get_text(GTK_EDITABLE(arrival_entry_p1));
    const char *at2_str = gtk_editable_get_text(GTK_EDITABLE(arrival_entry_p2));
    const char *at3_str = gtk_editable_get_text(GTK_EDITABLE(arrival_entry_p3));
    thread_args->arrival_times[0] = atoi(at1_str);
    thread_args->arrival_times[1] = atoi(at2_str);
    thread_args->arrival_times[2] = atoi(at3_str);

    if (thread_args->arrival_times[0] < 0 || thread_args->arrival_times[1] < 0 || thread_args->arrival_times[2] < 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Error - Default arrival times must be non-negative.");
        g_free(thread_args); g_free(selected_algo_str); return;
    }

    if (strcmp(thread_args->selected_algo, "Round Robin") == 0) {
        const char *quantum_text = gtk_editable_get_text(GTK_EDITABLE(quantum_entry));
        thread_args->quantum = atoi(quantum_text);
        if (thread_args->quantum <= 0) {
            gtk_label_set_text(GTK_LABEL(status_label), "Status: Error - Invalid Quantum for RR.");
            g_free(thread_args); g_free(selected_algo_str); return;
        }
    } else {
        thread_args->quantum = 0; // Not used
    }
    g_free(selected_algo_str);

    gtk_widget_set_sensitive(GTK_WIDGET(algo_combo), FALSE);
    gtk_widget_set_sensitive(quantum_entry, FALSE);
    gtk_widget_set_sensitive(arrival_entry_p1, FALSE);
    gtk_widget_set_sensitive(arrival_entry_p2, FALSE);
    gtk_widget_set_sensitive(arrival_entry_p3, FALSE);
    // gtk_widget_set_sensitive(add_process_file_button, FALSE);
    gtk_widget_set_sensitive(add_process_file1, FALSE);
    gtk_widget_set_sensitive(add_process_file2, FALSE); 
    gtk_widget_set_sensitive(add_process_file3, FALSE);// Disable add process
    gtk_widget_set_sensitive(GTK_WIDGET(start_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(reset_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(stop_button), TRUE);

    gtk_widget_set_visible(input_area_box, FALSE); // Hide input area if it was visible
    simulation_running = TRUE;
    please_stop_simulation = FALSE;
    gtk_label_set_text(GTK_LABEL(status_label), "Status: Simulation starting...");
    gui_log_message("--- New Simulation Run: %s (Initial P1@%d, P2@%d, P3@%d). Other processes as added. ---",
                    thread_args->selected_algo, thread_args->arrival_times[0],
                    thread_args->arrival_times[1], thread_args->arrival_times[2]);

    if (pthread_create(&simulation_thread_id, NULL, simulation_thread_func, thread_args) != 0) {
        perror("Error creating simulation thread");
        gui_log_message("FATAL: Error creating simulation thread.");
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Error starting thread.");
        simulation_running = FALSE; // Reset flag
        simulation_finished_on_gui_thread(NULL); // Call to re-enable controls
        g_free(thread_args);
    }
}

static gboolean hide_input_and_restore_controls_idle(gpointer data) {
    gtk_widget_set_visible(input_area_box, FALSE);
    if (!simulation_running) { // If simulation isn't active (e.g. input was for setup)
         simulation_finished_on_gui_thread(NULL); // This re-enables relevant controls
    }
    // If simulation is running, its end will trigger full control re-enable.
    // Stop button was already disabled if stop was pressed.
    return G_SOURCE_REMOVE;
}

static void on_stop_clicked(GtkButton *button, gpointer user_data) {
    if (!simulation_running && !gtk_widget_get_visible(input_area_box)) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Not running or awaiting input.");
        return;
    }
    gui_log_message("Stop button clicked. Requesting simulation/input to halt...");
    gtk_label_set_text(GTK_LABEL(status_label), "Status: Stop requested...");

    pthread_mutex_lock(&g_input_system.mutex);
    please_stop_simulation = TRUE;
    if (g_input_system.input_requested_by_backend && !g_input_system.input_provided_by_gui) {
        pthread_cond_signal(&g_input_system.gui_provided_input_cond); // Wake up gui_request_input
        // The gui_request_input will return, and then the simulation thread will see please_stop_simulation
        // and exit, eventually calling simulation_finished_on_gui_thread to restore UI.
        // We also hide the input box now.
        g_main_context_invoke(NULL, hide_input_and_restore_controls_idle, NULL);
    }
    pthread_mutex_unlock(&g_input_system.mutex);
    
    // The simulation thread itself will detect please_stop_simulation and call simulation_finished_on_gui_thread.
    // Disabling stop button immediately is good.
    gtk_widget_set_sensitive(GTK_WIDGET(stop_button), FALSE);
}

static void on_reset_clicked(GtkButton *button, gpointer user_data) {
    if (simulation_running) {
         gui_log_message("Reset attempt while simulation active - Aborted.");
         gtk_label_set_text(GTK_LABEL(status_label), "Status: Cannot reset while simulation is active.");
         return;
    }

    gtk_widget_set_visible(input_area_box, FALSE); // Hide input area if visible
    gtk_text_buffer_set_text(output_textbuffer, "", -1); // Clear logs on reset
    gui_log_message("--- Simulation Reset ---");

    // Backend state reset
    initialize_cycle();      // cycle = 0;
    initialize_memory();     // Clears Memory array
    initialize_queue(&ready_queue); initialize_queue(&running_queue);
    initialize_queue(&ready_queue2); initialize_queue(&ready_queue3); initialize_queue(&ready_queue4);
    initialize_queue(&blocking_queue1); initialize_queue(&blocking_queue2); initialize_queue(&blocking_queue3);
    initialize_txtfiles();
    // Reset Semaphores (ensure holder_pid is also reset)
    semUserInput.value = 1; semUserInput.holder_pid = 0;
    semUserOutput.value = 1; semUserOutput.holder_pid = 0;
    semFile.value = 1; semFile.holder_pid = 0;
   
    // Clear all PCB entries and free allocated PCB memory
    for(int i = 0; i < MAX_PROCESSES; ++i) {
        if (pcbs[i] != NULL) {
            // Assuming create_process_from_file and create_process malloc PCB.
            // And that program instructions are in global Memory, not alloc'd per PCB.
            free(pcbs[i]); 
            pcbs[i] = NULL;
        }
    }

    wait=0;
    
    // number_of_processes = 0; // Reset process count

    // The backend's initialize_common_state (called by execute_FCFS etc.) will
    // re-create P1, P2, P3 if those files are present and named in its logic.
    // If not, then pcbs will remain empty until user adds them or starts a sim that creates them.
    // This reset makes the system clean for "Add Process" or for a new sim run.

    gui_log_message("Core simulation state reset. Add processes or configure and start.");
    gtk_label_set_text(GTK_LABEL(status_label), "Status: Simulation reset. Ready.");
    
    simulation_running = FALSE; // Should already be false
    please_stop_simulation = FALSE;

    // Restore UI to initial state (most done by simulation_finished_on_gui_thread)
    simulation_finished_on_gui_thread(NULL); 
    
    // Set default algo and trigger its UI update
    gtk_combo_box_set_active(GTK_COMBO_BOX(algo_combo), 0); 
    on_algo_changed(GTK_COMBO_BOX(algo_combo), NULL); // Ensure quantum box visibility is correct

    // Set default arrival times (optional, or leave them as user last set)
    gtk_editable_set_text(GTK_EDITABLE(arrival_entry_p1), "0");
    gtk_editable_set_text(GTK_EDITABLE(arrival_entry_p2), "1");
    gtk_editable_set_text(GTK_EDITABLE(arrival_entry_p3), "2");
    gtk_editable_set_text(GTK_EDITABLE(quantum_entry), "2");


    refresh_all_dashboard_views(); // Update dashboard to show empty state
    gui_log_message("State reset. Ready for new simulation or to add processes.");
}

// NEW: Callback for "Add Process from File" button
static void on_add_process_file_dialog_response1(GtkNativeDialog *native, int response_id, gpointer user_data) {
    GtkWindow *parent_window = GTK_WINDOW(user_data); // Or pass main_app_window directly
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);
        GFile *gfile = gtk_file_chooser_get_file(chooser);
        if (gfile) {
            char *filepath = g_file_get_path(gfile);
            g_object_unref(gfile);
            
           if (filepath) {
             strncpy(txtfiles[0], filepath, PATH_MAX - 1);
             txtfiles[0][PATH_MAX - 1] = '\0';  // Ensure null termination
             g_free(filepath);
         }

            
        }
    }
    // For GtkNativeDialog, it destroys itself after the signal.
    // If it were GtkDialog, we would call gtk_window_destroy(GTK_WINDOW(native)); here.
    // GtkNativeDialog is unreffed by GTK after the signal
    // g_object_unref(native); // This might be needed if we explicitly reffed it earlier
}

static void on_add_process_file_dialog_response2(GtkNativeDialog *native, int response_id, gpointer user_data) {
    GtkWindow *parent_window = GTK_WINDOW(user_data); // Or pass main_app_window directly

    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);
        GFile *gfile = gtk_file_chooser_get_file(chooser);
        if (gfile) {
            char *filepath = g_file_get_path(gfile);
             g_object_unref(gfile);

             if (filepath) {
                strncpy(txtfiles[1], filepath,1000-1);
                txtfiles[1][1000 - 1] = '\0';  // Ensure null termination
                g_free(filepath);
            }
                         

        }
    }
    // For GtkNativeDialog, it destroys itself after the signal.
    // If it were GtkDialog, we would call gtk_window_destroy(GTK_WINDOW(native)); here.
    // GtkNativeDialog is unreffed by GTK after the signal
    // g_object_unref(native); // This might be needed if we explicitly reffed it earlier
}

static void on_add_process_file_dialog_response3(GtkNativeDialog *native, int response_id, gpointer user_data) {
    GtkWindow *parent_window = GTK_WINDOW(user_data); // Or pass main_app_window directly

    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);
        GFile *gfile = gtk_file_chooser_get_file(chooser);
        if (gfile) {
            char *filepath = g_file_get_path(gfile);
              g_object_unref(gfile);

              if (filepath) {
                strncpy(txtfiles[2], filepath, PATH_MAX - 1);
                txtfiles[2][PATH_MAX - 1] = '\0';  // Ensure null termination
                g_free(filepath);
            }
            

        }
    }
    // For GtkNativeDialog, it destroys itself after the signal.
    // If it were GtkDialog, we would call gtk_window_destroy(GTK_WINDOW(native)); here.
    // GtkNativeDialog is unreffed by GTK after the signal
    // g_object_unref(native); // This might be needed if we explicitly reffed it earlier
}


static void on_add_process_file_clicked1(GtkButton *button, gpointer user_data) {
    if (simulation_running) {
        gui_log_message("Cannot add process while simulation is running.");
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Simulation active. Stop to add processes.");
        return;
    }

    GtkFileChooserNative *native;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    native = gtk_file_chooser_native_new("Open Process File",
                                         main_app_window, // Parent window
                                         action,
                                         "_Open",    // Default accept label
                                         "_Cancel"); // Default cancel label

    gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(native), TRUE);
    // gtk_native_dialog_set_transient_for(GTK_NATIVE_DIALOG(native), main_app_window); // Already set via constructor

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Process Files (*.txt, *.prog, *.prg)");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.prog");
    gtk_file_filter_add_pattern(filter, "*.prg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(native), filter);
    // g_object_unref(filter); // filter is owned by the chooser now, so unref it

    // Pass main_app_window or other necessary data if needed in the callback
    g_signal_connect(native, "response", G_CALLBACK(on_add_process_file_dialog_response1), main_app_window);
    
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
    
    // The native dialog is unreffed by GTK when it's done, or after the response signal.
    // If we created it with g_object_new and held a ref, we'd unref here.
    // gtk_file_chooser_native_new returns a floating reference, which is sunk by gtk_native_dialog_show
    // or when it becomes a child of a container. Here it's shown, so it's fine.
    // The documentation for GtkNativeDialog says: "The dialog will be destroyed automatically after the ::response signal is emitted."
    // So we don't need to call g_object_unref(native) or gtk_window_destroy.
}

static void on_add_process_file_clicked2(GtkButton *button, gpointer user_data) {
    if (simulation_running) {
        gui_log_message("Cannot add process while simulation is running.");
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Simulation active. Stop to add processes.");
        return;
    }

    GtkFileChooserNative *native;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    native = gtk_file_chooser_native_new("Open Process File",
                                         main_app_window, // Parent window
                                         action,
                                         "_Open",    // Default accept label
                                         "_Cancel"); // Default cancel label

    gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(native), TRUE);
    // gtk_native_dialog_set_transient_for(GTK_NATIVE_DIALOG(native), main_app_window); // Already set via constructor

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Process Files (*.txt, *.prog, *.prg)");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.prog");
    gtk_file_filter_add_pattern(filter, "*.prg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(native), filter);
    // g_object_unref(filter); // filter is owned by the chooser now, so unref it

    // Pass main_app_window or other necessary data if needed in the callback
    g_signal_connect(native, "response", G_CALLBACK(on_add_process_file_dialog_response2), main_app_window);
    
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
    
    // The native dialog is unreffed by GTK when it's done, or after the response signal.
    // If we created it with g_object_new and held a ref, we'd unref here.
    // gtk_file_chooser_native_new returns a floating reference, which is sunk by gtk_native_dialog_show
    // or when it becomes a child of a container. Here it's shown, so it's fine.
    // The documentation for GtkNativeDialog says: "The dialog will be destroyed automatically after the ::response signal is emitted."
    // So we don't need to call g_object_unref(native) or gtk_window_destroy.
}

static void on_add_process_file_clicked3(GtkButton *button, gpointer user_data) {
    if (simulation_running) {
        gui_log_message("Cannot add process while simulation is running.");
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Simulation active. Stop to add processes.");
        return;
    }

    GtkFileChooserNative *native;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    native = gtk_file_chooser_native_new("Open Process File",
                                         main_app_window, // Parent window
                                         action,
                                         "_Open",    // Default accept label
                                         "_Cancel"); // Default cancel label

    gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(native), TRUE);
    // gtk_native_dialog_set_transient_for(GTK_NATIVE_DIALOG(native), main_app_window); // Already set via constructor

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Process Files (*.txt, *.prog, *.prg)");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_filter_add_pattern(filter, "*.prog");
    gtk_file_filter_add_pattern(filter, "*.prg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(native), filter);
    // g_object_unref(filter); // filter is owned by the chooser now, so unref it

    // Pass main_app_window or other necessary data if needed in the callback
    g_signal_connect(native, "response", G_CALLBACK(on_add_process_file_dialog_response3), main_app_window);
    
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
    
    // The native dialog is unreffed by GTK when it's done, or after the response signal.
    // If we created it with g_object_new and held a ref, we'd unref here.
    // gtk_file_chooser_native_new returns a floating reference, which is sunk by gtk_native_dialog_show
    // or when it becomes a child of a container. Here it's shown, so it's fine.
    // The documentation for GtkNativeDialog says: "The dialog will be destroyed automatically after the ::response signal is emitted."
    // So we don't need to call g_object_unref(native) or gtk_window_destroy.
}

// --- Button Callbacks ---

// ... (on_clear_log_clicked, on_algo_changed) ...







static void load_custom_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();

    if (!display) {
        g_warning("Failed to get default GdkDisplay. Cannot apply CSS.");
        g_object_unref(provider);
        return;
    }

    const char *css_file_path = "style.css";

    if (g_file_test(css_file_path, G_FILE_TEST_EXISTS)) {
        gtk_css_provider_load_from_path(provider, css_file_path);
        // GTK will print g_warnings to the console if there are CSS parsing errors.
        // There's no GError returned by this function to check programmatically here.

        gtk_style_context_add_provider_for_display(display,
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gui_log_message("Custom CSS '%s' loaded (check console for parsing errors).", css_file_path);
    } else {
        gui_log_message("Custom CSS file '%s' not found. Using default styles.", css_file_path);
        // No need to call load_from_path if the file doesn't exist.
        // The provider will be empty and won't apply any styles.
    }

    g_object_unref(provider);
}



// --- Main Application Setup ---
static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    main_app_window = GTK_WINDOW(window);
    gtk_window_set_title(GTK_WINDOW(window), "Scheduler Simulator GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 1700, 1200);

    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_window_set_child(GTK_WINDOW(window), main_paned);

    GtkWidget *control_panel_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(control_panel_vbox, 10); gtk_widget_set_margin_end(control_panel_vbox, 10);
    gtk_widget_set_margin_top(control_panel_vbox, 10); gtk_widget_set_margin_bottom(control_panel_vbox, 10);
    gtk_paned_set_start_child(GTK_PANED(main_paned), control_panel_vbox);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_position(GTK_PANED(main_paned), 600); // Slightly more space for control panel

    GtkWidget *algo_label = gtk_label_new("Scheduling Algorithm:"); gtk_widget_set_halign(algo_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(control_panel_vbox), algo_label);
    algo_combo = gtk_combo_box_text_new(); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algo_combo), "FCFS"); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algo_combo), "Round Robin"); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(algo_combo), "MLFQ"); gtk_combo_box_set_active(GTK_COMBO_BOX(algo_combo), 0); g_signal_connect(algo_combo, "changed", G_CALLBACK(on_algo_changed), NULL); gtk_box_append(GTK_BOX(control_panel_vbox), algo_combo);
    quantum_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); GtkWidget *quantum_label_widget = gtk_label_new("Quantum:"); quantum_entry = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(quantum_entry), "e.g., 2"); gtk_editable_set_text(GTK_EDITABLE(quantum_entry), "2"); gtk_box_append(GTK_BOX(quantum_box), quantum_label_widget); gtk_box_append(GTK_BOX(quantum_box), quantum_entry); gtk_widget_set_hexpand(quantum_entry, TRUE); gtk_box_append(GTK_BOX(control_panel_vbox), quantum_box);
    
    // Initial Process Arrival Times (for default P1, P2, P3 if backend creates them on sim start)
    GtkWidget *initial_arrival_frame = gtk_frame_new("Initial Process Arrival Times (P1-P3)");
    GtkWidget *initial_arrival_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_margin_start(initial_arrival_vbox, 5); gtk_widget_set_margin_end(initial_arrival_vbox, 5);
    gtk_widget_set_margin_top(initial_arrival_vbox, 5); gtk_widget_set_margin_bottom(initial_arrival_vbox, 5);
    gtk_frame_set_child(GTK_FRAME(initial_arrival_frame), initial_arrival_vbox);
    gtk_box_append(GTK_BOX(control_panel_vbox), initial_arrival_frame);

    GtkWidget *arrival_times_label = gtk_label_new("P1, P2, P3 AT (used if backend auto-loads them):"); gtk_widget_set_halign(arrival_times_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(initial_arrival_vbox), arrival_times_label);
    GtkWidget *arrival_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); gtk_box_set_homogeneous(GTK_BOX(arrival_hbox), TRUE); arrival_entry_p1 = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(arrival_entry_p1), "P1 AT"); gtk_editable_set_text(GTK_EDITABLE(arrival_entry_p1), "0"); gtk_entry_set_input_purpose(GTK_ENTRY(arrival_entry_p1), GTK_INPUT_PURPOSE_DIGITS); gtk_box_append(GTK_BOX(arrival_hbox), arrival_entry_p1); arrival_entry_p2 = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(arrival_entry_p2), "P2 AT"); gtk_editable_set_text(GTK_EDITABLE(arrival_entry_p2), "1"); gtk_entry_set_input_purpose(GTK_ENTRY(arrival_entry_p2), GTK_INPUT_PURPOSE_DIGITS); gtk_box_append(GTK_BOX(arrival_hbox), arrival_entry_p2); arrival_entry_p3 = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(arrival_entry_p3), "P3 AT"); gtk_editable_set_text(GTK_EDITABLE(arrival_entry_p3), "2"); gtk_entry_set_input_purpose(GTK_ENTRY(arrival_entry_p3), GTK_INPUT_PURPOSE_DIGITS); gtk_box_append(GTK_BOX(arrival_hbox), arrival_entry_p3); gtk_box_append(GTK_BOX(initial_arrival_vbox), arrival_hbox);

    // Add Process Button
    add_process_file1 = gtk_button_new_with_label("Add Process 1 from File...");
    g_signal_connect(add_process_file1, "clicked", G_CALLBACK(on_add_process_file_clicked1), NULL);
    gtk_box_append(GTK_BOX(control_panel_vbox), add_process_file1);
    gtk_widget_set_margin_top(add_process_file1, 10);

    add_process_file2 = gtk_button_new_with_label("Add Process 2 from File...");
    g_signal_connect(add_process_file2, "clicked", G_CALLBACK(on_add_process_file_clicked2), NULL);
    gtk_box_append(GTK_BOX(control_panel_vbox), add_process_file2);
    gtk_widget_set_margin_top(add_process_file2, 10);

    add_process_file3 = gtk_button_new_with_label("Add Process 3 from File...");
    g_signal_connect(add_process_file3, "clicked", G_CALLBACK(on_add_process_file_clicked3), NULL);
    gtk_box_append(GTK_BOX(control_panel_vbox), add_process_file3);
    gtk_widget_set_margin_top(add_process_file3, 10);


    GtkWidget *button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); gtk_box_set_homogeneous(GTK_BOX(button_hbox), TRUE); start_button = gtk_button_new_with_label("Start"); stop_button = gtk_button_new_with_label("Stop"); reset_button = gtk_button_new_with_label("Reset");step_button = gtk_button_new_with_label("Step");execute_step=gtk_button_new_with_label("ExecuteStep");
    auto_button = gtk_button_new_with_label("Auto Run"); g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), NULL); g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), NULL); g_signal_connect(reset_button, "clicked", G_CALLBACK(on_reset_clicked), NULL);g_signal_connect(step_button, "clicked", G_CALLBACK(on_step_clicked), NULL);;g_signal_connect(execute_step, "clicked", G_CALLBACK(on_execute_step), NULL);
    g_signal_connect(auto_button, "clicked", G_CALLBACK(on_auto_run_clicked), NULL); gtk_box_append(GTK_BOX(button_hbox), start_button); gtk_box_append(GTK_BOX(button_hbox), stop_button); gtk_box_append(GTK_BOX(button_hbox), reset_button); gtk_box_append(GTK_BOX(control_panel_vbox), button_hbox);gtk_box_append(GTK_BOX(button_hbox), step_button);gtk_box_append(GTK_BOX(button_hbox), execute_step);
    gtk_box_append(GTK_BOX(button_hbox), auto_button);
    status_label = gtk_label_new("Status: Waiting..."); gtk_widget_set_halign(status_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(control_panel_vbox), status_label);
    input_area_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_set_margin_top(input_area_box, 10); input_prompt_label = gtk_label_new("Input prompt will appear here"); gtk_widget_set_halign(input_prompt_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(input_area_box), input_prompt_label); GtkWidget *input_field_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); input_entry_field = gtk_entry_new(); gtk_widget_set_hexpand(input_entry_field, TRUE); gtk_box_append(GTK_BOX(input_field_hbox), input_entry_field); input_submit_button = gtk_button_new_with_label("Submit Input"); gtk_box_append(GTK_BOX(input_field_hbox), input_submit_button); gtk_box_append(GTK_BOX(input_area_box), input_field_hbox); gtk_box_append(GTK_BOX(control_panel_vbox), input_area_box); gtk_widget_set_visible(input_area_box, FALSE); g_signal_connect(input_submit_button, "clicked", G_CALLBACK(on_gui_input_submitted), NULL); g_signal_connect(input_entry_field, "activate", G_CALLBACK(on_gui_input_submitted), NULL);

    
    GtkWidget *dashboard_grid = gtk_grid_new(); gtk_widget_set_margin_start(dashboard_grid, 10); gtk_widget_set_margin_end(dashboard_grid, 10); gtk_widget_set_margin_top(dashboard_grid, 10); gtk_widget_set_margin_bottom(dashboard_grid, 10); gtk_grid_set_row_spacing(GTK_GRID(dashboard_grid), 10); gtk_grid_set_column_spacing(GTK_GRID(dashboard_grid), 10); gtk_paned_set_end_child(GTK_PANED(main_paned), dashboard_grid); gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);
    GtkWidget *overview_frame = gtk_frame_new("Overview"); gtk_widget_set_hexpand(overview_frame, TRUE); GtkWidget *overview_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_set_margin_start(overview_vbox, 5); gtk_widget_set_margin_end(overview_vbox, 5); gtk_widget_set_margin_top(overview_vbox, 5); gtk_widget_set_margin_bottom(overview_vbox, 5); gtk_frame_set_child(GTK_FRAME(overview_frame), overview_vbox); overview_num_processes_label = gtk_label_new("Total Active Processes: 0"); gtk_widget_set_halign(overview_num_processes_label, GTK_ALIGN_START); overview_clock_cycle_label = gtk_label_new("Clock Cycle: 0"); gtk_widget_set_halign(overview_clock_cycle_label, GTK_ALIGN_START); overview_active_algo_label = gtk_label_new("Active Algorithm: FCFS"); gtk_widget_set_halign(overview_active_algo_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(overview_vbox), overview_num_processes_label); gtk_box_append(GTK_BOX(overview_vbox), overview_clock_cycle_label); gtk_box_append(GTK_BOX(overview_vbox), overview_active_algo_label); gtk_grid_attach(GTK_GRID(dashboard_grid), overview_frame, 0, 0, 1, 1);
    GtkWidget *queue_frame = gtk_frame_new("Queues & Running Process"); gtk_widget_set_hexpand(queue_frame, TRUE); GtkWidget *queue_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_set_margin_start(queue_vbox, 5); gtk_widget_set_margin_end(queue_vbox, 5); gtk_widget_set_margin_top(queue_vbox, 5); gtk_widget_set_margin_bottom(queue_vbox, 5); gtk_frame_set_child(GTK_FRAME(queue_frame), queue_vbox); ready_queue_label = gtk_label_new("Ready Queue: Empty"); gtk_widget_set_halign(ready_queue_label, GTK_ALIGN_START); running_process_label = gtk_label_new("Running Process: None"); gtk_widget_set_halign(running_process_label, GTK_ALIGN_START); blocked_queue_label = gtk_label_new("Blocked Queue(s): Empty"); gtk_widget_set_halign(blocked_queue_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(queue_vbox), ready_queue_label); gtk_box_append(GTK_BOX(queue_vbox), running_process_label); gtk_box_append(GTK_BOX(queue_vbox), blocked_queue_label); gtk_grid_attach(GTK_GRID(dashboard_grid), queue_frame, 1, 0, 1, 1);
    GtkWidget *resource_frame = gtk_frame_new("Resource Status"); gtk_widget_set_hexpand(resource_frame, TRUE); GtkWidget *resource_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_set_margin_start(resource_vbox, 5); gtk_widget_set_margin_end(resource_vbox, 5); gtk_widget_set_margin_top(resource_vbox, 5); gtk_widget_set_margin_bottom(resource_vbox, 5); gtk_frame_set_child(GTK_FRAME(resource_frame), resource_vbox); resource_input_status_label = gtk_label_new("User Input: Free"); gtk_widget_set_halign(resource_input_status_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(resource_vbox), resource_input_status_label); resource_output_status_label = gtk_label_new("User Output: Free"); gtk_widget_set_halign(resource_output_status_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(resource_vbox), resource_output_status_label); resource_file_status_label = gtk_label_new("File Access: Free"); gtk_widget_set_halign(resource_file_status_label, GTK_ALIGN_START); gtk_box_append(GTK_BOX(resource_vbox), resource_file_status_label); gtk_grid_attach(GTK_GRID(dashboard_grid), resource_frame, 2, 0, 1, 1); 

    GtkWidget *process_list_frame = gtk_frame_new("Process List"); gtk_widget_set_hexpand(process_list_frame, TRUE); gtk_widget_set_vexpand(process_list_frame, TRUE); GtkWidget *scrolled_window_processes = gtk_scrolled_window_new(); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_processes), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); gtk_frame_set_child(GTK_FRAME(process_list_frame), scrolled_window_processes); process_list_treeview = gtk_tree_view_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window_processes), process_list_treeview); const char *col_titles[] = {"PID", "State", "Pri", "Mem L", "Mem U", "PC"}; for (int i = 0; i < NUM_PROCESS_COLS; i++) { GtkCellRenderer *renderer = gtk_cell_renderer_text_new(); GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(col_titles[i], renderer, "text", i, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(process_list_treeview), column); } process_list_store = gtk_list_store_new(NUM_PROCESS_COLS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT); gtk_tree_view_set_model(GTK_TREE_VIEW(process_list_treeview), GTK_TREE_MODEL(process_list_store));
    gtk_grid_attach(GTK_GRID(dashboard_grid), process_list_frame, 0, 1, 1, 1); 
    GtkWidget *memory_view_frame = gtk_frame_new("Memory View (0-59)"); gtk_widget_set_hexpand(memory_view_frame, TRUE); gtk_widget_set_vexpand(memory_view_frame, TRUE); GtkWidget *memory_scrolled_window = gtk_scrolled_window_new(); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(memory_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); gtk_frame_set_child(GTK_FRAME(memory_view_frame), memory_scrolled_window); memory_view_textview = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(memory_view_textview), FALSE); gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(memory_view_textview), FALSE); gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(memory_view_textview), GTK_WRAP_NONE); gtk_widget_add_css_class(memory_view_textview, "monospace"); memory_view_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(memory_view_textview)); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(memory_scrolled_window), memory_view_textview);
    gtk_grid_attach(GTK_GRID(dashboard_grid), memory_view_frame, 1, 1, 2, 1); 

    GtkWidget *output_frame = gtk_frame_new("Simulation Log / Events"); gtk_widget_set_hexpand(output_frame, TRUE); gtk_widget_set_vexpand(output_frame, TRUE); GtkWidget *output_scrolled_window = gtk_scrolled_window_new(); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(output_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC); gtk_frame_set_child(GTK_FRAME(output_frame), output_scrolled_window); output_textview = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(output_textview), FALSE); gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(output_textview), FALSE); gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output_textview), GTK_WRAP_WORD_CHAR); output_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_textview)); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(output_scrolled_window), output_textview);
    gtk_grid_attach(GTK_GRID(dashboard_grid), output_frame, 0, 2, 3, 1); 

    on_reset_clicked(NULL, NULL); // Initialize UI state and backend state
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    initialize_input_request_system();
    // Initialize backend systems if needed before GUI (e.g., if testm22.c has a global init)
    // For example, if testm22.c has its own main or an explicit init function:
    // backend_initialize_globals(); // hypothetical
    

    GtkApplication *app = gtk_application_new("com.example.SimApp", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "startup", G_CALLBACK(load_custom_css), NULL); 
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    pthread_mutex_destroy(&g_input_system.mutex);
    pthread_cond_destroy(&g_input_system.gui_provided_input_cond);
   
    return status;
}