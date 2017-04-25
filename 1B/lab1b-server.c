// NAME:  BACH HOANG
// ID:    104737449
// EMAIL: ko.wing.bird@gmail.com

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mcrypt.h>

#define BUF_SIZE 1024

const char CR = 0x0D;
const char LF = 0x0A;
const char CTRL_D = 0x04;
const char CTRL_C = 0X03;
const int TIMEOUT = -1;
const char FILL_IV = 'A';
pid_t cpid;
int to_child_pipe[2];
int from_child_pipe[2];
int crypt_fl = 0;
int socket_fd, newsocket_fd;
int STDERR_COPY;
MCRYPT encrypt_fd, decrypt_fd;
char *key;
char *IV;
int key_size = 16;

void error( char *msg ) {
	fprintf( stderr, "%s\n", msg );
	exit(1);
}

void print_usage(int rc) {
	fprintf( stderr, "Usage: lab1b-client [e] port=port#" );
	exit(rc);
}

void shut_down(void) {
	if(crypt_fl) {
		mcrypt_generic_deinit(encrypt_fd);
		mcrypt_module_close(encrypt_fd);
		mcrypt_generic_deinit(decrypt_fd);
		mcrypt_module_close(decrypt_fd);		
	}

	dup2(STDERR_COPY, STDERR_FILENO);
	int stat;
	waitpid( cpid, &stat, 0 );
	if( WIFEXITED(stat) )
		fprintf( stderr, "SHELL EXIT SIGNAL=%d STATUS=%x\n", WEXITSTATUS(stat), (stat&0x007F) );	
	else if( WIFSIGNALED(stat) )
		fprintf( stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(stat), (stat&0x007F) );
	else
		fprintf( stderr, "SHELL EXIT\n" );
	close(socket_fd);			
}

void read_write( int r_fd, int w_fd ) {
	char buf[BUF_SIZE];
	int b_read = read( r_fd, buf, BUF_SIZE );

	if( b_read <= 0 ) {
		kill( cpid, SIGTERM );
		exit(1);
	}
	if( crypt_fl && r_fd == STDIN_FILENO ) {
		if( mdecrypt_generic( decrypt_fd, buf, b_read ) != 0 )
			error( "Decrypting failed" );
	}

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
		else if( buf[i] == CTRL_D ){
			close( to_child_pipe[1] );
			exit(0);
		}
		else if( buf[i] == CTRL_C ) {
			close( to_child_pipe[1] );
			close( from_child_pipe[0] );
			kill( cpid, SIGINT );
			exit(1);			
		}		
		else{
			if(crypt_fl && w_fd == STDOUT_FILENO) {
				if( mcrypt_generic( encrypt_fd, buf, b_read ) != 0 )
					error( "Encrypting failed" );
			}			
			if( write( w_fd, buf+i, 1 ) == -1 )
				error( "write() failed" );
		}
	}
}

void make_pipe(int fds[2]) {
	if( pipe(fds) == -1 )
		error( "pipe() failed" );
}

void sig_handler(int signum) {
	if( signum == SIGPIPE ) {
		close( to_child_pipe[1] );
		close( from_child_pipe[0] );		
		kill( cpid, SIGKILL );
		exit(0);
	}
}

char *process_key( char *file ) {
	int key_fd = open( file, O_RDONLY );
	if( key_fd == -1 )
		error( "Opening key file failed" );
	key = calloc(1, key_size);
	if( read( key_fd, key, key_size ) == -1 )
		error( "Reading key failed");
	return key;
}

int main( int argc, char **argv ) {
	int opt = 0;
	int portno;
	socklen_t client_len = 0;
	struct sockaddr_in server_addr, client_addr;
	static struct option long_opts[] = 
	{
		{"port", 	required_argument, 0, 'p'},
		{"encrypt", required_argument, 0, 'e'}
	};

	atexit(shut_down);

	while( (opt = getopt_long( argc, argv, "p:e:", long_opts, NULL )) != -1 ) {
		switch(opt) {
			case 'p':
				portno = atoi(optarg);
				break;
			case 'e':
				crypt_fl = 1;
				key = process_key(optarg);
				break;
			default:
				print_usage(1);
				break;
		}
	}

	// Encryption
	if(crypt_fl) {
		encrypt_fd = mcrypt_module_open( "twofish", NULL, "cfb", NULL );
		if( encrypt_fd == MCRYPT_FAILED )
			error( "mcrypt_open failed" );
		IV = malloc( mcrypt_enc_get_iv_size(encrypt_fd) );
		for( int i = 0; i< mcrypt_enc_get_iv_size(encrypt_fd); i++ )
    		IV[i] = FILL_IV;
		if( mcrypt_generic_init( encrypt_fd, key, key_size, IV ) < 0 )
			error( "mcrypt_init failed" );
		decrypt_fd = mcrypt_module_open( "twofish", NULL, "cfb", NULL );
		if( decrypt_fd == MCRYPT_FAILED )
			error( "mcrypt_open failed" );
		if( mcrypt_generic_init( decrypt_fd, key, key_size, IV ) < 0 )
			error( "mcrypt_init failed" );
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

	STDERR_COPY = dup(STDERR_FILENO);
	dup2( newsocket_fd, STDIN_FILENO );
	dup2( newsocket_fd, STDOUT_FILENO );
	dup2( newsocket_fd, STDERR_FILENO );
	close(newsocket_fd);
	signal( SIGPIPE, sig_handler );

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
			fprintf( stderr, "execvp() failed" );
			exit(1);
		}
	}
	else {
		fprintf( stderr, "fork() failed" );
		exit(1);
	}

	exit(0);
} 
