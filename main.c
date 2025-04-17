#include "utilities.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


/*
    create memory to store within it the code of a program, variables, PCB
    the memory will be divided into words where each word can be a PCB entry, program instructions, or variables in the form of a single name and the corresponding data ex: 
        
        state: "ready"
        (variableName_dataType): (value) 
        instruction: (some instruction) (unparsed) 

    now each word should be how many bytes?? i suggest 30 bytes


    Memory layout (60 words ):
        Instructions (variable but is currently 7-9)  (i suggest we add an END instruction/ indicator after the instructions to indicate that the program is DONE)
        PCB (5 values)
        Variables ( we only need space for 3 variables )

        since we have 3 programs we have to have 20 words/program

    the memory will be in the form of allocating using calloc the required number of words each of which with the wanted size

    
    the struct will contain the following:
        1. the program instructions in an array or sth 
        2. the PCB inside the struct:  
            Process ID (The process is given an ID when is being created)
            Process State
            Current Priority
            Program Counter
            Memory Boundaries
    
    create 

 */

int MemorySize = 3;

static int PCBID = 0; // a global counter of Process IDs in order to be tracked globally in other words ID to be used by next process initiated   

struct MemoryWord *Memory_start_location;

// structs defining the memoryWord layout

typedef enum{
    NEW,
    READY, 
    RUNNING,
    WAITING,
    TERMINATED
} process_state;

typedef enum {
    EXTRA_NONE,
    EXTRA_INT,
    EXTRA_STRING
} variable_type_t;


// a union is a structure that can hold either of 2 values and not both
typedef union {
    int   int_val;
    char *str_val;
} variable_data;

struct MemoryWord{
    char identifier[20];  
    char arg1[20];
    char arg2[20];
    variable_type_t var_data_type ;
    variable_data var_data;
};

struct MemoryWord * createMemory(){

    
    // creating program memory
    struct MemoryWord *arr = calloc(60, sizeof *arr);
    Memory_start_location = arr;
    if (!arr){
        perror("failed to create memory ");
        EXIT_FAILURE;
    }

    return arr;
}


// parsing instructions into the memory and ending the instructions with an EOI identifier
void parseProgram(char *filename, struct MemoryWord *memory) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[100];
    int index = 0;

    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;

        char *cmd = strtok(trimmed, " \n");
        if (!cmd) continue;

        strcpy(memory[index].identifier, cmd);

        if (strcmp(cmd, "print") == 0 || strcmp(cmd, "readFile") == 0 || strcmp(cmd, "semWait") == 0 || strcmp(cmd, "semSignal") == 0) {
            char *arg = strtok(NULL, " \n");
            if (!arg) {
                fprintf(stderr, "Error: '%s' missing argument in line: %s\n", cmd, line);
                exit(EXIT_FAILURE);
            }
            strcpy(memory[index].arg1, arg);
        }
        else if (strcmp(cmd, "assign") == 0 || strcmp(cmd, "writeFile") == 0 || strcmp(cmd, "printFromTo") == 0) {
            char *arg1 = strtok(NULL, " \n");
            char *arg2 = strtok(NULL, " \n");

            if (!arg1 || !arg2) {
                fprintf(stderr, "Error: '%s' missing arguments in line: %s\n", cmd, line);
                exit(EXIT_FAILURE);
            }

            strcpy(memory[index].arg1, arg1);
            strcpy(memory[index].arg2, arg2);
        }
        else {
            fprintf(stderr, "Error: Unknown instruction '%s'\n", cmd);
            exit(EXIT_FAILURE);
        }

        index++;
        if (index >= 60) {
            fprintf(stderr, "Error: Memory overflow, too many instructions\n");
            exit(EXIT_FAILURE);
        }
    }

    strcpy(memory[index].identifier, "EOI");

    fclose(file);
}


// creating the PCB

void createPCB(struct MemoryWord *memory, int priority){

    /*
    PCB is made up of 5 consecutive words for the following attributes:
        1.Process ID (depends on the global counter)
        2. Process state (states are: new, ready, running, waiting (blocked), terminated )
        3. current priority (initialzed with supplied memory address)
        4. Program counter (initialized with supplied memory address )
        5. Memory boundaries // fixed at supplied memory location for start, and supplied mem location + number of instructions + 1 (EOI) + 5 (PCB) + (number of variables)
    */
    

    int i = 0;

    // finding EOI
    while (strcmp(memory[i].identifier, "EOI") != 0) i++;
    i++; // move to start of PCB area

    // Process ID
    strcpy(memory[i].identifier, "ID");
    sprintf(memory[i].arg1, "%d", PCBID++);
    
    // Process State
    i++;
    strcpy(memory[i].identifier, "State");
    sprintf(memory[i].arg1, "%d", NEW);
    
    // Priority
    i++;
    strcpy(memory[i].identifier, "Current_priority");
    sprintf(memory[i].arg1, "%d", priority);
    
    // Program Counter
    i++;
    strcpy(memory[i].identifier, "Program_counter");
    sprintf(memory[i].arg1, "%d", 0);
    
    // Memory Boundaries
    i++;
    strcpy(memory[i].identifier, "Memory_Bounds");
    sprintf(memory[i].arg1, "%d",memory - Memory_start_location );           // Lower bound 
    sprintf(memory[i].arg2, "%d", memory + i + 3 - Memory_start_location);       // Upper bound
}


int main() {
    // Step 1: Create the memory
    struct MemoryWord* memory = createMemory();
    if (!memory) {
        printf("Memory creation failed!\n");
        return 1;
    }

    // Step 2: Call parseProgram() on a test file
    // Make sure test_program.txt exists with test instructions
    parseProgram("Program_1.txt", memory);

    // Step 3: Print out memory contents to verify
    printf("Parsed Memory Contents:\n");
    for (int i = 0; i < 20; i++) {
        //if (strlen(memory[i].identifier) == 0) break; // Stop at empty slot
        printf("Memory[%d]: identifier = %s, arg1 = %s, arg2 = %s\n", i, memory[i].identifier, memory[i].arg1, memory[i].arg2);
    }

   
        // Step 3: Create the PCB
        printf("Creating PCB for the parsed program...\n");
        int testPriority = 2;
        createPCB(memory, testPriority);
    
        // Step 4: Print out the memory contents to verify
        printf("\n===== Memory Dump =====\n");
        for (int i = 0; i < 60; i++) {
            // Only print if something is stored
            if (strlen(memory[i].identifier) == 0) continue;
     
            printf("Memory[%02d]: ", i);
    
            // Label the section
            if (strcmp(memory[i].identifier, "EOI") == 0) {
                printf("== End of Instructions ==\n");
                continue;
            } else if (strcmp(memory[i].identifier, "ID") == 0) {
                printf("[PCB] %s = %s\n", memory[i].identifier, memory[i].arg1);
            } else if (strcmp(memory[i].identifier, "State") == 0 ||
                       strcmp(memory[i].identifier, "Current_priority") == 0 ||
                       strcmp(memory[i].identifier, "Program_counter") == 0) {
                printf("[PCB] %s = %s\n", memory[i].identifier, memory[i].arg1);
            } else if (strcmp(memory[i].identifier, "Memory_Bounds") == 0) {
                printf("[PCB] %s = (%s -> %s)\n", memory[i].identifier, memory[i].arg1, memory[i].arg2);
            } else {
                // Default: print instruction
                printf("[Instr] %s %s %s\n", memory[i].identifier, memory[i].arg1, memory[i].arg2);
            }
        }
    
     
    

    // Step 4: Cleanup
    free(memory);
    return 0;
}

