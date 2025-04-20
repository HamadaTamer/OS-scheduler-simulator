#include "sim.h"
#include <stdio.h>

int main(){
    struct program list[3] = {
      {"Program_1.txt",0,0},
      {"Program_2.txt",0,2},
      {"Program_3.txt",0,4}
    };
    sim_reset();
    sim_init(list,3,FCFS,2);

    SimSnapshot snap;
    for(int tick=0; tick<20; tick++){
        if(!sim_step(&snap)){
            printf("All done at tick %d\n",tick);
            break;
        }
        printf("Tick %d: ran %d procs\n", snap.clock, snap.procs_total);
    }
    return 0;
}
