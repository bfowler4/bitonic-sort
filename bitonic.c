/* File:     bitonic.c
 * Author:   Brandon Fowler, cs220 - 01
 *
 * Purpose:  Implements a butterfly-structured bitonic sort
 * 
 * Input:    thread_count: number of threads
 *           n: number of elements
 *           g: optional flag to generate numbers randomly
 *           o: optional flag to print sorted and unsorted list
 * Output:   Sorted list of elements
 *
 * Compile:  gcc -g -Wall -o bitonic bitonic.c -lpthreads
 * Usage:    ./bitonic <thread_count> <n> [g] [o]
 *
 * Notes:
 *     1. Assumes number of threads is a power of 2 and number of elements in
 *        the list is evenly divisble by the number of threads, does not check
 *        this
 *     2. Debug flag can be used to print updated list after each stage of the 
 *        sorting algorithm. Includes information on which butterfly is being
 *        executed and which stage has been completed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "timer.h"

/* Global variables */
int thread_count;
int n;    // number of elements
int* x;    // array to store list
int* temp;    // temporary array to store list throughout stages of butterfly
int isRand = 0;    // determines if [g] was provided in command line   
int toPrint = 0;    // determines if [o] was provided in command line
int barrier_thread_count = 0;    // Useful for synchronization
int stage;    // for debugging only
pthread_mutex_t barrier_mutex;
pthread_cond_t ok_to_proceed;

/* Serial Functions */
void Get_info(int argc, char* argv[]);
void Usage(char prog[]);
void Gen_vector(int x[]);
void Print_vector(int x[]);
void Merge_inc(int A[], int B[], int rank, int partner);
void Merge_dec(int A[], int B[], int rank, int partner);
void Swap_list(int **curr_arr, int **temp_arr);
int cmpfunc(const void * a, const void * b);

/* Parallel function */
void *Thread_work(void* rank);

int main(int argc, char* argv[]) {
    long thread;
    pthread_t* thread_handles;
    double start, finish;

    Get_info(argc, argv);

    thread_handles = malloc(thread_count*sizeof(pthread_t));
    pthread_mutex_init(&barrier_mutex, NULL);
    pthread_cond_init(&ok_to_proceed, NULL);

    GET_TIME(start);
    for (thread = 0; thread < thread_count; thread++)
        pthread_create(&thread_handles[thread], (pthread_attr_t*) NULL,
            Thread_work, (void*) thread);

    for (thread = 0; thread < thread_count; thread++) 
        pthread_join(thread_handles[thread], NULL);
    GET_TIME(finish);
    
    if (toPrint) {
        printf("Sorted list is: ");
        Print_vector(x);
    }
    printf("Elapsed time = %e seconds\n", finish - start);


    pthread_mutex_destroy(&barrier_mutex);
    pthread_cond_destroy(&ok_to_proceed);
    free(thread_handles);
    free(x);
    free(temp);
    return 0;
}  /* main */

/*-------------------------------------------------------------------
 * Function:    Get_info
 * Purpose:     Get the input values from command line
 * Input args:  argc:  number of command line args
 *              argv:  array of command line args
 * Output args: thread_count: number of threads
 *              n: number of elements
 *              x: list of elements
 *              isRand: determines if numbers are generated randomly
 *              toPrint: determines if sorted and unsorted lists are printed
 *
 */
void Get_info(int argc, char* argv[]) {
    if (argc < 3) Usage(argv[0]);
    if (argc > 5) Usage(argv[0]);

    thread_count = strtol(argv[1], NULL, 10);
    n = strtol(argv[2], NULL, 10);
    x = malloc(n*sizeof(int));
    temp = malloc(n*sizeof(int));

    if (argc == 4) {
        if (strcmp(argv[3], "g") == 0)
            isRand = 1;
        if (strcmp(argv[3], "o") == 0)
            toPrint = 1;
    }

    if (argc == 5) {
        if (strcmp(argv[3], "g") == 0)
            isRand = 1;
        if (strcmp(argv[3], "o") == 0)
            toPrint = 1;
        if (strcmp(argv[4], "g") == 0)
            isRand = 1;
        if (strcmp(argv[4], "o") == 0)
            toPrint = 1;
    }

    Gen_vector(x);
}  /* Get_info */

/*-------------------------------------------------------------------
 * Function:  Usage
 * Purpose:   Print a brief message explaining how the program is run.
 *            The quit.
 * In arg:    prog:  name of the executable
 */
void Usage(char prog[]) {
    fprintf(stderr, "usage: %s <thread_count> <n> [g] [o]\n", prog);
    fprintf(stderr, "   thread_count = number of threads\n");
    fprintf(stderr, "   n = number of elements in list\n");
    fprintf(stderr, "   g = optional flag to generate numbers randomly\n");
    fprintf(stderr, "   o = optional flag to print unsorted and sorted list\n");
    exit(0);
}  /* Usage */

/*------------------------------------------------------------------
 * Function: Gen_vector
 * Purpose:  Generates vector randomly if [g] was provided on the command line
 *           or reads in the vector from stdin
 * Out arg:  x
 */
void Gen_vector(int x[]) {
    int i;

    if (isRand) {
        srandom(1);
        for (i = 0; i < n; i++)
            x[i] = random() % 999999;
    }
    else {
        for (i = 0; i < n; i ++)
            scanf("%d", &x[i]);
    }

    if (toPrint) {
        printf("Unsorted list is: ");
        Print_vector(x);
    }
}  /* Gen_vector */

/*------------------------------------------------------------------
 * Function:    Print_vector
 * Purpose:     Prints vector
 * In args:     
 */
void Print_vector(int x[]) {
    int i;

    for (i = 0; i < n; i++)
        printf("%d ", x[i]);
    printf("\n");
}  /* Print_vector */

/*-------------------------------------------------------------------
 * Function:   Merge_inc
 * Purpose:    Merges the thread's assigned section of the array with its
 *             partner's section into B in increasing order (left to right)
 * Input args: rank: thread rank
 *             partner: partner's rank
 *             A: the array
 * Output arg: B: result array
 */
void Merge_inc(int A[], int B[], int rank, int partner) {
    int my_first = rank * (n / thread_count);  // thread's start point
    int my_last = (((rank + 1) * n) / (thread_count)) - 1;  // thread's end
    int p_first = partner * (n / thread_count);  // partner's start point
    int p_last = (((partner + 1) * n) / (thread_count)) - 1;  // partner's end
    int i = my_first;
    int j = p_first;
   
    // Increasing order so function moves from left to right and compares using 
    // less than
    while (i <= my_last) {
        if (A[my_first] <= A[p_first]) {
            B[i] = A[my_first];
            i ++; my_first ++;
        } else {
            B[i] = A[p_first];
            i ++; p_first ++;
        }
    }

    while (j <= p_last && my_first <= my_last && p_first <= p_last) {
        if (A[my_first] <= A[p_first]) {
            B[j] = A[my_first];
            j ++; my_first ++;
        } else {
            B[j] = A[p_first];
            j ++; p_first ++;
        }
    }

   if (my_first > my_last)
      for (; j <= p_last; j ++, p_first ++)
         B[j] = A[p_first];
   else
      for (; j <= p_last; j ++, my_first ++)
         B[j] = A[my_first];
}  /* Merge_inc */

/*-------------------------------------------------------------------
 * Function:   Merge_dec
 * Purpose:    Merges the thread's assigned section of the array with its
 *             partner's section into B in decreasing order (right to left)
 * Input args: rank: thread rank
 *             partner: partner's rank
 *             A: the array
 * Output arg: B: result array
 */
void Merge_dec(int A[], int B[], int rank, int partner) {
    int my_first = (((rank + 1) * n) / (thread_count)) - 1;  //thread's start
    int my_last = rank * (n / thread_count);  // thread's end
    int p_first = (((partner + 1) * n) / (thread_count)) - 1;  // partner start
    int p_last = partner * (n / thread_count);  // partner end
    int i = my_first;
    int j = p_first;
   
    // Decreasing order so function moves from right to left and uses greater
    // than
    while (i >= my_last) {
        if (A[my_first] >= A[p_first]) {
            B[i] = A[my_first];
            i --; my_first --;
        } else {
            B[i] = A[p_first];
            i --; p_first --;
        }
    }

    while (j >= p_last && my_first >= my_last && p_first >= p_last) {
        if (A[my_first] >= A[p_first]) {
            B[j] = A[my_first];
            j --; my_first --;
        } else {
            B[j] = A[p_first];
            j --; p_first --;
        }
    }

   if (my_first < my_last)
      for (; j >= p_last; j --, p_first --)
         B[j] = A[p_first];
   else
      for (; j >= p_last; j --, my_first --)
         B[j] = A[my_first];
}  /* Merge_dec */

/*-------------------------------------------------------------------
 * Function: Swap_list
 * Purpose:  Swaps pointers for array
 * In/Out args: On input arrays are original arrays. On output, array pointers
 *              are swapped (curr becomes temp, temp becomes curr)  
 *               
 */
void Swap_list(int **curr_arr, int **temp_arr) {   
   int *c = *curr_arr; // create pointer to curr_arr;
   int *t = *temp_arr; // create pointer to temp_arr;

   *curr_arr = t;
   *temp_arr = c;
}  /* Swap_list */

/*-------------------------------------------------------------------
 * Function:    Thread_work
 * Purpose:     Implement butterfly structured bitonic sort
 * In arg:      rank
 * Global var:  thread_count, barrier_thread_count, barrier_mutex,
 *              ok_to_proceed, n, x, temp, stage (for debug only)
 * Return val:  Ignored
 */
void *Thread_work(void* rank) {
    long my_rank = (long) rank; 
    unsigned bitmask = 1; 
    unsigned bitmask2 = 0;
    unsigned and_bit = 2;
    int partner;

    // Original sort of each thread's section of the array
    qsort(&x[(my_rank * (n / thread_count))], (n / thread_count), sizeof(int), 
        cmpfunc);
    // Lock to make sure every thread is done sorting before proceeding
    pthread_mutex_lock(&barrier_mutex); 
    barrier_thread_count++;
    if (barrier_thread_count == thread_count) {
            #  ifdef DEBUG
                printf("List after local sort is: ");
                Print_vector(x);
            #  endif
        barrier_thread_count = 0;
        pthread_cond_broadcast(&ok_to_proceed);
    } else {
        while (pthread_cond_wait(&ok_to_proceed,
            &barrier_mutex) != 0);
    }
    pthread_mutex_unlock(&barrier_mutex);  

    while (bitmask < thread_count) {
        bitmask2 = bitmask;
        #   ifdef DEBUG
            stage = 1;
        #   endif
        while (bitmask2 > 0) {
            partner = my_rank ^ bitmask2;
            if (my_rank < partner) {  // Determines if thread merges or idle
                if ((my_rank & and_bit) == 0)  // Determine inc or dec merge
                    Merge_inc(x, temp, my_rank, partner);
                else
                    Merge_dec(x, temp, my_rank, partner); 
            }
            // Lock to make sure all threads have finished merging
            pthread_mutex_lock(&barrier_mutex);
            barrier_thread_count++;
            if (barrier_thread_count == thread_count) {
                barrier_thread_count = 0;
                // Swap pointers with x and temp array
                Swap_list(&x, &temp);
                #   ifdef DEBUG
                    printf("List after stage %d of %d-element butterfly is: ", 
                    	stage, and_bit);
                    Print_vector(x);
                    stage ++;
                #   endif
                pthread_cond_broadcast(&ok_to_proceed);
            } else {
                while (pthread_cond_wait(&ok_to_proceed,
                    &barrier_mutex) != 0);
            }
            pthread_mutex_unlock(&barrier_mutex);
            bitmask2 >>= 1;
        }
        bitmask <<= 1;
        and_bit <<= 1;
    }

    return NULL;
}  /* Thread_work */

/*-------------------------------------------------------------------
 * Function: cmpfunc
 * Purpose:  c library qsort helper function
 */
int cmpfunc(const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}  /* cmpfunc */ 