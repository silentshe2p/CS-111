// NAME:  BACH HOANG
// ID:    104737449
// EMAIL: ko.wing.bird@gmail.com

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <getopt.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mcrypt.h>

const char CR = 0x0D;
const char LF = 0x0A;
const size_t BUF_SIZE = 1024;
struct termios saved_attr;
int TIMEOUT = -1;
int log_fl = 0;
int encrypt_fl = 0;
int log_fd, socket_fd;
char *log_file;

void error( char *msg ) {
	fprintf( stderr, "%s", msg );
	exit(1);
}

void print_usage(int rc) {
	fprintf( stderr, "Usage: lab1b-client [le] port=port#" );
	exit(rc);
}

void reset_input_mode(void) {
	tcsetattr( STDIN_FILENO, TCSANOW, &saved_attr );	
}

void set_input_mode(void) {
	struct termios attr;

	if( !isatty(STDIN_FILENO) )
		error( "Not a terminal");

	tcgetattr( STDIN_FILENO, &saved_attr );
	atexit(reset_input_mode);

	tcgetattr( STDIN_FILENO, &saved_attr );
	attr.c_iflag = ISTRIP;
	attr.c_oflag = 0;
	attr.c_lflag = 0;
	tcsetattr( STDIN_FILENO, TCSANOW, &attr );
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
			if( w_fd != STDOUT_FILENO ) {
				if( write( w_fd, write_buf+1, 1 ) == -1 )
					error( "write() failed" );				
			}			
		}
		else{
			if( write( STDOUT_FILENO, buf+i, 1 ) == -1 )
				error( "write() failed" );
			if( w_fd != STDOUT_FILENO ) {
				if( write( w_fd, buf+i, 1 ) == -1 )
					error( "write() failed" );				
			}
		}
	}
}

int main( int argc, char **argv ) {
	int opt = 0;
	int portno;
	struct sockaddr_in server_addr;
	struct hostent *server;

	static struct option long_opts[] =
	{
		{"port", 	required_argument, 0, 'p'},
		{"log", 	required_argument, 0, 'l'},
		{"encrypt", no_argument, 	   0, 'e'}
	};

	while( (opt = getopt_long( argc, argv, "p:l:e", long_opts, NULL )) != -1 ) {
		switch(opt) {
			case 'p': 
				portno = atoi(optarg);
				break;
			case 'l': 
				log_file = optarg;
				log_fl = 1;
				break;
			case 'e': 
				encrypt_fl = 1;
			default:
				print_usage(1);
				break;
		}
	}
	set_input_mode();

	// Create a socket point
	socket_fd = socket( AF_INET, SOCK_STREAM, 0 );
	if( socket_fd < 0 )
		error( "Error opening socket" );
	server = gethostbyname("localhost");
	if( server == NULL )
		error( "No such host" );

	// Init socket structure
	memset( (char*) &server_addr, 0, sizeof(server_addr) );
	server_addr.sin_family = AF_INET;
	memcpy( (char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length );
	server_addr.sin_port = htons(portno);

	// Connect to the server
	if( connect( socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr) ) < 0 )
		error( "Error connecting" );

	struct pollfd pfds[] =
	{
		{ STDIN_FILENO, POLLIN | POLLHUP | POLLERR, 0 },
		{ socket_fd,	POLLIN | POLLHUP | POLLERR, 0 },
	};							

	while(1) {
		int ret = poll(pfds, 2, TIMEOUT);
		if( ret == -1 )
			error( "poll() error" );
		if( pfds[0].revents & POLLIN ) {
			read_write( pfds[0].fd, socket_fd );
		}
		if( pfds[1].revents & POLLIN ) {
			read_write( pfds[1].fd, STDOUT_FILENO );
		}
		if( (pfds[0].revents & (POLLHUP+POLLERR)) || (pfds[1].revents & (POLLHUP+POLLERR)) )
			exit(1);					
	}	
	exit(0);
}