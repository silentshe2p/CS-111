#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <sched.h>

#define BILLION 1000000000L

const char MUTEX = 'm';
const char SPIN = 's';
const char COMPARE = 'c';
int num_threads = 1;
int iterations = 1;
int opt_yield = 0;
int lock = 0;
char sync = '\0';
pthread_mutex_t mutex;

int print_usage( int rc ) {
	fprintf( stderr, "Usage [ys] -t threads# i iterations#\n" );
	exit(rc);
}

void add( long long *pointer, long long value ) {
	long long sum = *pointer + value;
	if( opt_yield )
		sched_yield();
	*pointer = sum;
}

void cas_add( long long *pointer, long long value ) {
	long long old, new;
	do {
		old = *pointer;
		new = old + value;
		if( opt_yield )
			sched_yield();
	}
	while( __sync_val_compare_and_swap( pointer, old, new ) != old );
}

void *do_add( void *counter ) {
	// add 1
	for( int i = 0; i < iterations; i++ ) {
		if( sync == MUTEX ) {
			if( pthread_mutex_lock(&mutex) != 0 ) {
				fprintf( stderr, "mutex_lock() failed" );
				exit(2);
			}
			add( counter, 1 );
			if( pthread_mutex_unlock(&mutex) != 0 ) {
				fprintf( stderr, "mutex_lock() failed" );
				exit(2);
			}
		}
		else if( sync == SPIN ) {
			while( __sync_lock_test_and_set( &lock, 1 ) == 1 );
			add( counter, 1 );
			__sync_lock_release(&lock);
		}
		else if( sync == COMPARE ) {
			cas_add( counter, 1 );
		}
		else
			add( counter, 1 );
	}
	// add -1
	for( int i = 0; i < iterations; i++ ) {
		if( sync == MUTEX ) {
			if( pthread_mutex_lock(&mutex) != 0 ) {
				fprintf( stderr, "mutex_lock() failed" );
				exit(2);
			}
			add( counter, -1 );
			if( pthread_mutex_unlock(&mutex) != 0 ) {
				fprintf( stderr, "mutex_lock() failed" );
				exit(2);
			}
		}
		else if( sync == SPIN ) {
			while( __sync_lock_test_and_set( &lock, 1 ) == 1 );
			add( counter, -1 );
			__sync_lock_release(&lock);
		}
		else if( sync == COMPARE ) {
			cas_add( counter, -1 );
		}
		else
			add( counter, -1 );
	}
	return NULL;
}

int main( int argc, char *argv[] ) {
	long long counter = 0;
	int opt = 0;
	static struct option long_opts[] = 
	{
		{"threads", required_argument, 0, 't'},
		{"iterations", required_argument, 0, 'i'},
		{"yield", no_argument, 0, 'y'},
		{"sync", required_argument, 0, 's'}
	};

	while( (opt = getopt_long( argc, argv, "t:i:", long_opts, NULL )) != -1 ) {
		switch(opt) {
			case 't': 
				num_threads = atoi(optarg);
				break;
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'y':
				opt_yield = 1;
				break;
			case 's':			
				if( optarg[0] == MUTEX || optarg[0] == SPIN || optarg[0] == COMPARE )
					sync = optarg[0];
				else 
					print_usage(1);
				break;
			default:
				print_usage(1);
		}
	}

	pthread_t threads[num_threads];
	struct timespec start, end;

	// Start timer
	if( clock_gettime( CLOCK_MONOTONIC, &start ) == -1 ) {
		fprintf( stderr, "clock_gettime() failed" );
		exit(1);
	}

	// Create thread(s)
	for( int i = 0; i < num_threads; i++ ) {
		if( pthread_create( &threads[i], NULL, do_add, &counter ) != 0 ) {
			fprintf( stderr, "pthread_create() failed" );
			exit(1);
		}
	}
	// Join thread(s)
	for( int i = 0; i < num_threads; i++ ) {
		if( pthread_join( threads[i], NULL ) != 0 ) {
			fprintf( stderr, "pthread_join failed" );
			exit(1);
		}
	}

	// Stop timer
	if( clock_gettime( CLOCK_MONOTONIC, &end ) == -1 ) {
		fprintf( stderr, "clock_gettime() failed" );
		exit(1);
	}

	int num_operations = num_threads * iterations * 2;
	long long total_run_time = ( end.tv_sec - start.tv_sec ) * BILLION + ( end.tv_nsec - start.tv_nsec );
	long long avg_time_per_op = total_run_time / num_operations;

	if( opt_yield ) {
		if( sync == '\0' )
			fprintf( stdout, "add-yield-none" );
		else
			fprintf( stdout, "add-yield-%c", sync );
	}
	else {
		if( sync == '\0' )
			fprintf( stdout, "add-none" );
		else
			fprintf( stdout, "add-%c", sync );
	}
	fprintf( stdout, ",%d,%d,%d,%lld,%lld,%lld\n", num_threads, iterations, num_operations, total_run_time, avg_time_per_op, counter );
	return 0;
}