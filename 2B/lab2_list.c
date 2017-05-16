#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <sched.h>
#include <string.h>
#include <signal.h>
#include "SortedList.h"

#define BILLION 1000000000L
#define KEY_LENGTH 5
const char INSERT = 'i';
const char DELETE = 'd';
const char LOOKUP = 'l';
const char MUTEX = 'm';
const char SPIN = 's';

int num_threads = 1;
int num_lists = 1;
int iterations = 1;
int opt_yield = 0;
int required = 0;
char sync = '\0';

typedef struct {
	SortedList_t list;
	int s_lock;
	pthread_mutex_t m_lock;
} SortedSubList_t;

SortedSubList_t *list;
SortedListElement_t *elements;

int print_usage( int rc ) {
	fprintf( stderr, "Usage [ys] -t threads# i iterations#\n" );
	exit(rc);
}

void sig_handler( int signum ) {
	if( signum == SIGSEGV ) {
		fprintf( stderr, "Segmentation fault caught\n" );
		exit(2);
	}
}

void assign_random_key() {
	srand( time(NULL) );
	for( int i = 0; i < required; i++ ) {
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
		// fprintf( stdout, "key is %s\n", key );
		elements[i].key = key;
	}
}

// Reference: http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx
unsigned int djb_hash( const char *key, int len ) {
    unsigned int h = 0;
    for ( int i = 0; i < len; i++ )
        h = 33 * h ^ key[i];
    return h;
}

long long elapsed_time( struct timespec *start, struct timespec *end ) {
	return BILLION * (end->tv_sec - start->tv_sec) + end->tv_nsec - start->tv_nsec;
}

void *thread_func( void *v_tid ) {
	int tid = *(int *)v_tid;
	SortedListElement_t *element;
	SortedSubList_t *sub;
	struct timespec start, end;
	long long wait_time = 0;
	// Insert elements into the list
	for( int i = tid; i < required; i += num_threads ) {
		element = &elements[i];
		sub = &list[ djb_hash( element->key, KEY_LENGTH ) % num_lists ];
		if( sync == MUTEX ) {
			clock_gettime( CLOCK_MONOTONIC, &start );
			pthread_mutex_lock(&sub->m_lock);
			clock_gettime( CLOCK_MONOTONIC, &end );
			wait_time += elapsed_time(&start, &end);
			SortedList_insert( &sub->list, element );
			pthread_mutex_unlock(&sub->m_lock);
		}
		else if( sync == SPIN ) {
			clock_gettime( CLOCK_MONOTONIC, &start );
			while( __sync_lock_test_and_set( &sub->s_lock, 1 ) == 1 );
			clock_gettime( CLOCK_MONOTONIC, &end );
			wait_time += elapsed_time(&start, &end);			
			SortedList_insert( &sub->list, element );
			__sync_lock_release(&sub->s_lock);
		}
		else
			SortedList_insert( &sub->list, element );
	}

	// Get the list length
	if( sync == MUTEX ) {
		clock_gettime( CLOCK_MONOTONIC, &start );
		for( int i = 0; i < num_lists; i++ )
			pthread_mutex_lock( &list[i].m_lock );
		clock_gettime( CLOCK_MONOTONIC, &end );
		wait_time += elapsed_time(&start, &end);
		for( int i = 0; i < num_lists; i++ )
			SortedList_length( &list[i].list );
		for( int i = 0; i < num_lists; i++ )
			pthread_mutex_unlock( &list[i].m_lock );
	}
	else if( sync == SPIN ) {
		for( int i = 0; i < num_lists; i++ ) {
			clock_gettime( CLOCK_MONOTONIC, &start );
			while( __sync_lock_test_and_set( &list[i].s_lock, 1 ) == 1 );
			clock_gettime( CLOCK_MONOTONIC, &end );
			wait_time += elapsed_time(&start, &end);			
			SortedList_length( &list[i].list );
			__sync_lock_release(&list[i].s_lock);
		}
	}
	else {
		for( int i = 0; i < num_lists; i++ )
			SortedList_length( &list[i].list );		
	}


	// Lookup and delete previously inserted elements from the list
	SortedListElement_t *tmp;
	for( int i = tid; i < required; i += num_threads ) {
		element = &elements[i];
		sub = &list[ djb_hash( element->key, KEY_LENGTH ) % num_lists ];		
		if( sync == MUTEX ) {
			clock_gettime( CLOCK_MONOTONIC, &start );
			pthread_mutex_lock(&sub->m_lock);
			clock_gettime( CLOCK_MONOTONIC, &end );
			wait_time += elapsed_time(&start, &end);
			tmp = SortedList_lookup( &sub->list, element->key );
			if( tmp == NULL ) {
				fprintf( stderr, "Unable to find an element that was inserted\n" );
				exit(2);
			}
			if( SortedList_delete(tmp) != 0 ) {
				fprintf( stderr, "Unable to delele\n" );
				exit(2);
			}				
			pthread_mutex_unlock(&sub->m_lock);
		}
		else if( sync == SPIN ) {
			clock_gettime( CLOCK_MONOTONIC, &start );
			while( __sync_lock_test_and_set( &sub->s_lock, 1 ) == 1 );
			clock_gettime( CLOCK_MONOTONIC, &end );
			wait_time += elapsed_time(&start, &end);			
			tmp = SortedList_lookup( &sub->list, element->key );
			if( tmp == NULL ) {
				fprintf( stderr, "Unable to find an element that was inserted\n" );
				exit(2);
			}			
			if( SortedList_delete(tmp) != 0 ) {
				fprintf( stderr, "Unable to delele\n" );
				exit(2);
			}				
			__sync_lock_release(&sub->s_lock);
		}
		else {
			tmp = SortedList_lookup( &sub->list, element->key );	
			if( tmp == NULL ) {
				fprintf( stderr, "Unable to find an element that was inserted\n" );
				exit(2);
			}
			if( SortedList_delete(tmp) != 0 ) {
				fprintf( stderr, "Unable to delele with key of %s\n", tmp->key );
				exit(2);
			}		
		}
	}
	return (void *)wait_time;
}

int main( int argc, char *argv[] ) {
	int opt = 0;
	static struct option long_opts[] = 
	{
		{"threads", required_argument, 0, 't'},
		{"iterations", required_argument, 0, 'i'},
		{"yield", required_argument, 0, 'y'},
		{"sync", required_argument, 0, 's'},
		{"lists", required_argument, 0, 'l'}
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
				if( strlen(optarg) > 3 )
					print_usage(2);
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
						print_usage(1);
				}
				break;
			case 's':			
				if( optarg[0] == MUTEX || optarg[0] == SPIN )
					sync = optarg[0];
				else 
					print_usage(1);
				break;
			case 'l':
				num_lists = atoi(optarg);
				break;
			default:
				print_usage(1);
		}
	}

	// Initialization
	required = num_threads * iterations;
	list = malloc( num_lists * sizeof(SortedSubList_t) );
	if( list == NULL ) {
		fprintf( stderr, "malloc() failed\n" );
		exit(1);
	}
	for( int i = 0; i < num_lists; i++ ) {
		SortedSubList_t *sub = &list[i];
		SortedList_t *sub_list = &sub->list;
		sub_list->next = sub_list;
		sub_list->prev = sub_list;
		sub_list->key = NULL;
		if( sync == MUTEX )
			pthread_mutex_init( &sub->m_lock, NULL );
		else
			sub->s_lock = 0;
	}
	
	elements = malloc( required * sizeof(SortedListElement_t) );
	if( elements == NULL ) {
		fprintf( stderr, "malloc() failed\n" );
		exit(1);
	}
	signal( SIGSEGV, sig_handler );
	assign_random_key();

	pthread_t threads[num_threads];
	int *tids = malloc( num_threads * sizeof(int) );
	long long wait_time = 0;
	void *wait_time_thread = NULL;
	struct timespec start, end;

	// Start timer
	if( clock_gettime( CLOCK_MONOTONIC, &start ) == -1 ) {
		fprintf( stderr, "clock_gettime() failed\n" );
		exit(1);
	}

	// Create thread(s)
	for( int i = 0; i < num_threads; i++ ) {
		tids[i] = i;
		if( pthread_create( &threads[i], NULL, thread_func, &tids[i] ) != 0 ) {
			fprintf( stderr, "pthread_create() failed\n" );
			exit(1);
		}
	}
	// Join thread(s)
	for( int i = 0; i < num_threads; i++ ) {
		if( pthread_join( threads[i], &wait_time_thread ) != 0 ) {
			fprintf( stderr, "pthread_join failed\n" );
			exit(1);
		}
		wait_time += (long long)wait_time_thread;
	}

	// Stop timer
	if( clock_gettime( CLOCK_MONOTONIC, &end ) == -1 ) {
		fprintf( stderr, "clock_gettime() failed\n" );
		exit(1);
	}

	int length = 0;
	for( int i = 0; i < num_lists; i++ ) {
		length += SortedList_length( &list[i].list );
	}
	if( length != 0 ) {
		fprintf( stderr, "The length list is %d instead of 0\n", length );
		exit(2);
	}

	long long num_operations = required * 3;
	long long total_run_time = elapsed_time(&start, &end);
	long long avg_time_per_op = total_run_time / num_operations;

	if( opt_yield ) {
		if( sync == '\0' )
			fprintf( stdout, "list-%s-none", yield_op );
		else
			fprintf( stdout, "list-%s-%c", yield_op, sync );
	}
	else {
		if( sync == '\0' )
			fprintf( stdout, "list-none-none" );
		else
			fprintf( stdout, "list-none-%c", sync );
	}
	fprintf( stdout, ",%d,%d,%d,%lld,%lld,%lld,%lld\n", num_threads, iterations, num_lists, num_operations, total_run_time, avg_time_per_op, wait_time/num_operations );
	free(tids);
	free(elements);
	free(list);
	return 0;
}
