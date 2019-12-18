//NAME: Michael Bustillos
//EMAIL: mdbust24@icloud.com
//ID: 304929353

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "SortedList.h"

int num_threads;
int num_iterations;
char sync_option;
int sub_list;
int opt_yield;
int yieldFlag;

SortedList_t* head;
SortedListElement_t* elements;
SortedListElement_t** sub_heads = NULL; 
long long* wait_for_lock;

int* sub_spin_lock = NULL;
volatile int spin_lock = 0;

pthread_mutex_t* sub_mut_lock = NULL;
pthread_mutex_t mut_lock;


void catch_segfault()
{
    fprintf(stderr, "Segmentation fault detected");
    exit(2);
}

long hash_func(const char *ref)
{
    unsigned long hash = 5381;
    int i;    
    while ((i = *ref++))
        hash = ((hash << 5) + hash) + i;
    
    return hash;
}

/*
 We use this function to test our parallel work and time how long we wait on our locks. We will insert elements, get
 the length of each doubly-linked list, and then delete each element from the list(s). We sum up the total time we
 wait as we progress through the function
*/
void * list_work (void* arg)
{
    // look for correct spot in elements array to do insert into lists
    long start_point = (long) (arg);
    long start_ref = num_iterations * start_point;
		
    // INSERTION
    for (long i = start_ref; i < start_ref + num_iterations; i++) {
        struct timespec start, end;
        if (sub_list == 1) {
            if (sync_option == 'm') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                pthread_mutex_lock(&mut_lock);
                clock_gettime(CLOCK_MONOTONIC, &end);
                            
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                SortedList_insert(head, &elements[i]);
                pthread_mutex_unlock(&mut_lock);
            } else if (sync_option == 's') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                while (__sync_lock_test_and_set(&spin_lock, 1));
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                SortedList_insert(head, &elements[i]);
                __sync_lock_release(&spin_lock);
            }
        } else {
            const char * hash_key = elements[i].key;
            unsigned long hash_num = hash_func(hash_key)% sub_list;

            if (sync_option == 'm') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                pthread_mutex_lock(&sub_mut_lock[hash_num]);
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                SortedList_insert(sub_heads[hash_num], &elements[i]);
                pthread_mutex_unlock(&sub_mut_lock[hash_num]);
            } else if (sync_option == 's') {
                //spinLock
                clock_gettime(CLOCK_MONOTONIC, &start);
                while (__sync_lock_test_and_set(&sub_spin_lock[hash_num], 1)) //acquire that specific lock for that sublist
                    ;//spin
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                SortedList_insert(sub_heads[hash_num], &elements[i]);
                __sync_lock_release(&sub_spin_lock[hash_num]);
            }
        }
    }
    
    // GET LENGTH
    long length = 0;
    if (sub_list == 1) {
        struct timespec start, end;
        
        if (sync_option == 'm') {
            clock_gettime(CLOCK_MONOTONIC, &start);
            pthread_mutex_lock(&mut_lock);
            clock_gettime(CLOCK_MONOTONIC, &end);
            
            long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
            wait_for_lock[start_point] += wait_time;
            
            length = SortedList_length(head);
            pthread_mutex_unlock(&mut_lock);
        } else if (sync_option == 's') {
            clock_gettime(CLOCK_MONOTONIC, &start);
            while (__sync_lock_test_and_set(&spin_lock, 1));
            clock_gettime(CLOCK_MONOTONIC, &end);
            
            long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
            wait_for_lock[start_point] += wait_time;
            
            length = SortedList_length(head);
            __sync_lock_release(&spin_lock);
        }
    } else {
        struct timespec start, end;
        //now we have to iterate through all of the sublists
        for (int j = 0; j < sub_list; j++) {
            if (sync_option == 'm') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                pthread_mutex_lock(&sub_mut_lock[j]); //lock the jth list
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                length += SortedList_length(sub_heads[j]);
                pthread_mutex_unlock(&sub_mut_lock[j]);
            } else if (sync_option == 's') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                while (__sync_lock_test_and_set(&sub_spin_lock[j], 1)) //acquire that specific lock for that sublist
                    ;//spin
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                length += SortedList_length(sub_heads[j]);
                __sync_lock_release(&sub_spin_lock[j]);
            }
        }
    }
    
    // DELETION
    start_ref = num_iterations * start_point;
    for (long i = start_ref; i< start_ref + num_iterations; i++) {
        struct timespec start, end;
        if (sub_list == 1) {
            if (sync_option == 'm') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                pthread_mutex_lock(&mut_lock);
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                SortedListElement_t * find = SortedList_lookup(head, elements[i].key); //look up the value
                if (find == NULL) {
                    fprintf(stderr, "Error with lookup\n");
                    exit(EXIT_FAILURE);
                }
                SortedList_delete(find);
                pthread_mutex_unlock(&mut_lock);
            } else if (sync_option == 's') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                while (__sync_lock_test_and_set(&spin_lock, 1));
                clock_gettime(CLOCK_MONOTONIC, &end);
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                
                SortedListElement_t * find = SortedList_lookup(head, elements[i].key); //look up the value
                if (find == NULL) {
                    fprintf(stderr, "Error with lookup\n");
                    exit(EXIT_FAILURE);
                }
                SortedList_delete(find);
                __sync_lock_release(&spin_lock);
            }
        } else {
            unsigned long indexList = hash_func(elements[i].key)% sub_list;
            if (sync_option == 'm') {
                pthread_mutex_lock(&sub_mut_lock[indexList]);
                
                SortedListElement_t * find = SortedList_lookup(sub_heads[indexList], elements[i].key);
                if (find == NULL) {
                    fprintf(stderr, "Error with lookup, exiting\n");
                    exit(EXIT_FAILURE);
                }
                
                SortedList_delete(find);
                pthread_mutex_unlock(&sub_mut_lock[indexList]);
            } else if (sync_option == 's') {
                clock_gettime(CLOCK_MONOTONIC, &start);
                while (__sync_lock_test_and_set(&sub_spin_lock[indexList], 1));
                clock_gettime(CLOCK_MONOTONIC, &end);
                
                long long wait_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
                wait_for_lock[start_point] += wait_time;
                
                SortedListElement_t * find = SortedList_lookup(sub_heads[indexList], elements[i].key);
                if (find == NULL) {
                    fprintf(stderr, "Error with lookup\n");
                    exit(EXIT_FAILURE);
                }
                
                SortedList_delete(find);
                __sync_lock_release(&sub_spin_lock[indexList]);
            }
        }
    }
    return NULL;
}

/*
Program aimed at testing efficiency of mutex and spin locks when we introduce threading. We will output the
time it takes to use both methods with a variable amount of iterations and elements to perform trivial work
doubly linked lists. Output is the time of operations and waiting
*/
int main(int argc, char ** argv)
{
	signal(SIGSEGV, catch_segfault);
	
    static struct option long_options[] = {
        {"threads", required_argument, 0, 't'},         // How many threads we want to run
        {"iterations", required_argument, 0, 'i'},      // How many elements we want to work with per thread
        {"yield", required_argument, 0, 'y'},           // Pthread_yield option
        {"sync", required_argument, 0, 's'},            // Mutex or spin lock
        {"lists", required_argument, 0, 'l'},           // How many doubly linked lists you want to use
        {0,0,0,0 }
    };
    int c = 0;
		num_threads = 1;
		num_iterations = 1;
		sync_option = 'n';
		sub_list = 1;
		opt_yield = 0;
		yieldFlag = 0;

    while ((c = getopt_long(argc, argv, "", long_options, 0)) != -1) {
        switch (c) {
            case 't':
                num_threads = atoi(optarg);
                if (num_threads < 1){
                    num_threads = 1;
                }
                break;
                
            case 'i':
                num_iterations = atoi(optarg);
                if (num_iterations < 1) {
                    num_iterations = 1;
                }
                break;
                
            case 's':
                if (optarg[0] == 'm') {
                    sync_option = 'm';
                    pthread_mutex_init(&mut_lock, NULL);
                } else if (optarg[0] == 's') {
                    sync_option = 's';
                } else {
                    fprintf(stderr, "Unrecognized sync option\n");
                    exit(1);
                }
                break;
                
            case 'l':
                sub_list = atoi(optarg);
                if(sub_list < 1) {
                    sub_list = 1;
                }
                break;
                
            case 'y':
                for (unsigned int i = 0; i < strlen(optarg); i++) {
                    if (optarg [i] == 'i') {
                        opt_yield = opt_yield | INSERT_YIELD;
                    } else if (optarg[i] == 'd') {
                        opt_yield = opt_yield | DELETE_YIELD;
                    } else if (optarg [i] == 'l') {
                        opt_yield = opt_yield | LOOKUP_YIELD;
                    } else {
                        fprintf(stderr, "Unrecognized argrument \n");
                        exit(1);
                    }
                }
                break;
                
            default:
                fprintf(stderr, "Unrecognized argrument \n");
                exit(1);
        }
    }
    
    // Initialize doubly linked lists. Can have one list or multiple lists
    if (sub_list == 1) {
        head = (SortedList_t *)malloc(sizeof(SortedList_t));
        head->key = NULL;
        head -> next = head;
        head -> prev = head;
    } else {
        // Create sub_lists
        sub_heads = (SortedListElement_t **) malloc(sub_list * sizeof (SortedListElement_t *));
        for (int k = 0; k < sub_list; k++)
				{
            sub_heads[k] = (SortedList_t *) malloc(sizeof(SortedList_t));
            //initialize each of the head pointers
            sub_heads[k]->key = NULL;
            sub_heads[k] -> next = sub_heads[k];
            sub_heads[k] -> prev = sub_heads[k];
        }
        
        if (sync_option == 'm') {
            sub_mut_lock = (pthread_mutex_t *) malloc(sub_list * sizeof (pthread_mutex_t));
            memset(sub_mut_lock, 0, sub_list * sizeof(pthread_mutex_t));
        } else if (sync_option == 's') {
            sub_spin_lock = (int *) malloc((sub_list * sizeof (int)));
            memset(sub_spin_lock, 0, sub_list * sizeof(int));
        }

    }

    // Create and initialize the list of elements
    int num_items = num_threads * num_iterations;
    elements = (SortedListElement_t*) malloc(num_items * sizeof(SortedListElement_t)); //elements is global
    if (elements == NULL) {
        fprintf(stderr, "Couldn't allocate enough memory for list\n");
        exit(1);
    }
		
    // Produce_random_keys(num_items, elements);
    for (int i = 0 ; i < num_items; i++) {
        char* create_key = (char*)malloc(10 * sizeof(char));
        if (create_key == NULL) {
            fprintf(stderr, "Note enough memory to allocate to keys\n");
            exit(1);
        }
        for (int j = 0; j < 9; j++) {
            create_key[j] = (rand() % 94) + 33;
        }
        create_key[9] = 0;
        elements[i].key = create_key;
    }
    
    // Create array of counters so we can time each thread individually
    wait_for_lock = (long long *) malloc(num_threads * sizeof(long));
    memset(wait_for_lock, 0, num_threads * sizeof(long));
    
    // Start timing work done for entire work, including thread creation and joining
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
		
    // Create the threads
    pthread_t threads [num_threads];
    for (long i = 0; i < num_threads; i ++) {
        int j = pthread_create(&threads[i], NULL, list_work, (void *)i); //just pass in the i value and make the elements global
        if (j) {
            fprintf(stderr, "Error creating threads\n");
            exit(2);
        }
    }
    
    // Join threads
    for (int i = 0; i < num_threads; i ++) {
        int j = pthread_join(threads[i], NULL);
        if (j != 0) {
            fprintf(stderr, "Error joining threads.\n");
            exit(2);
        }
    }
    
    //now we have the num_threads and num_iterations
    clock_gettime(CLOCK_MONOTONIC, &end);
    long long total_time =  (end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec; //in nanoseconds
    
    //get the total acquisition time
    long long total_wait = 0;
    for (int j = 0; j < num_threads; j++) {
        total_wait += wait_for_lock[j];
    }
    
    if (SortedList_length(head) != 0) {
        fprintf(stderr,"List isnt empty\n");
        exit(2);
    }
    
  //print csv 
	fprintf(stdout, "list-");

	switch(opt_yield) {
			 case 0:
					fprintf(stdout, "none");
			 break;
			 case 1:
					fprintf(stdout, "i");
			 break;
			 case 2:
					fprintf(stdout, "d");
			 break;
			 case 3:
					fprintf(stdout, "id");
			 break;
			 case 4:
					fprintf(stdout, "l");
			 break;
			 case 5:
					fprintf(stdout, "il");
			 break;
			 case 6:
					fprintf(stdout, "dl");
			 break;
			 case 7:
					fprintf(stdout, "idl");
			 break;
			 default:
					 break;
	 }
 
	 if (sync_option == 's')
	 {
		 fprintf(stdout, "-s");
	 }
	 else if (sync_option == 'm')
	 {
		 fprintf(stdout, "-m");
	 }
	 else
	 {
		 fprintf(stdout, "-none");
	 }

    int num_operations = num_threads * num_iterations * 3;
    long long time_per_operation = total_time /num_operations;
    long long time_per_wait = (total_wait/num_operations);
    
    fprintf(stdout, ",%d,%d,%d,%d,%lld,%lld,%lld\n", num_threads, num_iterations, sub_list, num_operations,wait_time, time_per_operation, timePerClockOperation);

    if (sync_option == 'm') {
        pthread_mutex_destroy(&mut_lock);
    }
    //important: free also all of the characters strings
    //free the keys
    for (int i = 0; i < num_threads * num_iterations; i++) {
        free((void *)elements[i].key);
    }
    
    if (sub_spin_lock != NULL) {
        free (sub_spin_lock );
    }
    if (sub_mut_lock != NULL) {
        free (sub_mut_lock);
    }
    
    if (wait_for_lock != NULL) {
        free (wait_for_lock);
    }
    free(elements);
    free (sub_heads);
    free(head);
    
    //IMPORTANT: dont forget to free up the head Pointer
    //free (head);
    //free the elements in the elements_head and also elements_head itself
    return 0;
    
}
