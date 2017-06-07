// NAME: Bach Hoang
// ID: 104737449
// EMAIL: ko.wing.bird@gmail.com

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <mraa.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#define CELCIUS 'C'
#define FAHRENHEIT 'F'
#define OFF_LEN 3
#define STOP_LEN 4
#define START_LEN 5
#define SCALE_LEN 6
#define PERIOD_LEN 7
#define MAX_CMD_LEN 7

const int TEMP_SENSOR_PIN = 0;
const int B = 4275;
const int R0 = 100000;
const int TIMEOUT = 0;
char scale = FAHRENHEIT;
int period = 1;
time_t last_report_time = 0;

void print_usage(int rc) {
	fprintf( stderr, "Usage: lab4c [ihl] port#\n" );
	exit(rc);
}

void report( int fd, mraa_aio_context adc_a0 ) {
	uint16_t adc_value = 0;
	adc_value = mraa_aio_read(adc_a0);
	float R = 1023.0/adc_value - 1.0;
	R = R0*R;
	float temperature = 1.0/( log(R/R0)/B + 1/298.15 ) - 273.15;
	if( scale == FAHRENHEIT )
		temperature = temperature * 1.8 + 32;
	char time_buf[9];
	last_report_time = time(0);
	struct tm *local = localtime(&last_report_time);
	strftime( time_buf, sizeof(time_buf), "%H:%M:%S", local );
	dprintf( fd, "%s %2.1f\n", time_buf, temperature );
}

void report_shutdown( int fd ) {
	char time_buf[9];
	time_t now = time(0);
	struct tm *local = localtime(&now);
	strftime( time_buf, sizeof(time_buf), "%H:%M:%S", local );
	dprintf( fd, "%s SHUTDOWN\n", time_buf );
}

void report_cmd( int fd, char *cmd ) {
	dprintf( fd, "%s", cmd );
}

int main( int argc, char *argv[] ) {
	int portno = -1;
	int id = 123581321;
	char *host = "lever.cs.ucla.edu";
	int log_fl = 0;
	int stop_fl = 0;
	int log_fd;
	int opt = 0;
	static struct option long_opts[] =
	{
		{"id", required_argument, 0, 'i'},
		{"host", required_argument, 0, 'h'},
		{"log", required_argument, 0, 'l'},
		{0,0,0,0}
	};
	while( (opt = getopt_long(argc, argv, "i:h:l:", long_opts, NULL)) != -1 ) {
		switch(opt) {
 			case 'i': 
 				if( strlen(optarg) != 10 ) { // null-terminated
 					fprintf( stderr, "Using default since id is not a 9-digit-number\n" );
 				}
				id = atoi(optarg);
				break;
			case 'h': 
				host = optarg;
				break;
			case 'l': 
				log_fl = 1;
				log_fd = open( optarg, O_WRONLY );
				if( log_fd == -1 ) {
					fprintf( stderr, "Error creating log\n" );
					exit(1);
				}
				break;
			default:
				print_usage(1);
				break;
		}
	}
	if( optind < argc ) {
		for( size_t i = 0; i < strlen(argv[optind]); i++ ) {
			if( !isdigit(argv[optind][i]) || atoi(&argv[optind][i]) < 0 ) {
				fprintf( stderr, "Invalid port number\n" );
				exit(1);
			}
		}
		portno = atoi(argv[optind]);
	}
	if( portno == -1 ) {
		fprintf( stderr, "Missing port number\n" );
		exit(1);
	}

	// Initialization	
	mraa_aio_context adc_a0;
	adc_a0 = mraa_aio_init(TEMP_SENSOR_PIN);
	if( adc_a0 == NULL ) {
		fprintf( stderr, "Temperature sensor init failed\n" );
		exit(1);
	}

	SSL* SSLStruct;
	int socket_fd;
	struct sockaddr_in server_addr;
	struct hostent *server;
	// Create a socket point
	socket_fd = socket( AF_INET, SOCK_STREAM, 0 );
	if( socket_fd < 0 ) {
		fprintf( stderr, "Error opening socket\n" );
		exit(2);
	}
	server = gethostbyname(host);
	if( server == NULL ) {
		fprintf( stderr, "No such host\n" );
		exit(1);
	}
	// Init socket structure
	memset( (char*) &server_addr, 0, sizeof(server_addr) );
	server_addr.sin_family = AF_INET;
	memcpy( (char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length );
	server_addr.sin_port = htons(portno);

	// Connect to the server
	if( connect( socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr) ) < 0 ) {
		fprintf( stderr, "Error connecting\n" );
		exit(2);
	}

	// Setting SSL
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
	SSL_CTX* SSLClient = SSL_CTX_new(TLSv1_client_method());
	SSLStruct = SSL_new(SSLClient);
	if( SSL_set_fd(SSLStruct, socket_fd) == 0 ) {
		fprintf( stderr, "SSL_set_fd() failed\n" );
		exit(2);
	}
	if( SSL_connect(SSLStruct) != 1 ) {
		fprintf( stderr, "SSL_connect() failed\n" );
		exit(2);
	}

	// Writing id to server
	if( SSL_write( socket_fd, id, strlen(id)+1 ) == -1 ) {
		fprintf( stderr, "Error writing id" );
		exit(2);
	}	

	struct pollfd pfd[1];
	pfd[0].fd = socket_fd;
	pfd[0].events = POLLIN;
	int ret;
	time_t now;
	char cmd[MAX_CMD_LEN];

	while(1) {
		// Polling		
		ret = poll( pfd, 1, TIMEOUT );
		if( ret == -1 ) {
			fprintf( stderr, "poll() error\n" );
			exit(1);
		}
		if( pfd[0].revents & POLLIN ) {
			// Get and process commands
			SSL_read(socket_fd, cmd, MAX_CMD_LEN);
			if( strncmp( cmd, "OFF", OFF_LEN ) == 0 ) {
				if(log_fl) {
					report_cmd(log_fd, cmd);
					report_shutdown(log_fd);
				}
				exit(0);
			}
			else if( strncmp( cmd, "STOP", STOP_LEN ) == 0 ) {					
				stop_fl = 1;
				if(log_fl)
					report_cmd(log_fd, cmd);	
			}
			else if( strncmp( cmd, "START", START_LEN ) == 0 ) {	
				stop_fl = 0;
				if(log_fl)
					report_cmd(log_fd, cmd);			
			}
			else if( strncmp( cmd, "SCALE=", SCALE_LEN ) == 0 ) {	
				if( cmd[SCALE_LEN] == CELCIUS || cmd[SCALE_LEN] == FAHRENHEIT )
					scale = cmd[SCALE_LEN];
				else {
					  fprintf( stderr, "Argument must be 'F' or 'C'\n" );
					  exit(1);
				}
				if(log_fl)
					report_cmd(log_fd, cmd);	
			}	
			else if( strncmp( cmd, "PERIOD=", PERIOD_LEN ) == 0 ) {
				char *period_string = malloc( strlen(cmd)-PERIOD_LEN );
				strcpy( period_string, cmd+PERIOD_LEN );
				period = atoi(period_string);
				free(period_string);
				if(log_fl)
					report_cmd(log_fd, cmd);
			}
			else {
				if(log_fl)
					report_cmd(log_fd, cmd);
				exit(1);
			}		
		}
		// Reporting
		now = time(0);
		if( (now - last_report_time >= period) && !stop_fl ) {		
			if(log_fl)
				report( log_fd, adc_a0 );
			else
				report( socket_fd, adc_a0 );
		}	
	}
	close(socket_fd);
	SSL_shutdown(SSLStruct);
	return 0;
}