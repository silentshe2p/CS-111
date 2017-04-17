#define _POSIX_SOURCE
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<getopt.h>
#include<poll.h>
#include<signal.h>
#include<sys/types.h>

const char CR = 0x0D;
const char LF = 0x0A;
const char ESC = 0x04;
const size_t BUF_SIZE = 1024;
const int TIMEOUT = -1;
struct termios saved_attr;
pid_t cpid;
int shell_fl = 0;	

void reset_input_mode(void) {
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
	// attr.c_lflag &= ~(ICANON|ECHO);
	// attr.c_cc[VMIN] = 1;
	// attr.c_cc[VTIME] = 0;
	tcsetattr( STDIN_FILENO, TCSANOW, &attr );
}

int readWrite(int rfd, int wfd){
	char buf[BUF_SIZE];
	int b_read = 0;
	int b_count = 0;
	int i = 0;
	while(1){
		b_read = read(rfd, buf+b_count, BUF_SIZE);
		if(b_count == -1){
			fprintf( stderr, "read() failed.\n" );
			exit(1);
		}
		// else if(b_count == 0 && rfd == pipe2[0]){
		// 	exit(1);
		// }
		b_count += b_read;
		while( i < b_count ) {
			char writeBuf[10];
			writeBuf[0] = buf[i];
			int writeBUF_SIZE = 1;
			char shellBuf[10];
			shellBuf[0] = buf[i];
			int shellBUF_SIZE = 1;

			if( buf[i] == '\r' || buf[i] == '\n' ){
				writeBuf[0] = '\r';
				writeBuf[1] = '\n';
				writeBUF_SIZE = 2;
				shellBuf[0] = '\n';
			}
			else if( buf[i] == ESC ){
				exit(1);
			}			
			for( int j = 0; j < writeBUF_SIZE; j++ ) {
				if( write( STDOUT_FILENO, writeBuf+j, 1 ) == -1 ) {
					fprintf( stderr, "write() failed.\n" );
					exit(1);
				}
			}
			if(shell_fl){
				for( int j = 0; j < shellBUF_SIZE; j++ ) {
					if( write( wfd, shellBuf+j, 1 ) == -1 ) {
						fprintf( stderr, "write() failed.\n" );
						exit(1);
					}
				}
			}
			i++;
		}
	}
}

void read_write( int r_fd, int w_fd ) {
	char buf[BUF_SIZE];
	int b_read;
	while(1) {
		b_read = read( r_fd, buf, BUF_SIZE );
		if( b_read == -1 ) {
			fprintf( stderr, "read() failed.\n" );
			exit(1);
		}
		if( b_read == 0 ) {
			exit(1);
		}
		for( int i = 0; i < b_read; i++ ) {
			if( (buf[i] == CR || buf[i] == LF) ) {
				char crlf[2];
				crlf[0] = '\r';
				crlf[1] = '\n';
				if( write( STDOUT_FILENO, crlf, 2 ) == -1 ) {
					fprintf( stderr, "write() failed.\n" );
					exit(1);
				}				
				if(shell_fl) {
					if( write( w_fd, crlf+1, 1 ) == -1 ) {
						fprintf( stderr, "write() shell failed.\n" );
						exit(1);
					}					
				}
			}
			else if( buf[i] == ESC )
				exit(0);
			else {
				if( write( STDOUT_FILENO, buf+i, 1 ) == -1 ) {
					fprintf( stderr, "write() failed.\n" );
					exit(1);
				}
				if(shell_fl){
					if( write( w_fd, buf+i, 1 ) == -1 ) {
						fprintf( stderr, "write() shell failed.\n" );
						exit(1);
					}					
				}
			}
		}
	}
}

void sig_handler(int signum) {
	// use kill() to send SIGINT on ^C
	if( signum == SIGINT )
		kill(cpid, SIGINT);
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
				signal( SIGINT, sig_handler );
				signal( SIGPIPE, sig_handler ); 
				break;
			default: 
				break;
		}
	}

	set_input_mode();

	if( shell_fl ) {	
		int to_child_pipe[2];
		int from_child_pipe[2];

		if( pipe(to_child_pipe) == -1 ) {
			fprintf( stderr, "pipe() failed.\n" );
			exit(1);
		}
		if( pipe(from_child_pipe) == -1 ) {
			fprintf( stderr, "pipe() failed.\n" );
			exit(1);
		}

		cpid = fork();
		// parent process
		if( cpid > 0 ) {
			close( to_child_pipe[0] );
			close( from_child_pipe[1] );

			int ret;
			struct pollfd pfds[] =
			{
				{ STDIN_FILENO,       POLLIN | POLLHUP | POLLERR, 0 },
				{ from_child_pipe[0], POLLIN | POLLHUP | POLLERR, 0 },
			};					

			while(1) {		
				ret = poll(pfds, 2, TIMEOUT);
				if( ret == -1 ) {
					fprintf( stderr, "poll() error.\n" );
					exit(1);
				}
				if( pfds[0].revents & POLLIN ){
					// char buffer[BUF_SIZE];
					// int count = 0;
					// count = read(STDIN_FILENO, buffer, BUF_SIZE);
					// write(to_child_pipe[1], buffer, count);
					readWrite(pfds[0].fd, to_child_pipe[1]);
					// read_write( pfds[0].fd, from_child_pipe[1] );
				}
				if( pfds[1].revents & POLLIN ){
			// 		// char buffer[BUF_SIZE];
			// 		// int count = 0;
					// count = read(from_child_pipe[0], buffer, BUF_SIZE);
					// write(STDOUT_FILENO, buffer, count);				
					// read_write( pfds[1].fd, STDOUT_FILENO );
					readWrite(pfds[1].fd, STDOUT_FILENO);
					// readWrite(pfds[1].fd, STDERR_FILENO);
				}
				if( (pfds[0].revents & POLLHUP) || (pfds[1].revents & POLLHUP) )
					exit(1);
				if( (pfds[0].revents & POLLERR) || (pfds[1].revents & POLLERR) ) {
					fprintf( stderr, "Poll error.\n" );
					exit(1);
				}					
			}							
		}
		// child process
		else if( cpid == 0 ) {
			dup2( to_child_pipe[0], STDIN_FILENO );
			dup2( from_child_pipe[1], STDOUT_FILENO );
			// dup2( from_child_pipe[1], STDERR_FILENO );
			close( to_child_pipe[0] );			
			close( to_child_pipe[1] );
			close( from_child_pipe[0] );
			close( from_child_pipe[1] );
			
			char *execvp_argv[1];
			execvp_argv[0] = NULL;
			if( execvp( "/bin/bash", execvp_argv ) == -1 ) {
				fprintf( stderr, "execvp() failed.\n" );
				exit(1);
			}
		}
		else {
			fprintf( stderr, "fork() failed." );
			exit(1);
		}
	}
	else
		read_write(STDIN_FILENO, STDOUT_FILENO);
	exit(0);
}
