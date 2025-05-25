#include "testm22.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>
#include "gui_io.h"

// #define MAX_LINES 1000
// #define MAX_LINE_LENGTH 1000
// #define MEMORY_SIZE 60
// #define MAX_PROCESSES 3
// #define number_of_processes 3
// #define LEVELS 4
char txtfiles[MAX_PROCESSES][1000];
int cycle;
int wait;
int exec_cont;
int check;
int PC1 = -1;
int PC2 = -1;
int PC3 = -1;
int PC; // <-- Added missing global PC
int lower_bound;
int upper_bound;



// typedef struct {
//     char *Name;
//     char *Value;
// } MemoryWord; // Name w data strings

MemoryWord Memory[MEMORY_SIZE];
// static char sim_status[256];

void run_simulation() {
    gui_log_message("run_simulation() called (stub).");
}

const char* get_simulation_status() {
    return "Status updated in GUI log.";
}
void initialize_cycle(){
    cycle=0;
}

void initialize_txtfiles(){
    for(int i=0;i<MAX_PROCESSES;i++){
        strncpy(txtfiles[i], "1", 1000 - 1);  // Copy "1" into txtfiles[i], leaving room for the null terminator
        txtfiles[i][1000 - 1] = '\0';  // Ensure null termination

    }
}
void initialize_memory() {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        Memory[i].Name = NULL;
        Memory[i].Value = NULL;
    }
}

// typedef enum {
//     NEW,
//     READY,
//     RUNNING,
//     BLOCKED,
//     TERMINATED
// } ProcessState;

// typedef struct {
//     int pid;
//     int burst_time;
//     int executed_INST;
//     int arrival_time;
//     ProcessState state;
// } Process;

// typedef struct {
//     int Pid;
//     ProcessState State;
//     int PC;
//     int current_priority;
//     int memory_lower_bound;
//     int memory_upper_bound;
// } PCB;


// typedef struct {
//     Process *queue[MAX_PROCESSES];
//     int front;
//     int rear;
//     int size;
// } Queue;

void initialize_queue(Queue *q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

const char* process_state_to_string(ProcessState state) {
    switch (state) {
        case NEW: return "NEW";
        case READY: return "READY";
        case RUNNING: return "RUNNING";
        case BLOCKED: return "BLOCKED";
        case TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

void enqueue(Queue *q, Process *process) {
    if (q->size == MAX_PROCESSES) {
        
        gui_log_message("Queue is full, cannot enqueue process %d.", process->pid);
        return;
    }

    // First element
    if (q->size == 0) {
        q->front = 0;
        q->rear = 0;
    } else {
        q->rear = (q->rear + 1) % MAX_PROCESSES;
    }

    q->queue[q->rear] = process;
    q->size++;

    process->time_entered_queue=cycle;
}


Process *dequeue(Queue *q) {
    if (q->size == 0) {
        gui_log_message("Queue is empty, cannot dequeue process."); 
        return NULL;
    }

    Process *process = q->queue[q->front];

    if (q->size == 1) {
        // Reset the queue
        q->front = 0;
        q->rear = -1;
    } else {
        q->front = (q->front + 1) % MAX_PROCESSES;
    }

    q->size--;
    return process;
}



Process* top(Queue *q) {
    if (q->size==0) {
        printf("Queue is empty!\n");
        return NULL;
    }
    return q->queue[q->front];
}


Queue ready_queue;
Queue running_queue;
Queue ready_queue2;
Queue ready_queue3;
Queue ready_queue4;
Queue blocking_queue1;
Queue blocking_queue2;
Queue blocking_queue3;






Process *create_process(int processID, int arrival_time, int size) {
    Process *process = (Process *)malloc(sizeof(Process));
    process->pid = processID;
    process->burst_time = size; // Assuming each instruction takes one time unit
    process->executed_INST = 0;
    process->state = NEW;
    process->arrival_time = arrival_time;
    return process;
}




PCB *create_pcb(int process_id, int lower_bound, int upper_bound) {
    PCB *pcb = (PCB *)malloc(sizeof(PCB));
    pcb->Pid = process_id;
    pcb->State = NEW;
    pcb->current_priority=1;
    pcb->PC = 0; // Starting at the first instruction
    pcb->memory_lower_bound = lower_bound;
    pcb->memory_upper_bound = upper_bound;
    return pcb;
}

int calculate_memory_bounds(int size_needed, int *lower_bound, int *upper_bound) {
    for (int i = 0; i <= MEMORY_SIZE - size_needed; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        int free_block = 1;
        for (int j = 0; j < size_needed; j++) {
            if (Memory[i + j].Name != NULL) {
                free_block = 0;
                break;
            }
        }
        if (free_block) {
            *lower_bound = i;
            *upper_bound = i + size_needed - 1;
            return 1;
        }
    }
    return 0;
}

void allocate_memory(PCB *pcb, char *process_id, int size_needed, int lower_bound, int upper_bound, char **program) {
    int i = lower_bound;

    Memory[i].Name = strdup("Pid");
    Memory[i].Value = strdup(process_id);

    char pc_str[20];
    char lower_bound_str[20];
    char upper_bound_str[20];
    char currentpriority[20];

    snprintf(lower_bound_str, sizeof(lower_bound_str), "%d", lower_bound);
    snprintf(upper_bound_str, sizeof(upper_bound_str), "%d", upper_bound);
    snprintf(pc_str, sizeof(pc_str), "%d", pcb->PC);
    snprintf(currentpriority, sizeof(currentpriority), "%d", pcb->current_priority);


    Memory[i + 1].Name = strdup("State");
    Memory[i + 1].Value = strdup(process_state_to_string(pcb->State));

    Memory[i + 2].Name = strdup("PC");
    Memory[i + 2].Value = strdup(pc_str);

    Memory[i + 3].Name = strdup("Current_priority");
    Memory[i + 3].Value = strdup(currentpriority);

    Memory[i + 4].Name = strdup("memory_lower_bound");
    Memory[i + 4].Value = strdup(lower_bound_str);

    Memory[i + 5].Name = strdup("memory_upper_bound");
    Memory[i + 5].Value = strdup(upper_bound_str);

    for (int k = 0; k < 3; k++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        Memory[i + 6 + k].Name = NULL;
        Memory[i + 6 + k].Value = NULL;
    }

    int temp = 0;
    for (int r = 9; r < size_needed; r++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        Memory[r + i].Name = strdup("Instruction");
        Memory[r + i].Value = strdup(program[temp]);
        temp++;
    }
}





// This is the NEW function that will be called by the GUI
char* get_formatted_memory_string_for_gui(void) {
    GString *memory_block_str = g_string_new(""); // Use GString for easy appending

    // You can keep the "--- Memory Dump ---" header if you like, or omit it for the dedicated view
    // g_string_append(memory_block_str, "--- Memory Dump ---\n");

    int instCount = 1; // These counters are specific to your original print logic
    int varCount = 1;

    for (int i = 0; i < MEMORY_SIZE; i++) {
        // For a quick formatting function, checking please_stop_simulation might be overkill
        // unless MEMORY_SIZE is extremely large and this takes significant time.
        // if (please_stop_simulation) {
        //     g_string_append(memory_block_str, "Memory formatting interrupted.\n");
        //     break;
        // }

        char line_buffer[256]; // Temporary buffer to format each line

        if (Memory[i].Name != NULL && Memory[i].Value != NULL) {
            if (strcmp(Memory[i].Name, "Instruction") == 0) {
                snprintf(line_buffer, sizeof(line_buffer), "Mem[%02d]: %s %d = %s", i, Memory[i].Name, instCount, Memory[i].Value);
                instCount++;
            } else if (strcmp(Memory[i].Name, "Variable") == 0) {
                snprintf(line_buffer, sizeof(line_buffer), "Mem[%02d]: %s %d = %s", i, Memory[i].Name, varCount, Memory[i].Value);
                varCount++;
            } else {
                snprintf(line_buffer, sizeof(line_buffer), "Mem[%02d]: %s = %s", i, Memory[i].Name, Memory[i].Value);
            }
        } else if (Memory[i].Name != NULL) {
            snprintf(line_buffer, sizeof(line_buffer), "Mem[%02d]: %s (Value: NULL)", i, Memory[i].Name);
        } else {
            snprintf(line_buffer, sizeof(line_buffer), "Mem[%02d]: Empty", i);
        }
        g_string_append(memory_block_str, line_buffer);
        g_string_append(memory_block_str, "\n"); // Add a newline after each memory entry
    }

    // You can keep the "--- End Memory Dump ---" footer if you like
    // g_string_append(memory_block_str, "--- End Memory Dump ---\n");

    // Return the duplicated string (caller must free it using g_free)
    return g_string_free(memory_block_str, FALSE); // FALSE keeps the buffer, g_string_free deallocates the GString struct itself
}


void print_memory() {
    gui_log_message("--- Memory Dump ---");
    int instCount = 1;
    int varCount = 1;
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (please_stop_simulation) { gui_log_message("Memory dump interrupted."); return; }
        if (Memory[i].Name != NULL && Memory[i].Value != NULL) {
            // If the memory entry is for an instruction
            if (strcmp(Memory[i].Name, "Instruction") == 0) {
                gui_log_message("Mem[%d]: %s %d = %s", i, Memory[i].Name, instCount, Memory[i].Value);
                // printf("Memory[%d]: %s %d = %s\n", i, Memory[i].Name, instCount, Memory[i].Value);
                instCount++;
            }
            // If the memory entry is for a variable
            else if (strcmp(Memory[i].Name, "Variable") == 0) {
                gui_log_message("Mem[%d]: %s %d = %s", i, Memory[i].Name, varCount, Memory[i].Value);
                // printf("Memory[%d]: %s %d = %s\n", i, Memory[i].Name, varCount, Memory[i].Value);
                varCount++;
            }
            // Otherwise, just print the name and value
            else {
                gui_log_message("Mem[%d]: %s = %s", i, Memory[i].Name, Memory[i].Value);
                // printf("Memory[%d]: %s = %s\n", i, Memory[i].Name, Memory[i].Value);
            }
        }
        // Case when only the name is available
        else if (Memory[i].Name != NULL) {
            gui_log_message("Mem[%d]: %s (Value: NULL)", i, Memory[i].Name);
            // printf("Memory[%d]: %s\n", i, Memory[i].Name);
        }
        // Case when memory is empty (both name and value are NULL)
        else {
            gui_log_message("Mem[%d]: Empty", i);
            // printf("Memory[%d]: Empty\n", i);
        }
    }
    gui_log_message("--- End Memory Dump ---");
}


int PC_calc(int lower_bound, int pc) {
    int temp = pc + lower_bound + 9;
    PC = temp;
    return temp;
}

void update(PCB pcb, MemoryWord memory[]) {
    int id = pcb.Pid;
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (memory[i].Name != NULL && strcmp(memory[i].Name, "Pid") == 0) {
            int value = atoi(memory[i].Value);
            if (value == id) {
                char currentpriority[20];
                char pc[20];
                snprintf(currentpriority, sizeof(currentpriority), "%d", pcb.current_priority);
                snprintf(pc, sizeof(pc), "%d", pcb.PC);
                memory[i + 1].Value = strdup(process_state_to_string(pcb.State));
                memory[i + 2].Value = strdup(pc);
                memory[i + 3].Value = strdup(currentpriority);
            }
        }
    }
}



//read the files & store the instructions & number of lines(service time)
int read_program_file(const char *file_path, char **program)
{
  FILE *file = fopen(file_path, "r");
  if (file == NULL)
  {
    gui_log_message("Error: Unable to open file %s", file_path);
    // fprintf(stderr, "Error: Unable to open file %s\n", file_path);
    return -1;
  }

  int num_lines = 0;
  char line[MAX_LINE_LENGTH];
  while (fgets(line, MAX_LINE_LENGTH, file) != NULL && num_lines < MAX_LINES)
  {
    if (please_stop_simulation) {
        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
        break; // or return from current function if that makes sense
    }
    if (line[strlen(line) - 1] == '\n')
    {
      line[strlen(line) - 1] = '\0';
    }
    program[num_lines] = strdup(line);
    num_lines++;
  } 
  gui_log_message("Program file %s read: %d lines.", file_path, num_lines);
  //printf("\nprogram is read\n");
  fclose(file);
  return num_lines;
}



char* readFile(const char *x) {
    FILE *file = fopen(x, "r");
    if (file == NULL) {
        gui_log_message("Could not open file (literal name) %s for reading", x);
        // printf("Could not open file %s for reading\n", x);
        return NULL;
    }

    static char buffer[1000]; 
    int index = 0;
    char ch;

    while ((ch = fgetc(file)) != EOF && index < sizeof(buffer) - 1) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        buffer[index++] = ch;
        
    }

    buffer[index] = '\0';  
    fclose(file);
    return buffer;
}



void printFromTo(char *word2, char *word3, PCB *pcb) {
    char *value1 = NULL;
    char *value2 = NULL;
    int x=pcb->memory_lower_bound;
    int y=pcb->memory_upper_bound;
    // Find the variables in memory
    for (int i = x; i < y; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (Memory[i].Name != NULL && strcmp(Memory[i].Name, word2) == 0) {
            value1 = Memory[i].Value;
        }
        if (Memory[i].Name != NULL && strcmp(Memory[i].Name, word3) == 0) {
            value2 = Memory[i].Value;
        }
    }
    
    // Check if found
    if (value1 == NULL || value2 == NULL) {
        printf("Error: One or both variables not found in memory.\n");
        return;
    }

    // Check that both values are numbers
    int isValue1Number = 1, isValue2Number = 1;
    for (int i = 0; value1[i] != '\0'; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (!isdigit(value1[i]) && !(i == 0 && value1[i] == '-')) {
            isValue1Number = 0;
            break;
        }
    }
    for (int i = 0; value2[i] != '\0'; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (!isdigit(value2[i]) && !(i == 0 && value2[i] == '-')) {
            isValue2Number = 0;
            break;
        }
    }

    if (isValue1Number && isValue2Number) {
        int num1 = atoi(value1);
        int num2 = atoi(value2);

        if (num1 <= num2) {
            for (int i = num1; i <= num2; i++) {
                gui_log_message("%d",i);
                // printf("%d ", i);
            }
        } else {
            for (int i = num1; i >= num2; i--) {
                gui_log_message("%d",i);
            //   printf("%d ", i);
            }
        }
        printf("\n");
    } else {
        printf("Error: Both inputs must be numbers.\n");
    }
}

void printFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        gui_log_message("printFile Error: Unable to open file %s for reading.", filename);
        // printf("Error: Unable to open file %s for reading.\n", filename);
        return;
    }
    gui_log_message("--- Contents of file %s ---", filename);
    // printf("Contents of %s:\n", filename);

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0; // Remove newline
        gui_log_message("%s", line); 
    }

    fclose(file);
}



void writeFile(const char *a, const char *b, PCB  *pcb) {
    char *filename = NULL;
    char *data = NULL;

    int x = pcb->memory_lower_bound;
    int y = pcb->memory_upper_bound;

    
    for (int i = x; i < y; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (strcmp(Memory[i].Name, a) == 0) {
            filename = Memory[i].Value;
            break;
        }
    }

    
    for (int i = x; i < y; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (strcmp(Memory[i].Name, b) == 0) {
            data = Memory[i].Value;
            break;
        }
    }

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        gui_log_message("Error: Unable to open file %s for writing.",  filename);
        // printf("\nError: Unable to open file %s for writing.\n", filename);
        return;
    }

    fprintf(file, "%s\n", data);
    fclose(file);
    gui_log_message("Data successfully written to %s", filename);
    // printf("\nData successfully written to %s\n", filename);

    printFile(filename);
}



void assign(char *variable, char *value,PCB *pcb){
    char input_value[1000];
    int x =pcb->memory_lower_bound;
    int y=pcb->memory_upper_bound;
        strcpy(input_value, value);

    for (int i = x; i < y; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (Memory[i].Name == NULL) {
            Memory[i].Name = strdup(variable);
            Memory[i].Value = strdup(input_value);
            gui_log_message("\nAssigned %s at Memory slot [%d]:\n%s\n" , variable,i, input_value);
            // printf("\nAssigned %s at Memory slot [%d]:\n%s\n" , variable,i, input_value);
           
            return;
        } else if (strcmp(Memory[i].Name, variable) == 0){
            free(Memory[i].Value); 
            Memory[i].Value = strdup(input_value); 
            gui_log_message("Updated %s = %s at Memory slot %d\n", variable, input_value, i);
            // printf("Updated %s = %s at Memory slot %d\n", variable, input_value, i);
            return;
        }
    }
    gui_log_message("Error: Memory is full, cannot assign variable %s.\n", variable);
    // printf("Error: Memory is full, cannot assign variable %s.\n", variable);
}


typedef struct Semaphore {
    char* name;
    int value;      // Typically 1 for mutex, can be >1 for counting semaphore
    int holder_pid; // NEW: PID of the process holding the lock, or -1/0 if free
} Semaphore;


Semaphore semUserInput = {"userInput", 1, 0}; 
Semaphore semUserOutput = {"userOutput", 1, 0};
Semaphore semFile = {"file", 1, 0};



int schedule_number;
void semWait(char *x,PCB *pcb){
   
    if(strcmp(x, "userInput") == 0){
        if(semUserInput.value==1){
            semUserInput.value=0;
            gui_log_message("\nProcess %d acquired %s\n", pcb->Pid, semUserInput.name);
            semUserInput.holder_pid = pcb->Pid;
            // printf("\nProcess %d acquired %s\n", pcb->Pid, semUserInput.name);
        }
        else{
            pcb->State=BLOCKED;
            update(*pcb, Memory);
            Process *process = dequeue(&running_queue);
            if (process != NULL) {
                enqueue(&blocking_queue1, process);
                gui_log_message("\nProcess %d is blocked on %s\n", process->pid, semUserInput.name);
                printf("\nProcess %d is blocked on %s\n", process->pid, semUserInput.name);
            } else {
                gui_log_message("\nError: No process to block.\n");
                printf("\nError: No process to block.\n");
            }
            gui_request_dashboard_refresh();
        }
        
    }
    else if(strcmp(x, "userOutput") == 0){
        if(semUserOutput.value==1){
            semUserOutput.value=0;
            semUserOutput.holder_pid=pcb->Pid;
            gui_log_message("\nProcess %d acquired %s\n", pcb->Pid, semUserOutput.name);
            printf("\nProcess %d acquired %s\n", pcb->Pid, semUserOutput.name);
        }
         else{
            pcb->State=BLOCKED;
            update(*pcb, Memory);
           
            Process *process = dequeue(&running_queue);
            if (process != NULL) {
                enqueue(&blocking_queue2, process);
                gui_log_message("\nProcess %d is blocked on %s\n", process->pid, semUserOutput.name);
                printf("\nProcess %d is blocked on %s\n", process->pid, semUserOutput.name);
            } else {
                gui_log_message("\nError: No process to block.\n");
                printf("\nError: No process to block.\n");
            }
            
        }
    }
    else if(strcmp(x, "file") == 0){
        if(semFile.value==1){
            semFile.value=0;
            semFile.holder_pid=pcb->Pid;
            gui_log_message("\nProcess %d acquired %s\n", pcb->Pid, semFile.name);
            printf("\nProcess %d acquired %s\n", pcb->Pid, semFile.name);
        }
        else{
            pcb->State=BLOCKED;
            update(*pcb, Memory);
            Process *process = dequeue(&running_queue);
            if (process != NULL) {
                enqueue(&blocking_queue3, process);
                gui_log_message("\nProcess %d is blocked on %s\n", process->pid, semFile.name);
                printf("\nProcess %d is blocked on %s\n", process->pid, semFile.name);
            } else {
                gui_log_message("\nError: No process to block.\n");
                printf("\nError: No process to block.\n");
            } 
           
        }
    }
}



bool are_processes_equal(Process a, Process b) {
    return a.pid == b.pid &&
           a.burst_time == b.burst_time &&
           a.executed_INST == b.executed_INST &&
           a.arrival_time == b.arrival_time &&
           a.state == b.state;
}

PCB *pcbs[number_of_processes];



void semSignal(char *x,PCB *pcb){
    int minpri=999;
    Process *p;
    if(strcmp(x, "userInput") == 0){
        if(semUserInput.value==0 && blocking_queue1.size == 0){
            semUserInput.value=1;
            semUserInput.holder_pid =0;
            gui_log_message("User input is now Released");
        }
        else if (semUserInput.value==0 && blocking_queue1.size > 0){
         
           
            if(schedule_number==3){
               
                int size = blocking_queue1.size;
                for (int i=0; i<size;i++){
                    if (please_stop_simulation) {
                        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                        break; // or return from current function if that makes sense
                    }
                    PCB *current_pcb=pcbs[blocking_queue1.queue[i]->pid-1];
                    if(current_pcb->current_priority<minpri){
                         minpri=current_pcb->current_priority;
                    }
                  
                }
            
                for(int i=0;i<size;i++){
                    if (please_stop_simulation) {
                        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                        break; // or return from current function if that makes sense
                    }
                    PCB *current_pcb=pcbs[blocking_queue1.queue[i]->pid-1];
                    if(current_pcb->current_priority==minpri){
                       p=dequeue(&blocking_queue1);
                       semUserInput.holder_pid=p->pid;
                       gui_log_message("\nProcess %d acquired %s\n", p->pid, semUserInput.name);
                       break;
                    }
                 
                }

              
                
                PCB *highestpcb=pcbs[p->pid-1];

                highestpcb->State=READY;
                update(*highestpcb, Memory);

                if(highestpcb->current_priority==1){
                    enqueue(&ready_queue,p); // the pointer returned by dequeue

                }
                else if (highestpcb->current_priority==2){
                    enqueue(&ready_queue2,p); // the pointer returned by dequeue

                }
                else if (highestpcb->current_priority==3){
                    enqueue(&ready_queue3,p); // the pointer returned by dequeue

                }
                else if (highestpcb->current_priority==4){
                    enqueue(&ready_queue4,p); // the pointer returned by dequeue

                }
            }
            else{
                Process *pro =dequeue(&blocking_queue1);
                PCB *pcbk=pcbs[pro->pid-1];
                semUserInput.holder_pid=pcbk->Pid;
                gui_log_message("\nProcess %d unblocked and moved to the Ready Queue.\n", pro->pid);
                gui_log_message("\nProcess %d acquired %s\n", pcbk->Pid, semUserInput.name);
                printf("\nProcess %d unblocked and moved to the Ready Queue.\n", pro->pid);
                pcbk->State=READY;
                update(*pcbk,Memory);
                enqueue(&ready_queue,pro);
            }
        }

        else{
            gui_log_message("no process");
            printf("no process");
        }
    }
    else if(strcmp(x, "userOutput") == 0){
        if(semUserOutput.value==0 && blocking_queue2.size == 0){
            semUserOutput.value=1;
            semUserOutput.holder_pid=0;
            gui_log_message("User Output is now Released");
        }
        else if(semUserOutput.value==0 && blocking_queue2.size >0){
            
          
            if(schedule_number==3){
                int size = blocking_queue2.size;
                for (int i=0; i<size;i++){
                    if (please_stop_simulation) {
                        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                        break; // or return from current function if that makes sense
                    }
                    PCB *current_pcb=pcbs[blocking_queue2.queue[i]->pid-1];
                    if(current_pcb->current_priority<minpri){
                         minpri=current_pcb->current_priority;
                    }
                  
                }
            
                for(int i=0;i<size;i++){
                    if (please_stop_simulation) {
                        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                        break; // or return from current function if that makes sense
                    }
                    PCB *current_pcb=pcbs[blocking_queue2.queue[i]->pid-1];
                    if(current_pcb->current_priority==minpri){
                       p=dequeue(&blocking_queue2);
                       semUserOutput.holder_pid=p->pid;
                       gui_log_message("\nProcess %d acquired %s\n", p->pid, semUserOutput.name);
                       break;
                    }
                 
                }

              
                
                PCB *highestpcb=  pcbs[p->pid-1];

                highestpcb->State=READY;
                update(*highestpcb, Memory);

                if(highestpcb->current_priority==1){
                    enqueue(&ready_queue,p); 

                }
                else if (highestpcb->current_priority==2){
                    enqueue(&ready_queue2,p); 

                }
                else if (highestpcb->current_priority==3){
                    enqueue(&ready_queue3,p); 

                }
                else if (highestpcb->current_priority==4){
                    enqueue(&ready_queue4,p); 

                }
            }
            else{
                Process *pro =dequeue(&blocking_queue2);
                PCB *pcbk=pcbs[pro->pid-1];
                semUserOutput.holder_pid=pcbk->Pid;
                gui_log_message("\nProcess %d unblocked and moved to the Ready Queue.\n", pro->pid);
                gui_log_message("\nProcess %d acquired %s\n", pcbk->Pid, semUserOutput.name);
                printf("\nProcess %d unblocked and moved to the Ready Queue.\n", pro->pid);
                pcbk->State=READY;
                update(*pcbk,Memory);
                enqueue(&ready_queue,pro);
            }
        }
    }

    
    else if(strcmp(x, "file") == 0){
        if(semFile.value==0&& blocking_queue3.size == 0){
            semFile.holder_pid=0;
            semFile.value=1;
            gui_log_message("File is now Released");
        }
        else if (semFile.value==0 && blocking_queue3.size > 0){
            Process *process=top(&blocking_queue3);
            if (process == NULL) {
                gui_log_message("Top of blocking_queue1 is NULL.\n");
                printf("Top of blocking_queue1 is NULL.\n");
                return;
            }
            if(schedule_number==3){
                int size = blocking_queue3.size;
                for (int i=0; i<size;i++){
                    if (please_stop_simulation) {
                        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                        break; // or return from current function if that makes sense
                    }
                    PCB *current_pcb=pcbs[blocking_queue3.queue[i]->pid-1];
                    if(current_pcb->current_priority<minpri){
                         minpri=current_pcb->current_priority;
                    }
                  
                }
            
                for(int i=0;i<size;i++){
                    if (please_stop_simulation) {
                        gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                        break; // or return from current function if that makes sense
                    }
                    PCB *current_pcb=pcbs[blocking_queue3.queue[i]->pid-1];
                    if(current_pcb->current_priority==minpri){
                       p=dequeue(&blocking_queue3);
                       semFile.holder_pid=p->pid;
                       gui_log_message("\nProcess %d acquired %s\n", p->pid, semFile.name);
                       break;
                    }
                 
                }

              
                
                PCB *highestpcb=pcbs[p->pid-1];

                highestpcb->State=READY;
                update(*highestpcb, Memory);

                if(highestpcb->current_priority==1){
                    enqueue(&ready_queue,p); // the pointer returned by dequeue

                }
                else if (highestpcb->current_priority==2){
                    enqueue(&ready_queue2,p); // the pointer returned by dequeue

                }
                else if (highestpcb->current_priority==3){
                    enqueue(&ready_queue3,p); // the pointer returned by dequeue

                }
                else if (highestpcb->current_priority==4){
                    enqueue(&ready_queue4,p); // the pointer returned by dequeue

                }
            }
            else{
                Process *pro =dequeue(&blocking_queue3);
                PCB *pcbk=pcbs[pro->pid-1];
                semFile.holder_pid=pcbk->Pid;
                gui_log_message("\nProcess %d unblocked and moved to the Ready Queue.\n", pro->pid);
                gui_log_message("\nProcess %d acquired %s\n", pcbk->Pid, semFile.name);
                printf("\nProcess %d unblocked and moved to the Ready Queue.\n", pro->pid);
                pcbk->State=READY;
                update(*pcbk,Memory);
                enqueue(&ready_queue,pro);
            }
        }
    }

        
}



void methods(char *line,PCB *pcb){
    char word1[50], word2[50],word3[50], word4[50];

    sscanf(line, "%s %s %s %s", word1, word2, word3, word4) ;
    if(strcmp(word1, "print") == 0) {
    int x=pcb->memory_lower_bound;
    int y=pcb->memory_upper_bound;
    char *value1 = NULL;
    // Find the variables in memory
    for (int i = x; i < y; i++) {
        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
            break; // or return from current function if that makes sense
        }
        if (Memory[i].Name != NULL && strcmp(Memory[i].Name, word2) == 0) {
            value1 = Memory[i].Value;
        }
      
    }
    gui_log_message("The value inside %s is : \n%s\n",word2,value1);
    printf("The value inside %s is : \n%s\n",word2,value1);

    }
    else if(strcmp(word1, "assign") == 0) {
        if(strcmp(word3, "input") == 0){
            char input_value[1000];
            char gui_prompt[256];
        
            snprintf(gui_prompt, sizeof(gui_prompt), "Enter value for variable '%s':", word2);
            gui_log_message("Please enter a value for %s: ", word2);
        
            if (gui_request_input(gui_prompt, input_value, sizeof(input_value))) {
                gui_log_message("Input received for '%s': %s", word2, input_value);
                //
                // >>> YOUR ORIGINAL LOGIC TO PROCESS 'input_value' GOES HERE <<<
                // e.g., assign_value_to_simulated_variable(current_pcb, word2, input_value);
                //      or parse to number:
                // int numeric_result;
                // if (sscanf(input_value, "%d", &numeric_result) == 1) {
                //     store_numeric_variable(current_pcb, word2, numeric_result);
                // } else {
                //     handle_parse_error_for_variable(current_pcb, word2, input_value);
                // }
                //
            } else {
                if (please_stop_simulation) {
                    gui_log_message("Input for '%s' aborted due to simulation stop request.", word2);
                } else {
                    gui_log_message("No input provided or input cancelled for '%s'.", word2);
                    //
                    // >>> HANDLE CANCELLATION OR EMPTY INPUT HERE <<<
                    // e.g., assign_default_value_to_simulated_variable(current_pcb, word2);
                    //      or set_process_error_state(current_pcb, "Input cancelled by user");
                    //
                }
            }
            assign(word2, input_value,pcb);
        }
        else if(strcmp(word3, "readFile") == 0){
            char *value1 = NULL;
            int lower_bound=pcb->memory_lower_bound;
            int upper_bound=pcb->memory_upper_bound;
            for (int i = lower_bound; i < upper_bound; i++) {
                if (please_stop_simulation) {
                    gui_log_message("SIM_CORE: Stop request after complex task. Halting.");
                    break; // or return from current function if that makes sense
                }
                if (Memory[i].Name != NULL && strcmp(Memory[i].Name, word4) == 0) {
                    value1 = Memory[i].Value;
                }
            }
            if (value1 == NULL) {
                gui_log_message("Error: variable not found in memory.\n");
                printf("Error: variable not found in memory.\n");
               
            }
            char *value= readFile(value1);
            assign(word2,value, pcb);
        }
        else{
            assign(word2, word3,pcb);
        }
    }
    else if(strcmp(word1, "writeFile") == 0) {
        writeFile(word2,word3,pcb);

    }
    else if(strcmp(word1, "readFile") == 0) {
        char *value= readFile(word2);
    }
    else if(strcmp(word1, "printFromTo") == 0) {
       
        printFromTo(word2,word3,pcb);
    }
    else if(strcmp(word1, "semWait") == 0) {
        semWait(word2,pcb);
    }
    else if(strcmp(word1, "semSignal") == 0) {
        semSignal(word2,pcb);
    }
}



bool isInBlockingQueue1(Process* p) {
   
    for (size_t i = 0; i < blocking_queue1.size; i++)
    {
        Process* temp = blocking_queue1.queue[i];
        if (temp->pid == p->pid) {
            return true;
        }
        
    }
 
    return false;
}

bool isInBlockingQueue2(Process* p) {
   
    for (size_t i = 0; i < blocking_queue2.size; i++)
    {
        Process* temp = blocking_queue2.queue[i];
        if (temp->pid == p->pid) {
            return true;
        }
        
    }
 
    return false;
}

bool isInBlockingQueue3(Process* p) {
   
    for (size_t i = 0; i < blocking_queue3.size; i++)
    {
        Process* temp = blocking_queue3.queue[i];
        if (temp->pid == p->pid) {
            return true;
        }
        
    }
 
    return false;
}



void execute_RR(int arrival_times[3],int Quantum) {
    initialize_queue(&ready_queue);
    initialize_queue(&running_queue);
    initialize_queue(&blocking_queue1);
    initialize_queue(&blocking_queue2);
    initialize_queue(&blocking_queue3);
    initialize_memory();
    initialize_cycle();

    int process_count=0;
    for(int i=0;i<MAX_PROCESSES;i++){
        if (strcmp(txtfiles[i], "1") != 0) {  
            process_count++;
        }
        
    }
    
    int arrivalprog1 = arrival_times[0];
    int arrivalprog2 = arrival_times[1];
    int arrivalprog3 = arrival_times[2];

    Process *processes[process_count];
    // PCB *pcbs[number_of_processes];
    char *programs[MAX_PROCESSES][MAX_LINES];
    int program_sizes[process_count];
    int finished = 0;
    

    gui_log_message("number of files recieved %d",process_count);

 

    int quantum_counter = 0;

   

    while (finished < process_count) {
       

        if(wait){
            check=1; 
         }
         else{
             check=0;
         }

        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request detected in main loop. Halting.");
            break; // or return;
        }

          
            while(check){
                if (please_stop_simulation) {
                    gui_log_message("SIM_CORE: Stop request detected in main loop. Halting.");
                    break; // or return;
                }

                if(!wait){
                    check=0;
                }
            }

      
        // Check for new arrivals
        if (cycle == arrivalprog1) {
            if (strcmp(txtfiles[0], "1") != 0) {
            char *program1_path = txtfiles[0];
            int num_lines_program1 = read_program_file(program1_path, programs[0]);
            int size_needed1 = num_lines_program1 + 3 + 6;
            processes[0] = create_process(1, arrivalprog1, num_lines_program1);
            int lb, ub;
            if (calculate_memory_bounds(size_needed1, &lb, &ub)) {
                pcbs[0] = create_pcb(1, lb, ub);
                allocate_memory(pcbs[0], "1", size_needed1, lb, ub, programs[0]);
                int initpc = PC_calc(lb,pcbs[0]->PC);
                pcbs[0]->PC=initpc;
                enqueue(&ready_queue, processes[0]);
                program_sizes[0] = num_lines_program1;
                PCB *pcb= pcbs[0];
                pcb->State= READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 1);
                printf("Failed to allocate memory for process %d\n", 1);
                return;
            }
        }
        }

        if (cycle == arrivalprog2) {
            if (strcmp(txtfiles[1], "1") != 0) {      
            char *program2_path = txtfiles[1];
            int num_lines_program2 = read_program_file(program2_path, programs[1]);
            int size_needed2 = num_lines_program2 + 3 + 6;
            processes[1] = create_process(2, arrivalprog2, num_lines_program2);
            int lb, ub;
            if (calculate_memory_bounds(size_needed2, &lb, &ub)) {
                pcbs[1] = create_pcb(2, lb, ub);
                allocate_memory(pcbs[1], "2", size_needed2, lb, ub, programs[1]);
                int initpc = PC_calc(lb,pcbs[1]->PC);
                pcbs[1]->PC=initpc;
                enqueue(&ready_queue, processes[1]);
                program_sizes[1] = num_lines_program2;
                PCB *pcb= pcbs[1];
                pcb->State= READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 2);
                printf("Failed to allocate memory for process %d\n", 2);
                return;
            }
        }
        }

        if (cycle == arrivalprog3) {
            if (strcmp(txtfiles[2], "1") != 0) {
            char *program3_path = txtfiles[2];
            int num_lines_program3 = read_program_file(program3_path, programs[2]);
            int size_needed3 = num_lines_program3 + 3 + 6;
            processes[2] = create_process(3, arrivalprog3, num_lines_program3);
            int lb, ub;
            if (calculate_memory_bounds(size_needed3, &lb, &ub)) {
                pcbs[2] = create_pcb(3, lb, ub);
                allocate_memory(pcbs[2], "3", size_needed3, lb, ub, programs[2]);
                int initpc = PC_calc(lb,pcbs[2]->PC);
                pcbs[2]->PC=initpc;
                enqueue(&ready_queue, processes[2]);
                program_sizes[2] = num_lines_program3;
                PCB *pcb= pcbs[2];
                pcbs[2]->State= READY;
                update(*pcb, Memory);
                
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 3);
                printf("Failed to allocate memory for process %d\n", 3);
                return;
            }

        }
        }

        if(cycle==0){
            gui_request_dashboard_refresh();
            // gui_log_message("\nMemory at Cycle %d:\n", cycle);
            // printf("\nMemory at Cycle %d:\n", cycle);
            // print_memory(); 
        }

        // If no running process, move from ready_queue to running_queue
        if (running_queue.size == 0 && ready_queue.size > 0) {
            Process *p = dequeue(&ready_queue);
            enqueue(&running_queue, p);
            quantum_counter = 0;
           
           
        }

        // Execute one instruction from running process
        if (running_queue.size > 0) {
            Process *p = running_queue.queue[running_queue.front];
            int pid = p->pid;
            PCB *pcb = pcbs[pid - 1];
            pcb->State= RUNNING;
            update(*pcb, Memory);
            int pc = pcb->PC;

            int instruction_index =  pc; 
            if (instruction_index <= pcb->memory_upper_bound) {
                if (Memory[instruction_index].Value != NULL) {
                    gui_log_message("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                    printf("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                    methods(Memory[instruction_index].Value,pcb);
                    pcb->PC++;
                    update(*pcb, Memory);
                    quantum_counter++;
                }
            }

            // Check if process finished
            if (pcb->PC > pcb->memory_upper_bound) {
                gui_log_message("\nProcess %d has finished execution.\n", pid);
                printf("\nProcess %d has finished execution.\n", pid);
                dequeue(&running_queue);
                pcb->State= TERMINATED;
                update(*pcb,Memory);
                finished++;
                quantum_counter = 0;
            }
            // Check if quantum expired
            else if (quantum_counter >= Quantum) {
                
                if(!isInBlockingQueue1(p) && !isInBlockingQueue2(p) && !isInBlockingQueue3(p)){
                    gui_log_message("\nQuantum expired for process %d, moving it back to ready queue.\n", pid);
                    printf("\nQuantum expired for process %d, moving it back to ready queue.\n", pid);
                    Process *expired = dequeue(&running_queue);
                    enqueue(&ready_queue, expired);
                    pcb->State=READY;
                    update(*pcb,Memory);
                    
                }
               
                quantum_counter = 0;
            }
        }
        gui_request_dashboard_refresh();
        // gui_log_message("\nMemory after finishing Cycle %d:\n", cycle);
        // printf("\nMemory after finishing Cycle %d:\n", cycle);
        // print_memory();  
        cycle++;
    }
}


 

void execute_FCFS(int arrival_times[3]) {
    initialize_queue(&ready_queue);
    initialize_queue(&running_queue);
    initialize_queue(&blocking_queue1);
    initialize_queue(&blocking_queue2);
    initialize_queue(&blocking_queue3);
    initialize_memory();
    initialize_cycle();
    
    

    int arrivalprog1 = arrival_times[0];
    int arrivalprog2 = arrival_times[1];
    int arrivalprog3 = arrival_times[2];

    int process_count=0;
    for(int i=0;i<MAX_PROCESSES;i++){
        if (strcmp(txtfiles[i], "1") != 0) {  
            process_count++;
        }
        
    }

    gui_log_message("number of files recieved %d",process_count);

    Process *processes[process_count];
    // PCB *pcbs[number_of_processes];
    char *programs[MAX_PROCESSES][MAX_LINES];
    int program_sizes[process_count];
    int finished = 0;

   
    gui_log_message("number of files recieved %d",process_count);
  

    while (finished < process_count ) {

        if(wait){
            check=1; 
         }
         else{
             check=0;
         }

        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request detected in main loop. Halting.");
            break; // or return;
        }

          
            while(check){
                if (please_stop_simulation) {
                    gui_log_message("SIM_CORE: Stop request detected in main loop. Halting.");
                    break; // or return;
                }

                if(!wait){
                    check=0;
                }
            }
          

       
        
        // Check for new arrivals
       
    
       

        if (cycle == arrivalprog1) {
            if (strcmp(txtfiles[0], "1") != 0) {
            char *program1_path = txtfiles[0];
            int num_lines_program1 = read_program_file(program1_path, programs[0]);
            int size_needed1 = num_lines_program1 + 3 + 6;
            processes[0] = create_process(1, arrivalprog1, num_lines_program1);
            
            int lb, ub;
            if (calculate_memory_bounds(size_needed1, &lb, &ub)) {
                pcbs[0] = create_pcb(1, lb, ub);
                allocate_memory(pcbs[0], "1", size_needed1, lb, ub, programs[0]);
                int initpc = PC_calc(lb,pcbs[0]->PC);
                pcbs[0]->PC=initpc;
                enqueue(&ready_queue, processes[0]);
                program_sizes[0] = num_lines_program1;
                PCB *pcb = pcbs[0];
                pcb->State = READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 1);
                printf("Failed to allocate memory for process %d\n", 1);
                return;
            }
        }

        } 

        if (cycle == arrivalprog2) {
            if (strcmp(txtfiles[1], "1") != 0) {
            char *program2_path = txtfiles[1];;
            int num_lines_program2 = read_program_file(program2_path, programs[1]);
            int size_needed2 = num_lines_program2 + 3 + 6;
            processes[1] = create_process(2, arrivalprog2, num_lines_program2);
            int lb, ub;
            if (calculate_memory_bounds(size_needed2, &lb, &ub)) {
                pcbs[1] = create_pcb(2, lb, ub);
                allocate_memory(pcbs[1], "2", size_needed2, lb, ub, programs[1]);
                int initpc = PC_calc(lb,pcbs[1]->PC);
                pcbs[1]->PC=initpc;
                enqueue(&ready_queue, processes[1]);
                program_sizes[1] = num_lines_program2;
                PCB *pcb = pcbs[1];
                pcb->State = READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 2);
                printf("Failed to allocate memory for process %d\n", 2);
                return;
            }
        }
        }

        if (cycle == arrivalprog3) {
            if (strcmp(txtfiles[2], "1") != 0) {
            char *program3_path = txtfiles[2];;
            int num_lines_program3 = read_program_file(program3_path, programs[2]);
            int size_needed3 = num_lines_program3 + 3 + 6;
            processes[2] = create_process(3, arrivalprog3, num_lines_program3);
            int lb, ub;
            if (calculate_memory_bounds(size_needed3, &lb, &ub)) {
                pcbs[2] = create_pcb(3, lb, ub);
                allocate_memory(pcbs[2], "3", size_needed3, lb, ub, programs[2]);
                int initpc = PC_calc(lb,pcbs[2]->PC);
                pcbs[2]->PC=initpc;
                enqueue(&ready_queue, processes[2]);
                program_sizes[2] = num_lines_program3;
                PCB *pcb = pcbs[2];
                pcb->State = READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 3);
                printf("Failed to allocate memory for process %d\n", 3);
                return;
            }
        }
                
        }

        if(cycle==0){
            gui_request_dashboard_refresh();
            // gui_log_message("\nMemory at Cycle %d:\n", cycle);
            // printf("\nMemory at Cycle %d:\n", cycle);
            // print_memory(); 
        }

        // If no running process, pick one from ready queue
        if (running_queue.size == 0 && ready_queue.size > 0) {
            Process *p = dequeue(&ready_queue);
            enqueue(&running_queue, p);
        }

        // Execute one instruction from running process
        if (running_queue.size > 0) {
            Process *p = running_queue.queue[running_queue.front];
            int pid = p->pid;
            PCB *pcb = pcbs[pid - 1];
            int pc = pcb->PC;
            pcb->State = RUNNING;
            update(*pcb, Memory);

            int instruction_index = pc; 
            if (instruction_index <= pcb->memory_upper_bound) {
                if (Memory[instruction_index].Value != NULL) {
                    gui_log_message("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                    printf("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                    methods(Memory[instruction_index].Value,pcb);
                    pcb->PC++;
                    update(*pcb, Memory);
                }
            }

            // Check if process finished
            if (pcb->PC > pcb->memory_upper_bound ) {
                gui_log_message("\nProcess %d has finished execution.\n", pid);
                printf("\nProcess %d has finished execution.\n", pid);
                dequeue(&running_queue);
                pcb->State = TERMINATED;
                update(*pcb, Memory);
                finished++;
            }
        }
        
        // Print memory at each cycle
        // gui_log_message("\nMemory after finishing Cycle %d:\n", cycle);
        // printf("\nMemory after finishing Cycle %d:\n", cycle);
        gui_request_dashboard_refresh();
        // print_memory();  


        cycle++;

    }
}




void execute_MLFQ(int arrival_times[3]) {
    
    initialize_queue(&running_queue);
    initialize_queue(&ready_queue2);
    initialize_queue(&ready_queue3);
    initialize_queue(&ready_queue4);
    initialize_queue(&blocking_queue1);
    initialize_queue(&blocking_queue2);
    initialize_queue(&blocking_queue3);
    initialize_memory();
    initialize_cycle();


    int arrivalprog1 = arrival_times[0];
    int arrivalprog2 = arrival_times[1];
    int arrivalprog3 = arrival_times[2];

    int process_count=0;
    for(int i=0;i<MAX_PROCESSES;i++){
        if (strcmp(txtfiles[i], "1") != 0) {  
            process_count++;
        }
        
    }

    Process *processes[process_count];
    // PCB *pcbs[number_of_processes];
    char *programs[MAX_PROCESSES][MAX_LINES];
    int program_sizes[process_count];
    int x;
  
    int finished = 0;




    initialize_queue(&ready_queue);
 

 
  
    while(finished!=process_count){
    
        if(wait){
            check=1; 
         }
         else{
             check=0;
         }

        if (please_stop_simulation) {
            gui_log_message("SIM_CORE: Stop request detected in main loop. Halting.");
            break; // or return;
        }

          
            while(check){
                if (please_stop_simulation) {
                    gui_log_message("SIM_CORE: Stop request detected in main loop. Halting.");
                    break; // or return;
                }

                if(!wait){
                    check=0;
                }
            }
        if (cycle == arrivalprog1) {
            
            if (strcmp(txtfiles[0], "1") != 0) {
             
            
            char *program1_path = txtfiles[0];
            int num_lines_program1 = read_program_file(program1_path, programs[0]);
            int size_needed1 = num_lines_program1 + 3 + 6;
            processes[0] = create_process(1, arrivalprog1, num_lines_program1);
            int lb, ub;
            if (calculate_memory_bounds(size_needed1, &lb, &ub)) {
                pcbs[0] = create_pcb(1, lb, ub);
                allocate_memory(pcbs[0], "1", size_needed1, lb, ub, programs[0]);
                int initpc = PC_calc(lb,pcbs[0]->PC);
                pcbs[0]->PC=initpc;
                enqueue(&ready_queue, processes[0]);
                program_sizes[0] = num_lines_program1;
                PCB *pcb= pcbs[0];
                pcb->State= READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 1);
                printf("Failed to allocate memory for process %d\n", 1);
                return;
            }
        }
        } 

        if (cycle == arrivalprog2) {
            if (strcmp(txtfiles[1], "1") != 0) {
            char *program2_path = txtfiles[1];
            int num_lines_program2 = read_program_file(program2_path, programs[1]);
            int size_needed2 = num_lines_program2 + 3 + 6;
            processes[1] = create_process(2, arrivalprog2, num_lines_program2);
            int lb, ub;
            if (calculate_memory_bounds(size_needed2, &lb, &ub)) {
                pcbs[1] = create_pcb(2, lb, ub);
                allocate_memory(pcbs[1], "2", size_needed2, lb, ub, programs[1]);
                int initpc = PC_calc(lb,pcbs[1]->PC);
                pcbs[1]->PC=initpc;
                enqueue(&ready_queue, processes[1]);
                program_sizes[1] = num_lines_program2;
                PCB *pcb= pcbs[1];
                pcb->State= READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 2);
                printf("Failed to allocate memory for process %d\n", 2);
                return;
            }
        }
        }

        if (cycle == arrivalprog3) {
            if (strcmp(txtfiles[2], "1") != 0) {
            char *program3_path = txtfiles[2];
            int num_lines_program3 = read_program_file(program3_path, programs[2]);
            int size_needed3 = num_lines_program3 + 3 + 6;
            processes[2] = create_process(3, arrivalprog3, num_lines_program3);
            int lb, ub;
            if (calculate_memory_bounds(size_needed3, &lb, &ub)) {
                pcbs[2] = create_pcb(3, lb, ub);
                allocate_memory(pcbs[2], "3", size_needed3, lb, ub, programs[2]);
                int initpc = PC_calc(lb,pcbs[2]->PC);
                pcbs[2]->PC=initpc;
                enqueue(&ready_queue, processes[2]);
                program_sizes[2] = num_lines_program3;
                PCB *pcb= pcbs[2];
                pcb->State= READY;
                update(*pcb, Memory);
            } else {
                gui_log_message("Failed to allocate memory for process %d\n", 3);
                printf("Failed to allocate memory for process %d\n", 3);
                return;
            }
        }
        }

        if(cycle==0){
            gui_request_dashboard_refresh();
            // gui_log_message("\nMemory at Cycle %d before starting:\n", cycle);
            // printf("\nMemory at Cycle %d before starting:\n", cycle);
            // print_memory(); 
        }

        if(running_queue.size==0 && ready_queue.size>0){
            Process *p = dequeue(&ready_queue);
            enqueue(&running_queue, p);
            x=0;
        }
        if(running_queue.size==0 && ready_queue.size==0 && ready_queue2.size>0){
            Process *p = dequeue(&ready_queue2);
            enqueue(&running_queue, p);
            x=0;
        }
        if(running_queue.size==0 && ready_queue.size==0 && ready_queue2.size==0 && ready_queue3.size>0){
            Process *p = dequeue(&ready_queue3);
            enqueue(&running_queue, p);
            x=0;
        }
        if(running_queue.size==0 && ready_queue.size==0 && ready_queue2.size==0 && ready_queue3.size==0 && ready_queue4.size>0){
            Process *p = dequeue(&ready_queue4);
            enqueue(&running_queue, p);
            x=0;
        }


        if(running_queue.size!=0){
            Process *p = running_queue.queue[running_queue.front];
            int pid = p->pid;
            PCB *pcb = pcbs[pid - 1];
            int pc = pcb->PC;
            int priority= pcb->current_priority;
            pcb->State=RUNNING;
            update(*pcb,Memory);
            if(priority==1){

                 

                   
                if (x!=1){
                    int instruction_index =  pc; 
                    if (instruction_index <= pcb->memory_upper_bound) {
                        if (Memory[instruction_index].Value != NULL) {
                            gui_log_message("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            printf("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            methods(Memory[instruction_index].Value,pcb);
                            pcb->PC++;
                            update(*pcb,Memory);
                        }
                    }
                    
                    x++;
                

                    if (pcb->PC > pcb->memory_upper_bound) {
                        gui_log_message("\nProcess %d has finished execution.\n", pid);
                        printf("\nProcess %d has finished execution.\n", pid);
                        x=0;
                        pcb->State= TERMINATED;
                        update(*pcb,Memory);
                        dequeue(&running_queue);
                        finished++;
                    }
                    
                    else if(x==1){
                        pcb->current_priority=2;
                        if(!isInBlockingQueue1(p) && !isInBlockingQueue2(p) && !isInBlockingQueue3(p)){
                         gui_log_message("\nQuantum expired for Process %d at Priority %d. Demoting to Priority %d\n", pid, 1, 2);
                         printf("\nQuantum expired for Process %d at Priority %d. Demoting to Priority %d\n", pid, 1, 2);
                         pcb->State= READY;
                         update(*pcb,Memory);
                        dequeue(&running_queue);
                        enqueue(&ready_queue2,p);
                        
                        }
                        
                        x=0;
                }
                
            }
        
        }
            else if(priority==2){
               
                if (x!=2){
                    int instruction_index =  pc; 
                    if (instruction_index <= pcb->memory_upper_bound) {
                        if (Memory[instruction_index].Value != NULL) {
                            gui_log_message("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            printf("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            methods(Memory[instruction_index].Value,pcb);
                            pcb->PC++;
                            update(*pcb,Memory);
                        }
                    }
                    
                    x++;
                    
                    if (pcb->PC >  pcb->memory_upper_bound) {
                        gui_log_message("\nProcess %d has finished execution.\n", pid);
                        printf("\nProcess %d has finished execution.\n", pid);
                        x=0;
                        pcb->State= TERMINATED;
                        dequeue(&running_queue);
                        update(*pcb,Memory);
                        finished++;
                    }
                    
                    else if(x==2){
                        pcb->current_priority=3;
                       if(!isInBlockingQueue1(p) && !isInBlockingQueue2(p) && !isInBlockingQueue3(p)){
                       dequeue(&running_queue);
                       gui_log_message("\nQuantum expired for Process %d at Priority %d. Demoting to Priority %d\n", pid, 2, 3);
                       printf("\nQuantum expired for Process %d at Priority %d. Demoting to Priority %d\n", pid, 2, 3);
                       pcb->State= READY;
                       update(*pcb,Memory);
                       enqueue(&ready_queue3,p);
                       
                       }
                        x=0;
                     } 
                     
                }
             
            }
            else if(priority==3){
               
                if (x!=4){
                    int instruction_index =  pc; 
                    if (instruction_index <= pcb->memory_upper_bound) {
                        if (Memory[instruction_index].Value != NULL) {
                            gui_log_message("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            printf("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            methods(Memory[instruction_index].Value,pcb);
                            pcb->PC++;
                            update(*pcb,Memory);
                        }
                    }
                    
                      x++;
                  
                    if (pcb->PC > pcb->memory_upper_bound) {
                        gui_log_message("\nProcess %d has finished execution.\n", pid);
                        printf("\nProcess %d has finished execution.\n", pid);
                        x=0;
                        dequeue(&running_queue);
                        pcb->State= TERMINATED;
                        update(*pcb,Memory);
                        finished++;
                    }
                   
                    else if (x==4){
                        pcb->current_priority=4;
                        if(!isInBlockingQueue1(p) && !isInBlockingQueue2(p) && !isInBlockingQueue3(p)){
                        dequeue(&running_queue);
                        gui_log_message("\nQuantum expired for Process %d at Priority %d. Demoting to Priority %d\n", pid, 3, 4);
                        printf("\nQuantum expired for Process %d at Priority %d. Demoting to Priority %d\n", pid, 3, 4);
                        pcb->State= READY;
                        update(*pcb,Memory);
                        enqueue(&ready_queue4,p);
                        
                        }
                        x=0;
                    }
                    
                }
               
            }
            else if(priority==4){
               
                if (x!=8){
                    int instruction_index =  pc; 
                    if (instruction_index <= pcb->memory_upper_bound) {
                        if (Memory[instruction_index].Value != NULL) {
                            gui_log_message("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            printf("\nCycle %d: Executing process %d instruction: %s\n", cycle, pid, Memory[instruction_index].Value);
                            methods(Memory[instruction_index].Value,pcb);
                            pcb->PC++;
                            update(*pcb,Memory);
                        }
                    }
                    
                    x++;
                    
                    if (pcb->PC > pcb->memory_upper_bound) {
                        gui_log_message("\nProcess %d has finished execution.\n", pid);
                        printf("\nProcess %d has finished execution.\n", pid);
                        x=0;
                        dequeue(&running_queue);
                        pcb->State= TERMINATED;
                        update(*pcb,Memory);
                        finished++;
                    }
                    
                    else if (x==8){
                        if(!isInBlockingQueue1(p) && !isInBlockingQueue2(p) && !isInBlockingQueue3(p)){
                        dequeue(&running_queue);
                        gui_log_message("\nQuantum expired for process %d, moving it back to ready queue.\n", pid);
                        printf("\nQuantum expired for process %d, moving it back to ready queue.\n", pid);
                        enqueue(&ready_queue4,p);
                       
                        }
                        x=0;
                    }
                    
                
    
                }
            }
        
       }
       
       
      
       // Print memory at each cycle
       gui_request_dashboard_refresh();
    //    gui_log_message("\nMemory after finishing Cycle %d:\n", cycle);
    //    printf("\nMemory after finishing Cycle %d:\n", cycle);
    //    print_memory();
       cycle++;

    }
}



// int main() {
    
//     int quantum;
//     int scheduler_type;
//     int arrival_times[3];

//     // Input scheduler type
//     printf("Enter scheduler type [1 for RR , 2 for FCFS , 3 for MLFQ]:\n");
//     scanf("%d", &scheduler_type);

    
    

//     // Input arrival times
//     for (int i = 0; i < 3; i++) {
//         printf("Enter arrival time for program %d:", i + 1);
//         scanf("%d", &arrival_times[i]);
//     }


//     if (scheduler_type == 1) {
//         printf("Enter quantum time for RR Scheduling algorithm:\n");
//         scanf("%d", &quantum);
//         schedule_number=1;
//         execute_RR(arrival_times,quantum);
//     } else if (scheduler_type == 2) {
//         schedule_number=2;
//         execute_FCFS(arrival_times);  
//     } else if (scheduler_type == 3) {
//         schedule_number=3;
//         execute_MLFQ(arrival_times);
//     } else {
//         printf("Invalid scheduler type.\n");
//     }
    
//     return 0;
// }