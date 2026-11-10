/* Dependent-load latency for a small working set, and a large one. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+t.tv_nsec/1e9;}
static double run(uint8_t *b, size_t n, long iters){
    double t0,t1; volatile uint8_t s=0; size_t i=0;
    for(size_t k=0;k<n;k++) b[k]=(uint8_t)((k*7+1)%n);   /* build a cycle */
    t0=now_s();
    for(long k=0;k<iters;k++){ i = b[i]; s = (uint8_t)(s+i); }
    t1=now_s();
    return (t1-t0)*1e9/iters;
}
int main(void){
    static uint8_t small[3000];                 /* static: same segment as struct */
    uint8_t *big = malloc(1<<20);
    memset(big,0,1<<20);
    printf("dependent load,  3 KB static : %.2f ns\n", run(small, sizeof small, 5000000));
    printf("dependent load, 64 KB heap   : %.2f ns\n", run(big, 65536, 2000000));
    return 0;
}
