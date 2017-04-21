// NAME:  BACH HOANG
// ID:    104737449
// EMAIL: ko.wing.bird@gmail.com

#define _POSIX_SOURCE
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<getopt.h>
#include<poll.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>

const char CR = 0x0D;
const char LF = 0x0A;
const char CTRL_D = 0x04;
const char CTRL_C = 0X03;
const size_t BUF_SIZE = 1024;
const int TIMEOUT = -1;
struct termios saved_attr;
pid_t cpid;
int shell_fl = 0;	
int to_child_pipe[2];
int from_child_pipe[2];

void reset_input_mode(void) {
	if(shell_fl) {
		int stat;
		waitpid( cpid, &stat, 0 );
		if( WIFEXITED(stat) )
			fprintf( stderr, "SHELL EXIT SIGNAL=%d STATUS=%x\r\n", WEXITSTATUS(stat), (stat&0x007F) );	
		else if( WIFSIGNALED(stat) )
			fprintf( stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\r\n", WTERMSIG(stat), (stat&0x007F) );
		else
			fprintf( stderr, "SHELL EXIT.\r\n" );
	}	
	tcsetattr( STDIN_FILENO, TCSANOW, &saved_attr );
}

void set_input_mode(void) {
	struct termios attr;
	
	// terminal checking
	if( !isatty(STDIN_FILENO) ) {
		fprintf( stderr, "Not a terminal.\n" );
		exit(1);
	}

	tcgetattr( STDIN_FILENO, &saved_attr );
	atexit(reset_input_mode);

	// set input mode to non-canonical
	tcgetattr( STDIN_FILENO, &attr );
	attr.c_iflag = ISTRIP;
	attr.c_oflag = 0;
	attr.c_lflag = 0;
	tcsetattr( STDIN_FILENO, TCSANOW, &attr );
}

void read_write(int r_fd, int w_fd){
	char buf[BUF_SIZE];
	int b_read = read(r_fd, buf, BUF_SIZE);

	if( b_read == 0 )
		exit(1);
	for( int i = 0; i < b_read; i++ ) {
		if( buf[i] == CR || buf[i] == LF ) {
			char write_buf[2];
			write_buf[0] = CR;
			write_buf[1] = LF;			
			if( write( STDOUT_FILENO, write_buf, 2 ) == -1 ) {
				fprintf( stderr, "write() failed.\r\n" );
				exit(1);					
			}
			if( w_fd != STDOUT_FILENO ) {
				if( write( w_fd, write_buf+1, 1 ) == -1 ) {
					fprintf( stderr, "write() failed.\r\n" );
					exit(1);
				}						
			}					
		}
		else if( buf[i] == CTRL_D ){
			if(shell_fl) {
				close( to_child_pipe[1] );
				close( from_child_pipe[0] );
				kill( cpid, SIGHUP );
				exit(0);
			}
			exit(0);
		}
		else if( buf[i] == CTRL_C ) {
			if(shell_fl) {
				close( to_child_pipe[1] );
				close( from_child_pipe[0] );
				kill( cpid, SIGINT );
				exit(1);
			}
			exit(1);				
		}
		else {
			if( write( STDOUT_FILENO, buf+i, 1 ) == -1 ) {
				fprintf( stderr, "write() failed.\r\n" );
				exit(1);
			}	
			if( w_fd != STDOUT_FILENO ) {
				if( write( w_fd, buf+i, 1 ) == -1 ) {
					fprintf( stderr, "write() failed.\r\n" );
					exit(1);	
				}				
			}				
		}
	}
}

void sig_handler(int signum) {
	// shutdown on SIGPIPE
	if( signum == SIGPIPE )
		exit(1);
}

int main( int argc, char **argv ) {
	int opt = 0;
	static struct option long_opts[] = 
	{
		{"shell", no_argument, 0, 's'}
	};

	while( ( opt = getopt_long(argc, argv, "s", long_opts, NULL) ) != -1 ) {
		switch(opt) {
			case 's': shell_fl = 1;
				signal( SIGPIPE, sig_handler ); 
				break;
			default: 
				break;
		}
	}

	set_input_mode();

	if( shell_fl ) {	
		if( pipe(to_child_pipe) == -1 ) {
			fprintf( stderr, "pipe() failed.\r\n" );
			exit(1);
		}
		if( pipe(from_child_pipe) == -1 ) {
			fprintf( stderr, "pipe() failed.\r\n" );
			exit(1);
		}				

		cpid = fork();
		// parent process
		if( cpid > 0 ) {
			close( to_child_pipe[0] );
			close( from_child_pipe[1] );

			struct pollfd pfds[] =
			{
				{ STDIN_FILENO,       POLLIN | POLLHUP | POLLERR, 0 },
				{ from_child_pipe[0], POLLIN | POLLHUP | POLLERR, 0 },
			};							

			while(1) {
				int ret = poll(pfds, 2, TIMEOUT);
				if( ret == -1 ) {
					fprintf( stderr, "poll() error.\r\n" );
					exit(1);
				}
				if( pfds[0].revents & POLLIN ) {
					read_write(pfds[0].fd, to_child_pipe[1]);
				}
				if( pfds[1].revents & POLLIN ) {
					read_write(pfds[1].fd, STDOUT_FILENO);
				}
				if( (pfds[0].revents & POLLHUP+POLLERR) || (pfds[1].revents & POLLHUP+POLLERR) )
					exit(1);					
			}							
		}
		// child process
		else if( cpid == 0 ) {			
			close( to_child_pipe[1] );
			close( from_child_pipe[0] );			
			dup2( to_child_pipe[0], STDIN_FILENO );
			dup2( from_child_pipe[1], STDOUT_FILENO );
			dup2( from_child_pipe[1], STDERR_FILENO );
			close( to_child_pipe[0] );			
			close( from_child_pipe[1] );
			
			char *execvp_argv[1];
			char *execvp_filename = "/bin/bash";
			execvp_argv[0] = execvp_filename;
			execvp_argv[1] = NULL;
			if( execvp( execvp_filename, execvp_argv ) == -1 ) {
				fprintf( stderr, "execvp() failed.\r\n" );
				exit(1);
			}
		}
		else {
			fprintf( stderr, "fork() failed.\r\n" );
			exit(1);
		}
	}
	else
		read_write(STDIN_FILENO, STDOUT_FILENO);
	exit(0);
}
