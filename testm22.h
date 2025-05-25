#ifndef TESTM22_H
#define TESTM22_H

#include <stdbool.h>
#include <stdarg.h>

// Define necessary constants and memory size
#define MAX_LINES 1000
#define MAX_LINE_LENGTH 1000
#define MEMORY_SIZE 60
#define MAX_PROCESSES 3
#define number_of_processes 3 // Consider making this one constant with MAX_PROCESSES
#define LEVELS 4
#ifndef GLIB_H_INSIDE // Or another suitable include guard for this mini-typedef
    #ifndef gboolean // Check if it's already defined (e.g., by another minimal include)
        typedef int gboolean; // GLib's gboolean is often an int
        #define TRUE 1
        #define FALSE 0
    #endif
#endif
// -----------------------------------------------------------------
// --- STEP 1: Define all your structs and enums FIRST ---
// -----------------------------------------------------------------

typedef struct {
    char *Name;
    char *Value;
} MemoryWord;

typedef enum {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} ProcessState;

typedef struct {
    int pid;
    int burst_time;
    int executed_INST;
    int arrival_time;
    int time_entered_queue;
    ProcessState state;
} Process;

typedef struct {
    int Pid;
    ProcessState State;
    int PC;
    int current_priority;
    int memory_lower_bound;
    int memory_upper_bound;
} PCB;

// Define Queue struct BEFORE using it in extern declarations
typedef struct {
    Process *queue[MAX_PROCESSES];
    int front;
    int rear;
    int size;
} Queue;

// Define Semaphore struct BEFORE using it in extern declarations
typedef struct Semaphore {
    char* name;
    int value;      // Typically 1 for mutex, can be >1 for counting semaphore
    int holder_pid; // NEW: PID of the process holding the lock, or -1/0 if free
} Semaphore;


// -----------------------------------------------------------------
// --- STEP 2: Declare global variables using extern AFTER types are defined ---
// -----------------------------------------------------------------
extern char txtfiles[MAX_PROCESSES][1000];
extern int cycle;
extern int wait;
extern int exec_cont;
extern int check;
// extern int PC1, PC2, PC3, PC; // Commenting out unused/potentially confusing globals
// extern int lower_bound, upper_bound; // Commenting out unused/potentially confusing globals
extern MemoryWord Memory[MEMORY_SIZE];

// Now the compiler knows what 'Queue' is
extern Queue ready_queue;
extern Queue running_queue;
extern Queue ready_queue2;
extern Queue ready_queue3;
extern Queue ready_queue4;
extern Queue blocking_queue1;
extern Queue blocking_queue2;
extern Queue blocking_queue3;

// Now the compiler knows what 'Semaphore' is
extern Semaphore semUserInput;
extern Semaphore semUserOutput;
extern Semaphore semFile;

// Now the compiler knows what 'PCB' is
extern PCB *pcbs[MAX_PROCESSES]; // or number_of_processes if that's the intended size
extern volatile gboolean please_stop_simulation;

// -----------------------------------------------------------------
// --- STEP 3: Declare your function prototypes ---
// -----------------------------------------------------------------
char* get_formatted_memory_string_for_gui(void);
void gui_log_message(const char *format, ...);
void run_simulation(); // This function seems to be a stub currently
const char* get_simulation_status(); // This function seems to be a stub currently
void initialize_cycle();
void initialize_txtfiles();
void initialize_memory();
void enqueue(Queue *q, Process *process);
Process *dequeue(Queue *q);
Process* top(Queue *q);
void initialize_queue(Queue *q);
const char* process_state_to_string(ProcessState state);
Process *create_process(int processID, int arrival_time, int size);
PCB *create_pcb(int process_id, int lower_bound, int upper_bound);
int calculate_memory_bounds(int size_needed, int *lower_bound, int *upper_bound);
void allocate_memory(PCB *pcb, char *process_id, int size_needed, int lower_bound, int upper_bound, char **program);
void print_memory();
int PC_calc(int lower_bound, int pc);
void update(PCB pcb, MemoryWord memory[]);
int read_program_file(const char *file_path, char **program);
char* readFile(const char *x); // Be mindful of the static buffer issue in this function's implementation
void printFromTo(char *word2, char *word3, PCB *pcb);
void printFile(const char *filename);
void writeFile(const char *a, const char *b, PCB *pcb);
void assign(char *variable, char *value, PCB *pcb);
void semWait(char *x, PCB *pcb);
void semSignal(char *x, PCB *pcb);
bool are_processes_equal(Process a, Process b);
bool isInBlockingQueue1(Process* p);
bool isInBlockingQueue2(Process* p);
bool isInBlockingQueue3(Process* p);
void execute_RR(int arrival_times[3], int Quantum);
void execute_FCFS(int arrival_times[3]);
void execute_MLFQ(int arrival_times[3]);
#endif // TESTM22_H