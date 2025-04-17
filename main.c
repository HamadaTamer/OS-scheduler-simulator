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
    for (int i = 0; i < 60; i++) {
        //if (strlen(memory[i].identifier) == 0) break; // Stop at empty slot
        printf("Memory[%d]: identifier = %s, arg1 = %s, arg2 = %s\n", i, memory[i].identifier, memory[i].arg1, memory[i].arg2);
    }

    // Step 4: Cleanup
    free(memory);
    return 0;
}

