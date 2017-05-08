#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <sched.h>
#include <string.h>
#include "SortedList.h"

#define BILLION 1000000000L
#define KEY_LENGTH 5

const char INSERT = 'i';
const char DELETE = 'd';
const char LOOKUP = 'l';
const char MUTEX = 'm';
const char SPIN = 's';
int num_threads = 1;
int iterations = 1;
int num_lists = 1;
int opt_yield = 0;
int required = 0;
int lock = 0;
char sync = '\0';
pthread_mutex_t mutex;
SortedList_t *list;
SortedListElement_t *elements;
char *randChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";


int print_usage( int rc ) {
	fprintf( stderr, "Usage [ys] -t threads# i iterations#\n" );
	exit(rc);
}

void assign_random_key( SortedListElement_t element ) {
	srand( time(NULL) );
	char *key = malloc( (KEY_LENGTH+1) * sizeof(char) );
	if( key == NULL ) {
		fprintf( stderr, "malloc() failed\n" );
		exit(1);
	}
	for( int i = 0; i < KEY_LENGTH; i++ ) {
		// ASCII 65(A) to 122(z)
		key[i] = rand() % 58 + 65;
	}
	key[KEY_LENGTH] = '\0';
	fprintf( stdout, "key is %s\n", key );
	element.key = key;
}

void *thead_func( void *tid ) {
	// Insert elements into the list
	for( int i = *(int*)tid; i < required; i += num_threads ) {
		if( sync == MUTEX ) {
			pthread_mutex_lock(&mutex);
			SortedList_insert( list, &elements[i] );
			pthread_mutex_unlock(&mutex);
		}
		else if( sync == SPIN ) {
			while( __sync_lock_test_and_set( &lock, 1 ) == 1 );
			SortedList_insert( list, &elements[i] );
			__sync_lock_release(&lock);
		}
		else
			SortedList_insert( list, &elements[i] );
	}
	// Get the list length
	SortedList_length(list);
	SortedListElement_t *tmp;
	// Lookup and delete previously inserted elements from the list
	for( int i = *(int*)tid; i < required; i += num_threads ) {
		if( sync == MUTEX ) {
			pthread_mutex_lock(&mutex);
			tmp = SortedList_lookup( list, elements[i].key );
			if( tmp == NULL ) {
				fprintf( stderr, "Unable to find an element that was inserted\n" );
				exit(2);
			}
			if( SortedList_delete(tmp) != 0 ) {
				fprintf( stderr, "Unable to delele\n" );
				exit(2);
			}				
			pthread_mutex_unlock(&mutex);
		}
		else if( sync == SPIN ) {
			while( __sync_lock_test_and_set( &lock, 1 ) == 1 );
			tmp = SortedList_lookup( list, elements[i].key );
			if( tmp == NULL ) {
				fprintf( stderr, "Unable to find an element that was inserted\n" );
				exit(2);
			}			
			if( SortedList_delete(tmp) != 0 ) {
				fprintf( stderr, "Unable to delele\n" );
				exit(2);
			}				
			__sync_lock_release(&lock);
		}
		else {
			tmp = SortedList_lookup( list, elements[i].key );
			if( tmp == NULL ) {
				fprintf( stderr, "Unable to find an element that was inserted\n" );
				exit(2);
			}				
			if( SortedList_delete(tmp) != 0 ) {
				fprintf( stderr, "Unable to delele\n" );
				exit(2);
			}				
		}
	}
	return NULL;
}

int main( int argc, char *argv[] ) {
	int opt = 0;
	static struct option long_opts[] = 
	{
		{"threads", required_argument, 0, 't'},
		{"iterations", required_argument, 0, 'i'},
		{"yield", required_argument, 0, 'y'},
		{"sync", required_argument, 0, 's'}
	};

	char *yield_op;
	while( (opt = getopt_long( argc, argv, "t:i:", long_opts, NULL )) != -1 ) {
		switch(opt) {
			case 't': 
				num_threads = atoi(optarg);
				break;
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'y':
				yield_op = malloc( strlen(optarg) * sizeof(char) );
				yield_op = optarg;
				for( int i = 0; optarg[i] != '\0'; i++ ) {
					if( optarg[i] == INSERT )
						opt_yield |= INSERT_YIELD;
					else if( optarg[i] == DELETE )
						opt_yield |= DELETE_YIELD;
					else if( optarg[i] == LOOKUP )
						opt_yield |= LOOKUP_YIELD;
					else
						print_usage(2);
				}
				break;
			case 's':			
				if( optarg[0] == MUTEX || optarg[0] == SPIN )
					sync = optarg[0];
				else 
					print_usage(2);
				break;
			default:
				print_usage(2);
		}
	}

	// Initialization
	required = num_threads * iterations;
	list = malloc( sizeof(SortedList_t) );
	if( list == NULL ) {
		fprintf( stderr, "malloc() failed\n" );
		exit(1);
	}
	list->key = NULL;
	list->next = list;
	list->prev = list;
	elements = malloc( required * sizeof(SortedListElement_t) );
	if( elements == NULL ) {
		fprintf( stderr, "malloc() failed\n" );
		exit(1);
	}

	for( int i = 0; i < required; i++ ) {
		assign_random_key(elements[i]);
		// random_key(key);
		// fprintf( stdout, "key is %s\n", key );
		// elements[i].key = key;
	}
	printf("h");
	if( sync == MUTEX )
		pthread_mutex_init( &mutex, NULL );
	pthread_t threads[num_threads];
	int *tid = malloc( num_threads * sizeof(int) );
	struct timespec start, end;

	// Start timer
	if( clock_gettime( CLOCK_MONOTONIC, &start ) == -1 ) {
		fprintf( stderr, "clock_gettime() failed\n" );
		exit(1);
	}

	// Create thread(s)
	for( int i = 0; i < num_threads; i++ ) {
		tid[i] = i;
		if( pthread_create( &threads[i], NULL, thead_func, &tid[i] ) != 0 ) {
			fprintf( stderr, "pthread_create() failed\n" );
			exit(1);
		}
	}
	// Join thread(s)
	for( int i = 0; i < num_threads; i++ ) {
		if( pthread_join( threads[i], NULL ) != 0 ) {
			fprintf( stderr, "pthread_join failed\n" );
			exit(1);
		}
	}

	// Stop timer
	if( clock_gettime( CLOCK_MONOTONIC, &end ) == -1 ) {
		fprintf( stderr, "clock_gettime() failed\n" );
		exit(1);
	}
	free(tid);
	// free(key);

	int length = SortedList_length(list);
	if(  length != 0 ) {
		fprintf( stderr, "The length list is %d instead of 0\n", length );
		exit(2);
	}

	int num_operations = required * 3;
	long long total_run_time = ( end.tv_sec - start.tv_sec ) * BILLION + ( end.tv_nsec - start.tv_nsec );
	long long avg_time_per_op = total_run_time / num_operations;

	if( opt_yield ) {
		if( sync == '\0' )
			fprintf( stdout, "list-%s-none,", yield_op );
		else
			fprintf( stdout, "list-%s-%c", yield_op, sync );
	}
	else {
		if( sync == '\0' )
			fprintf( stdout, "list-none-none" );
		else
			fprintf( stdout, "list-none-%c", sync );
	}
	fprintf( stdout, ",%d,%d,%d,%d,%lld,%lld\n", num_threads, iterations, num_lists, num_operations, total_run_time, avg_time_per_op );
	return 0;
}