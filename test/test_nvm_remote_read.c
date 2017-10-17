#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "../src/lib/pmalloc.h"
#include "../src/lib/pflush.h"
#ifdef __i386__
#include <emmintrin.h>
#else
#ifdef __amd64__
#include <emmintrin.h>
#endif
#endif


#define _mm_clflush(addr)\
        asm volatile("clflush %0" : "+m" (*(volatile char *)(addr)))
#define _mm_clflushopt(addr)\
        asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(addr)))
#define _mm_clwb(addr)\
        asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(addr)))
#define _mm_pcommit()\
        asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8")


#define BUF_SIZE (1000)
#define CACHELINE_SIZE 64
static inline void PERSISTENT_BARRIER(void)
{   
    asm volatile ("sfence\n" : : );
}

static inline void persistent(void *buf, int len, int fence)
{   
    int i;
    len = len + ((unsigned long)(buf) & (CACHELINE_SIZE - 1));
    int support_clwb = 0;
    
    if (support_clwb) { 
        for (i = 0; i < len; i += CACHELINE_SIZE)
                pflush(buf + i);
    } else {
        for (i = 0; i < len; i += CACHELINE_SIZE)
                pflush(buf + i);
    }
    //if (fence)
        //printf("Reach fence");
	//PERSISTENT_BARRIER();
}

unsigned long **mem;


void NVM_test()
{ 
    clock_t start_time, end_time;
    double duration, rand_latency;
    start_time = clock();
    int i;
    int j;
    unsigned long k, temp;
    start_time = clock();
    mem = (unsigned long **) pmalloc(BUF_SIZE * sizeof(unsigned long *));

    for (i=0; i < BUF_SIZE; ++i) {
        mem[i] = (unsigned long *) pmalloc(BUF_SIZE * sizeof(unsigned long));
        
    }
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf( "Time for NVM Allocation is %f seconds\n ", duration );

    /*test the rand() function costs*/
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            int a, b;
            a = rand()%BUF_SIZE;
            b = rand()%BUF_SIZE;
        }
    } 
    end_time = clock();
    rand_latency = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            int a, b;
            a = rand()%BUF_SIZE;
            b = rand()%BUF_SIZE;
            mem[a][b] = a * b;
     	    pflush(&(mem[a][b]));
        /* __asm__ __volatile__("");*/
        }
    }   
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC - rand_latency;
    printf( "Time for NVM Writing is %f seconds\n ", duration );

    /*make sure every elements in the array have initilized*/
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            mem[i][j] = i * j;
            pflush(&(mem[i][j]));
        }
    } 
    
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            int a,b;
            a = rand() % BUF_SIZE;
            b = rand() % BUF_SIZE;
            temp = mem[a][b];
            }
        }   
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC - rand_latency;
    printf( "Time for NVM Reading is %f seconds\n ", duration );

    k = 0;
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        __asm__ __volatile__("");
       int a, b;
        for (j=0; j < BUF_SIZE; ++j) {
            a = rand() % BUF_SIZE;
            b = rand() % BUF_SIZE;
            mem[a][b] = a*b; 
        }
    }   
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf( "Time for NVＭ W/R with ordering is %f seconds\n ", duration );
    
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
            pfree(mem[i], BUF_SIZE * sizeof(unsigned long));
    }       
    pfree(mem, BUF_SIZE * sizeof(unsigned long *));
    end_time = clock(); 
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf( "Time for NVＭ deallocation is %f seconds\n ", duration );
} 

void DRAM_test()
{
    clock_t start_time, end_time;
    double duration, rand_latency;
    start_time = clock();
    int i;
    int j;
    unsigned long k, temp;
    start_time = clock();
    mem = (unsigned long **) malloc(BUF_SIZE * sizeof(unsigned long *));

    for (i=0; i < BUF_SIZE; ++i) {
        mem[i] = (unsigned long *) malloc(BUF_SIZE * sizeof(unsigned long));
        
    }
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf( "Time for DRAM Allocation is %f seconds\n ", duration );

    /*test the rand() function costs*/
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            rand()%BUF_SIZE;
            rand()%BUF_SIZE;
        }
    } 
    end_time = clock();
    rand_latency = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            int a, b;
            a = rand()%BUF_SIZE;
            b = rand()%BUF_SIZE;
            mem[a][b] = a * b;
            persistent(&(mem[a][b]), sizeof(long), 1);
        }
    }   
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC - rand_latency;
    printf( "Time for DRAM Writing is %f seconds\n ", duration );

    /*make sure every elements in the array have initilized*/
    for (i=0; i < BUF_SIZE; ++i) {
        for (j=0; j < BUF_SIZE; ++j) {
            mem[i][j] = i * j;
            persistent(&(mem[i][j]), sizeof(long), 1);
        }
    } 
    
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        /* __asm__ __volatile__("");*/
        for (j=0; j < BUF_SIZE; ++j) {
            int a,b;
            a = rand() % BUF_SIZE;
            b = rand() % BUF_SIZE;
            temp = mem[a][b];
            }
        }   
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC - rand_latency;
    printf( "Time for DRAM Reading is %f seconds\n ", duration );

    k = 0;
    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
        __asm__ __volatile__("");
        for (j=0; j < BUF_SIZE; ++j) {
        k += mem[j][i] + i*j;
        mem[j][i] = k;
        }
    }
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf( "Time for DRAM W/R with ordering is %f seconds\n ", duration );

    start_time = clock();
    for (i=0; i < BUF_SIZE; ++i) {
            free(mem[i]);
    }
    free(mem);
    end_time = clock();
    duration = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf( "Time for DRAM deallocation is %f seconds\n ", duration );
}

int main()
{
    //DRAM_test();
    NVM_test();
    return 0;
}
