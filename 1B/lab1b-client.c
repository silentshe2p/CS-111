// NAME:  BACH HOANG
// ID:    104737449
// EMAIL: ko.wing.bird@gmail.com

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termio.h>
#include <getopt.h>
#include <poll.h>
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
const char FILL_IV = 'A';
struct termios saved_attr;
int TIMEOUT = -1;
int log_fl = 0;
int crypt_fl = 0;
int log_fd, socket_fd;
int log_count = 0;
int log_idx = 0;
char log_buf[BUF_SIZE];
MCRYPT encrypt_fd, decrypt_fd;
char *key;
char *IV;
int key_size = 16;

void error( char *msg ) {
	fprintf( stderr, "%s\n", msg );
	exit(1);
}

void print_usage(int rc) {
	fprintf( stderr, "Usage: lab1b-client [le] port=port#" );
	exit(rc);
}

void reset_input_mode(void) {
	tcsetattr( STDIN_FILENO, TCSANOW, &saved_attr );	
	if(crypt_fl) {
		mcrypt_generic_deinit(encrypt_fd);
		mcrypt_module_close(encrypt_fd);
		mcrypt_generic_deinit(decrypt_fd);
		mcrypt_module_close(decrypt_fd);		
	}
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

void write_log( int receiving ) {
	int high = log_count/10;
	int low = log_count%10;
	if(receiving) {
		char start[22] = "RECEIVED xx byte(s):";
		start[9] = '0' + high;
		start[10] = '0' + low;
		if( write( log_fd, start, 22 ) == -1 )
			error( "write() failed" );
	}
	else {
		char start[17] = "SENT xx byte(s):";
		start[5] = '0' + high;
		start[6] = '0' + low;
		if( write( log_fd, start, 17 ) == -1 )
			error( "write() failed" );
	}
				
	if( write( log_fd, log_buf, log_count ) == -1 )
		error( "write() failed" );

	char nl = '\n';	
	if( write( log_fd, &nl, 1 ) == -1 )
		error( "write() failed" );				
}

void read_write( int r_fd, int w_fd ) {
	char buf[BUF_SIZE];
	int b_read = read( r_fd, buf, BUF_SIZE );

	if( b_read <= 0 )
		exit(1);
	if(log_fl)
		log_count += b_read;
	if( crypt_fl && r_fd != STDIN_FILENO ) {
		if( mdecrypt_generic( decrypt_fd, buf, b_read ) != 0 )
			error( "Decrypting failed" );
	}

	for( int i = 0; i < b_read; i++ ) {
		if( write( STDOUT_FILENO, buf+i, 1 ) == -1 )
			error( "write() failed" );
		if( w_fd != STDOUT_FILENO ) {
			if(crypt_fl) {
				if( mcrypt_generic( encrypt_fd, buf, b_read ) != 0 )
					error( "Encrypting failed" );
			}
			if( write( w_fd, buf+i, 1 ) == -1 )
				error( "write() failed" );		
		}
		if( log_fl ) {		
			if( buf[i] == CR || buf[i] == LF || buf[i] == CTRL_C || buf[i] == CTRL_D ) {
				log_count -= 1;
			}	
			else {
				log_buf[log_idx] = buf[i];
				log_idx += 1;
			}						
		}
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
	struct sockaddr_in server_addr;
	struct hostent *server;

	static struct option long_opts[] =
	{
		{"port", 	required_argument, 0, 'p'},
		{"log", 	required_argument, 0, 'l'},
		{"encrypt", 	required_argument, 0, 'e'}
	};

	while( (opt = getopt_long( argc, argv, "p:l:e:", long_opts, NULL )) != -1 ) {
		switch(opt) {
			case 'p': 
				portno = atoi(optarg);
				break;
			case 'l':
				log_fl = 1;
				log_fd = creat( optarg, S_IRWXU );
				if( log_fd == -1 )
					error( "Error creating log" );
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
	set_input_mode();

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

	int log_switch = 0;
	while(1) {
		int ret = poll(pfds, 2, TIMEOUT);
		if( ret == -1 )
			error( "poll() error" );
		if( pfds[0].revents & POLLIN ) {
			if( log_fl && log_switch == 1 ) {
				write_log(0);
				log_count = 0;
				log_idx = 0;
				log_switch = 0;
			}
			read_write( pfds[0].fd, socket_fd );
		}
		if( pfds[1].revents & POLLIN ) {
			if( log_fl && log_switch == 0 ) {
				write_log(1);
				log_count = 0;
				log_idx = 0;
				log_switch = 1;
			}
			read_write( pfds[1].fd, STDOUT_FILENO );
		}
		if( (pfds[0].revents & (POLLHUP+POLLERR)) || (pfds[1].revents & (POLLHUP+POLLERR)) )
			exit(1);					
	}	
	exit(0);
}
