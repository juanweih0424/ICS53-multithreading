#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "linkedlist.h"

#define BUFFER_SIZE 1024
#define SA struct sockaddr
#define HELP_MSG \
"./bin/zbid_client [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME\n\n\
-h\t\t\tDisplays this help menu, and returns EXIT_SUCCESS.\n\
-j N\t\t\tNumber of job threads. If option not specified, default to 2.\n\
-t M\t\t\tM seconds between time ticks. If option not specified, default is to \
be in debug mode and only tick upon input from stdin e.g. a newline is entered \
then the server ticks once.\n\
PORT_NUMBER\t\tPort number to listen on.\n\
AUCTION_FILENAME\tFile to read auction item information from at the start of the server.\n"


int next_auction_id = 1;
int dec_time = -1; // debug mode, only decrease ticks upon input

// shared data structures
// shared data structures
list_t* users = NULL;
list_t* auctions = NULL;
list_t* job_buffer = NULL;
list_t* threads = NULL;

// USRLIST - linked list of users
typedef struct user {
    char* username;
    char* password;
    FILE* fp;
    int client_fd;
    // for USRWINS
    int won_auctions;
    // for USRBLNC
    int selling; // total price items are selling for
    int sold; // total gains
    int bought; // total costs
    int logged_in; // 0 not logged in, 1 logged in
    // for USRSALES
    struct list_t* completed_auctions;
} user;

typedef struct auction {
    int id;
    char* creator;
    char* item_name;
    int bin_price;
    int duration;
    int num_users;
    char* winner;
    int winning_bid;
} auction;

typedef struct job {
    int job_type;
    int client_fd;
    // possible arguments for job
    int auction_id;
    char* item_name;
    int duration;
    int bin_price;
    int bid;
} job;

int validate_login(char* username, char* password, int client_fd);
void addUser(char* username, char* password, int client_fd);
int server_init(int server_port);
void run_server(int server_port);
void* process_client(void* clientfd_ptr);
#endif