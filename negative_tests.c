/* negative_tests.c  ───────────────────────────────────────────
   build together with your happy tests:

     gcc -std=c11 -Wall -Wextra -pedantic \
         your_module.c tests.c negative_tests.c -o tests
 */
 #include "main.c" 
 #include <assert.h>
 #include <string.h>
 #include <stdlib.h>
 #include <sys/wait.h>
 #include <unistd.h>

 #include "memory.h"
 
 /* ───────── helper: run a statement in a child and expect it to
                call exit(EXIT_FAILURE) (=> non‑zero status)     */
 #define EXPECT_EXIT_NONZERO(stmt)                                  \
     do {                                                           \
         pid_t _pid = fork();                                       \
         assert(_pid >= 0 && "fork failed");                        \
         if (_pid == 0) {                                           \
             stmt;                                                  \
             /* If we ever reach here the test failed. */           \
             _exit(EXIT_SUCCESS);                                   \
         } else {                                                   \
             int _status;                                           \
             waitpid(_pid, &_status, 0);                            \
             assert(WIFEXITED(_status));                            \
             assert(WEXITSTATUS(_status) != 0 &&                    \
                    "expected non‑zero exit status");               \
         }                                                          \
     } while (0)
 
 /* ──────────────────────────────────────────────────────────────
    1. assignValue fails when there is **no free variable slot**  */
 static void test_assignValue_no_slot(void)
 {
     struct MemoryWord *mem = createMemory();
 
     /* fill all four slots 5‑8 with dummy vars */
     for (int i = 5; i < 9; ++i) {
         snprintf(mem[i].identifier, sizeof mem[i].identifier,
                  "v%d", i);
     }
 
     char buf[] = "assign overflow 99";
     strtok(buf, " \n");                       /* eat "assign" */
     EXPECT_EXIT_NONZERO(assignValue(buf, mem));
 
     free(mem);
 }
 
 /* ──────────────────────────────────────────────────────────────
    2. readfile branch – variable name **not found**              */
 static void test_readfile_var_not_found(void)
 {
     struct MemoryWord *mem = createMemory();
 
     char buf[] = "assign out readfile missingName";
     strtok(buf, " \n");
     EXPECT_EXIT_NONZERO(assignValue(buf, mem));
 
     free(mem);
 }
 
 /* ──────────────────────────────────────────────────────────────
    3. readfile branch – variable holds an **integer**, not string */
 static void test_readfile_var_not_string(void)
 {
     struct MemoryWord *mem = createMemory();
 
     strcpy(mem[5].identifier, "fname");
     strcpy(mem[5].arg1, "123");               /* looks like int */
 
     char buf[] = "assign bad readfile fname";
     strtok(buf, " \n");
     EXPECT_EXIT_NONZERO(assignValue(buf, mem));
 
     free(mem);
 }
 
 /* ──────────────────────────────────────────────────────────────
    4. readfile branch – file **does not exist**                  */
 static void test_readfile_file_missing(void)
 {
     struct MemoryWord *mem = createMemory();
 
     strcpy(mem[5].identifier, "fname");
     strcpy(mem[5].arg1, "definitely‑no‑such‑file.xyz");
 
     char buf[] = "assign content readfile fname";
     strtok(buf, " \n");
     EXPECT_EXIT_NONZERO(assignValue(buf, mem));
 
     free(mem);
 }
 
 /* ──────────────────────────────────────────────────────────────
    5. parseProgram overflows the  60‑word  memory                */
 static void test_parseProgram_overflow(void)
 {
     /* build a temp script with 55 instructions (> 60‑8 = 52) */
     const char *fn = "big.tmp";
     {
         FILE *f = fopen(fn, "w");
         for (int i = 0; i < 55; ++i)
             fprintf(f, "assign x %d\n", i);
         fclose(f);
     }
 
     struct MemoryWord *mem = createMemory();
     EXPECT_EXIT_NONZERO(parseProgram((char*)fn, mem));
 
     remove(fn);
     free(mem);
 }
 
 /* ──────────────────────────────────────────────────────────────
    export a single entry point so you can link all test files
    together.  Call this from main() in your existing tests.c     */
 void run_negative_tests(void)
 {
     test_assignValue_no_slot();
     test_readfile_var_not_found();
     test_readfile_var_not_string();
     test_readfile_file_missing();
     test_parseProgram_overflow();
 }
 int main(){
    run_negative_tests();
 }