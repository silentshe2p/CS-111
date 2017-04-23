// NAME:  BACH HOANG
// ID:    104737449
// EMAIL: ko.wing.bird@gmail.com

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mcrypt.h>

const char CR = 0x0D;
const char LF = 0x0A;
const size_t BUF_SIZE = 1024;
const int TIMEOUT = -1;
pid_t cpid;
int encrypt_fl = 0;
int socket_fd, newsocket_fd;

void error( char *msg ) {
	fprintf( stderr, "%s", msg );
	exit(1);
}

void print_usage(int rc) {
	fprintf( stderr, "Usage: lab1b-client [e] port=port#" );
	exit(rc);
}

void read_write( int r_fd, int w_fd ) {
	char buf[BUF_SIZE];
	int b_read = read( r_fd, buf, BUF_SIZE );

	if( b_read == 0 )
		exit(1);
	for( int i = 0; i < b_read; i++ ) {
		if( buf[i] == CR || buf[i] == LF ) {
			char write_buf[2];
			write_buf[0] = CR;
			write_buf[1] = LF;
			if( write( STDOUT_FILENO, write_buf, 2 ) == -1 )
				error( "write() failed" );
		}
		else{
			if( write( STDOUT_FILENO, buf+i, 1 ) == -1 )
				error( "write() failed" );
			// if( w_fd == sock_fd ) {
			// 	if( write( STDOUT_FILENO, write_buf, 2 ) == -1 )
			// 		error( "write() failed" );				
			// }
		}
	}
}

void make_pipe(int fds[2]) {
	if( pipe(fds) == -1 )
		error( "pipe() failed" );
}

int main( int argc, char **argv ) {
	int opt = 0;
	int portno;
	socklen_t client_len = 0;
	struct sockaddr_in server_addr, client_addr;
	static struct option long_opts[] = 
	{
		{"port", 	required_argument, 0, 'p'},
		{"encrypt", no_argument, 	   0, 'e'}
	};

	while( (opt = getopt_long(argc, argv, "p:e", long_opts, NULL)) != -1 ) {
		switch(opt) {
			case 'p':
				portno = atoi(optarg);
				break;
			case 'e':
				encrypt_fl = 1;
				break;
			default:
				print_usage(1);
				break;
		}
	}

	// Create socket connection
	socket_fd = socket( AF_INET, SOCK_STREAM, 0 );
	if( socket_fd < 0 )
		error( "Error opening socket" );

	// Init socket structure
	memset( (char*) &server_addr, 0, sizeof(server_addr) );
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;	
	server_addr.sin_port = htons(portno);

	// Bind the host address
	if( bind(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0 ) {
		error("Error on binding");
	}

	// Listen for the clients
	listen(socket_fd, 5);
	client_len = sizeof(client_addr);
	// Accept actual connection
	newsocket_fd = accept( socket_fd, (struct sockaddr*) &client_addr, &client_len );
	if( newsocket_fd < 0 )
		error( "Error on accept" );
	dup2( newsocket_fd, STDIN_FILENO );
	dup2( newsocket_fd, STDOUT_FILENO );
	dup2( newsocket_fd, STDERR_FILENO );
	close(newsocket_fd);

	int to_child_pipe[2];
	int from_child_pipe[2];
	make_pipe(to_child_pipe);
	make_pipe(from_child_pipe);
	cpid = fork();
	if( cpid < 0 )
		error( "fork() failed" );
	//parent process
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
			if( ret == -1 )
				error( "poll() error" );
			if( pfds[0].revents & POLLIN ) {
				read_write(pfds[0].fd, to_child_pipe[1]);
			}
			if( pfds[1].revents & POLLIN ) {
				read_write(pfds[1].fd, STDOUT_FILENO);
			}
			if( (pfds[0].revents & (POLLHUP+POLLERR)) || (pfds[1].revents & (POLLHUP+POLLERR)) )
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
	exit(0);
} 