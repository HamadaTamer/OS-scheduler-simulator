/* ─────────────────────────────────────────────────────────────
   tests.c  —  happy‑path unit‑tests adapted to the dispatcher
   that consumes the first token with strtok()
   ───────────────────────────────────────────────────────────── */
   #include "main.c"
   #include <assert.h>
   #include <string.h>
   #include <stdio.h>
   #include <stdlib.h>
   
   #include "memory.h"          /* ← put your enum / struct / prototypes here */
   
   /* helper ------------------------------------------------------*/
   extern int PCBID;            /* in your implementation file */
   static void reset_globals(void) { PCBID = 0; }
   
   static void cleanup(struct MemoryWord *mem)
   {
       free(mem);
       reset_globals();
   }
   
   /* ---- 1. createMemory ---------------------------------------*/
   static void test_createMemory_allocates_and_zeroes(void)
   {
       struct MemoryWord *mem = createMemory();
       assert(mem);
   
       for (int i = 0; i < 60; ++i)
           assert(mem[i].identifier[0] == 0 &&
                  mem[i].arg1[0]      == 0 &&
                  mem[i].arg2         == 0);
   
       cleanup(mem);
   }
   
   /* ---- 2. parseProgram ---------------------------------------*/
   static void make_prog_file(const char *fname)
   {
       FILE *f = fopen(fname, "w");
       fputs("assign x 5\n"
             "print x\n", f);
       fclose(f);
   }
   static void test_parseProgram_copies_instructions_and_EOI(void)
   {
       const char *fname = "prog.tmp";
       make_prog_file(fname);
   
       struct MemoryWord *mem = createMemory();
       parseProgram((char*)fname, mem);
   
       assert(strcmp(mem[8].identifier, "assign x 5") == 0);
       assert(strcmp(mem[9].identifier, "print x")   == 0);
       assert(strcmp(mem[10].identifier, "EOI")      == 0);
   
       remove(fname);
       cleanup(mem);
   }
   
   /* ---- 3. createPCB ------------------------------------------*/
   static void test_createPCB_initialises_all_fields(void)
   {
       struct MemoryWord *mem = createMemory();
       strcpy(mem[8].identifier, "dummy");
       strcpy(mem[9].identifier, "EOI");
   
       createPCB(mem, 7);
   
       assert(strcmp(mem[0].identifier, "ID") == 0);
       assert(atoi(mem[0].arg1) == 0);
   
       assert(strcmp(mem[1].identifier, "State") == 0);
       assert(atoi(mem[1].arg1) == NEW);
   
       assert(strcmp(mem[2].identifier, "Current_priority") == 0);
       assert(atoi(mem[2].arg1) == 7);
   
       assert(strcmp(mem[3].identifier, "Program_counter") == 0);
       assert(atoi(mem[3].arg1) == 0);
   
       assert(strcmp(mem[4].identifier, "Memory_Bounds") == 0);
       assert(atoi(mem[4].arg1) == 0);
       assert(mem[4].arg2       == 9);
   
       cleanup(mem);
   }
   
   /* ---- 4. lookupValue ----------------------------------------*/
   static void test_lookupValue_returns_correct_slot(void)
   {
       struct MemoryWord *mem = createMemory();
       strcpy(mem[5].identifier, "a");
       strcpy(mem[6].identifier, "b");
   
       assert(lookupValue(mem, "a") == 5);
       assert(lookupValue(mem, "b") == 6);
       assert(lookupValue(mem, "nope") == -1);
   
       cleanup(mem);
   }
   
   /* ---- 5. assignValue – literals -----------------------------*/
   static void test_assignValue_int_and_string_literal(void)
   {
       struct MemoryWord *mem = createMemory();
   
       /* int literal ------------------------------------------------*/
       char l1[] = "assign foo 42";
       strtok(l1, " \n");                 /* dispatcher consumes "assign" */
       assignValue(l1, mem);              /* handler continues           */
   
       assert(strcmp(mem[5].identifier, "foo") == 0);
       assert(strcmp(mem[5].arg1,       "42")  == 0);
   
       /* string literal ---------------------------------------------*/
       char l2[] = "assign bar \"hello\"";
       strtok(l2, " \n");
       assignValue(l2, mem);
   
       assert(strcmp(mem[6].identifier, "bar")   == 0);
       assert(strcmp(mem[6].arg1,       "hello") == 0);
   
       cleanup(mem);
   }
   
   /* ---- 6. assignValue – readfile path ------------------------*/
   static void test_assignValue_readfile_happy_path(void)
   {
       const char *fn = "file.tmp";
       FILE *f = fopen(fn, "w");
       fputs("line‑1\n", f);
       fclose(f);
   
       struct MemoryWord *mem = createMemory();
       strcpy(mem[5].identifier, "fname");
       strcpy(mem[5].arg1, fn);
   
       char l[] = "assign out readfile fname";
       strtok(l, " \n");                  /* eat "assign" */
       assignValue(l, mem);
   
       assert(strcmp(mem[6].identifier, "out")   == 0);
       assert(strcmp(mem[6].arg1,       "line‑1")== 0);
   
       remove(fn);
       cleanup(mem);
   }
   
   /* ---- 7. assignValue – stdin branch -------------------------*/
   static void test_assignValue_input_path(void)
   {
       struct MemoryWord *mem = createMemory();
   
       char fake_stdin[] = "0\n123\n";    /* 0 = int, value 123 */
       FILE *stub = fmemopen(fake_stdin, sizeof(fake_stdin) - 1, "r");
       FILE *real = stdin;
       stdin = stub;
   
       char l[] = "assign num input";
       strtok(l, " \n");
       assignValue(l, mem);
   
       stdin = real;
       fclose(stub);
   
       assert(strcmp(mem[5].identifier, "num") == 0);
       assert(strcmp(mem[5].arg1,       "123") == 0);
   
       cleanup(mem);
   }
   
   /* ---- 8. execute_an_instruction – PC bump -------------------*/
   static void test_execute_instruction_advances_PC(void)
   {
       struct MemoryWord *mem = createMemory();
       strcpy(mem[8].identifier, "assign foo 1");
       strcpy(mem[9].identifier, "EOI");
   
       createPCB(mem, 1);
       execute_an_instruction(0, mem);
   
       assert(atoi(mem[3].arg1) == 1);
       cleanup(mem);
   }
   
   /* ---- main --------------------------------------------------*/
   int main(void)
   {
       test_createMemory_allocates_and_zeroes();
       test_parseProgram_copies_instructions_and_EOI();
       test_createPCB_initialises_all_fields();
       test_lookupValue_returns_correct_slot();
       test_assignValue_int_and_string_literal();
       test_assignValue_readfile_happy_path();
       test_assignValue_input_path();
       test_execute_instruction_advances_PC();
   
       puts("All happy‑path tests passed ✔");
       return 0;
   }
   