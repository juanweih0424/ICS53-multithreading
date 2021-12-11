#include <pthread.h>
#include <signal.h>
#include <server.h>
#include <pthread.h>
#include <stdint.h>
#include <protocol.h>

char buffer[BUFFER_SIZE];
pthread_mutex_t buffer_lock; 

int total_num_msg = 0;
int listen_fd;

void sigint_handler(int sig)

{
    close(listen_fd);

    // clean up threads
    node_t* temp = (node_t*)(threads->head);
    while (temp != NULL) { // close job threads
        void ** retval = NULL;
        pthread_join((pthread_t)(temp->value), retval);
    }

    // TODO: clean up data structures
    exit(EXIT_SUCCESS);
}

char* convertInt(int num)
{
    int count = 0;
    int n = num;
    while (n != 0)
    {
        n = n / 10;
        count++;
    }
    char* string = malloc(count+1);
    sprintf(string, "%d", num);
    
    return string;
}

char* getAuctionList(int client_fd) {
    //char* string = malloc(1);
    node_t* temp = auctions->head;
    
    while (temp != NULL){
        char* id = convertInt(((auction*)temp->value)->id);
        char* bin = convertInt(((auction*)temp->value)->bin_price);
        char* num_watch = convertInt(((auction*)temp->value)->num_users);
        char* duration = convertInt(((auction*)temp->value)->duration);
        char* highest = convertInt(((auction*)temp->value)->winning_bid);
        temp = temp->next;
    }


    return NULL;
}



void printJobList() {
    printf("PRINTING JOB BUFFER LINKED LIST\n");
    node_t* temp = job_buffer->head;
    while (temp != NULL){
        job* event = ((job*)(temp->value));
        printf("\nclient_fd: %d\njob_type: %d\nauction_id: %d\nitem: %s\nduration: %d\nbin price: %d\nbid: %d\n", event->client_fd, event->job_type, event->auction_id, event->item_name, event->duration, event->bin_price, event->bid);
        temp = temp->next;
    }
}

void initialize_shared_structures(){
    // malloc pointer space
    users = malloc(sizeof(list_t));
    auctions = malloc(sizeof(list_t));
    job_buffer = malloc(sizeof(list_t));
    threads = malloc(sizeof(list_t));

    // initialize length
    users->length = 0;
    auctions->length = 0;
    job_buffer->length = 0;
    threads->length = 0;

    // initialize comparator
    users->comparator = NULL;
    auctions->comparator = NULL;
    job_buffer->comparator = NULL;
    threads->comparator = NULL;

    // initialize head
    users->head = NULL;
    auctions->head = NULL;
    job_buffer->head = NULL;
    threads->head = NULL;
}

int setup_auctions(char* auction_file){
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    auction* event;

    fp = fopen(auction_file, "r");
    if (fp == NULL) {return -1;}

    int index = 0; // index to keep track of lines

    while (getline(&line, &len, fp) != -1) {
        if (strcmp(line, "\n") != 0) {
            
            char* ptr = line;
            // remove newline from line
            for (char c = *ptr; c; c=*++ptr) {if (c == '\n') {*ptr = '\0';}}

            if (index == 0) { // setup initial auction variables
                event = malloc(sizeof(auction));
                event->id = next_auction_id;
                next_auction_id++;
                event->num_users = 0;
                event->winner = NULL;
                event->winning_bid = -1;
                event->creator = NULL; // added when validating user
                event->item_name = calloc(1, strlen(line)+1);
                strcpy(event->item_name, line);
                index++;
            }
            else if (index == 1) { // duration
                event->duration = atoi(line);
                index++;
            }
            else if (index == 2) { // bin_price
                event->bin_price = atoi(line);
                insertRear(auctions, event);
                index = 0;
            }
        }
    }
    //printAuctionList();
    fclose(fp);
    if (line) {free(line);}
    return 0;
}

void associate_auctions(char* username){
    node_t* temp = auctions->head;
    while (temp != NULL){
        auction* event = ((auction*)(temp->value));
        if (event->creator == NULL) {
            event->creator = calloc(1, strlen(username)+1);
            strcpy(event->creator, username);
        }
        temp = temp->next;
    }
    //printAuctionList();
}

void addUser(char* username, char* password, int client_fd) {
    user* newUser = malloc(sizeof(user));
    newUser->username = username;
    newUser->password = password;
    newUser->logged_in = 1;
    newUser->bought = 0;
    newUser->completed_auctions = NULL;
    newUser->selling = 0;
    newUser->sold = 0;
    newUser->won_auctions = 0;
    newUser->fp = NULL;
    newUser->client_fd = client_fd;
    insertRear(users, newUser);
}

user* getUserwithUsername(char* username){
    node_t* temp = (node_t*)(users->head);
    while (temp != NULL) {
        user* u = (user*)(temp->value);
        if (strcmp(u->username, username) == 0) {return u;}
        temp = temp->next;
    }
    return NULL;
}

void logoutUser(int client_fd) {
    node_t* temp = (node_t*)(users->head);
    while (temp != NULL) {
        user* u = (user*)(temp->value);
        if (u->client_fd == client_fd) {
            u->client_fd = -1;
            u->logged_in = 0;
            break;
        }
        temp = temp->next;
    }
}

int validate_login(char* username, char* password, int client_fd) {
    // return 0 means new user, 3 means existed user both sends ok, 1 is EUSRLGDIN 2 is EWRNGPWD
    node_t* temp = users->head;
    printf("insisde validate_login\n");
    while (temp != NULL) {
        user* u = (user*)temp->value;
        if (strcmp(u->username, username) == 0) { // match username
            if (strcmp(u->password, password) != 0) {return 2;} // password does not match
            else { // password matches
                if (u->logged_in == 1) {return 1;} // user is already logged in
                else {
                    u->logged_in = 1;
                    u->client_fd = client_fd;
                    return 3;
                } 
            }
        }
        temp = temp->next;
    }
    printf("while lopp done\n");
    free(temp);
    temp = NULL;
    printf("ball grabbed\n");
    addUser(username, password, client_fd);

    printf("ball let go\n");
    return 0;
}

int get_user_info(char* buffer, int client_fd) {
    int i;
    // find size of username "<username>\r\n<password>"
    // "\r" == 13, "\n" == 10
    for (i = 0 ; i < strlen(buffer); i++) {if (*(buffer+i) == 13 || *(buffer+i) == 10) {break;}}
    printf("buffer length is %ld\n", strlen(buffer));
    printf("counter is %d\n", i);
    char* username = calloc(i+1, sizeof(char));
    // get username
    for (int x = 0; x < i; x++){*(username+x) = *(buffer+x);}
    printf("username after is %s\n", username);
    printf("length of username %ld\n", strlen(username));
    char* password = calloc(strlen(buffer)-2-i+1, sizeof(char));
    int index = 0;
    // get password
    for (int y = i+2; y < strlen(buffer); y++) {
        *(password+index) = *(buffer+y);
        index++;
    }
    printf("password is %s\n", password);
    associate_auctions(username); // list user as creator for auction items
    printf("associate called\n");
    int validate = validate_login(username, password, client_fd);
     printf("validate called\n");
    return validate;
}

char* get_all_username(int client_fd)
{
    char* string = malloc(1);
    int count = 0;
    long length = 0;
    node_t* temp = users->head;
    while (temp != NULL)
    {
        if (((user*)temp->value)->client_fd != client_fd)
        {
            if (((user*)temp->value)->logged_in == 1)
            {
                length += strlen(((user*)temp->value)->username);
                length++;
                if (count == 0)
                {
                    string = realloc(string, length);
                    strcpy(string, ((user*)temp->value)->username);
                    strcat(string,"\n");
                }
                else
                {
                    string = realloc(string, length);
                    strcat(string, ((user*)temp->value)->username);
                    strcat(string,"\n");
                }
                count++;
            }
        }
        temp = temp->next;
    }
    free(temp);
    temp = NULL;
    if (count == 0)
    {
        return NULL;
    }
    length++;
    string = realloc(string, length);
    strcat(string, "\0");

    return string;
}

int server_init(int server_port) {
    // establish socket connection
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {exit(EXIT_FAILURE);}

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int option = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEPORT, (char *)&option, sizeof(option)) < 0) {exit(EXIT_FAILURE);}
    if (bind(socketfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {exit(EXIT_FAILURE);}
    if (listen(socketfd, 1) != 0) {exit(EXIT_FAILURE);}
    else {}

    return socketfd;
}

void decrement_tickers(void* _) {
    // check for finished auctions and add to job thread to close auction
    // use total num messages for debug mode
    if (dec_time == -1) {
        while (1) {
            break;
            // decrease auctions->duration every input
        }
    }
    else{
        while(1) {
            sleep(dec_time); 
            // decrease auction->duration every dec_time-seconds
            break;
        }
        
    }
}



void* process_jobs(void* _) { 
    while (1)
    {
        /* code */
    }
    
    
    while(1){
        pthread_mutex_lock(&buffer_lock);
        if (job_buffer->head != NULL) {
            node_t* job_item = (node_t*)(job_buffer->head);
            job* cmd = (job*)(job_item->value);
            int client_fd = cmd->client_fd;      
            switch (cmd->job_type) {
                case LOGOUT: 
                    ;
                    logoutUser(client_fd);
                    
                    petr_header* send = calloc(1, sizeof(petr_header));
                    send->msg_len = 0;
                    send->msg_type = OK;
                    char* newbuffer = NULL;
                    wr_msg(client_fd, send, newbuffer);

                    // close client
                    close(client_fd);
                    break;
                case ANCREATE:
                    
                    break;
                case ANCLOSED:
                    break;
                case ANLIST:
                    ;
                    char* auctionlist = getAuctionList(client_fd);
                    break;
                case ANWATCH:
                    break;
                case ANLEAVE:
                    break;
                case ANBID:
                    break;
                case ANUPDATE:
                    break;
                case USRLIST:
                    ;
                    petr_header* send_list = malloc(sizeof(petr_header));
                    char* userlist = get_all_username(client_fd);
                    if (userlist == NULL)
                    {
                        send_list->msg_len = 0;
                        send_list->msg_type = USRLIST;
                    }
                    else
                    {                                 
                        send_list->msg_len = strlen(userlist) + 1;
                        send_list->msg_type = USRLIST;    
                    }
                    wr_msg(client_fd, send_list, userlist);
                    free(userlist);
                    userlist = NULL;
                    break;
                case USRWINS:
                    break;
                case USRSALES:
                    break;
                case USRBLNC:
                    break;
                default:
                    break;
            }
            removeFront(job_buffer); // get new job
        }
        pthread_mutex_unlock(&buffer_lock); // unlock thread after modifying job_buffer structure
    }
    return NULL;
}

void *process_client(void* clientfd_ptr){
    // read header, add jobs to linked list
    int client_fd = *(int *)clientfd_ptr;
    free(clientfd_ptr);
    pthread_detach(pthread_self());
    while(1){
        petr_header* h = malloc(sizeof(petr_header));
        if (rd_msgheader(client_fd, h) != -1) {
            total_num_msg++;
            // TODO: if-block should be moved to process_job
            // process_client should be creating job struct and adding to job_buffer struct for job threads to use
            job* cmd = malloc(sizeof(job));
            cmd->client_fd = client_fd;
            cmd->job_type = h->msg_type;
            
            // TODO: create functions to parse arguments and add them to job buffer
            if (h->msg_type == ANCREATE) {
                //get_ancreate_args(); // TODO
            }
            else if (h->msg_type == ANWATCH || h->msg_type == ANLEAVE) {
                //get_anwatch_anleave_args(); // TODO
            }
            else if (h->msg_type == ANBID) {
                //get_anbid_args(); // TODO
            }
            else {
                cmd->item_name = NULL;
                cmd->duration = -1;
                cmd->bin_price = -1;
                cmd->auction_id = -1;
                cmd->bid = -1;
            }
            pthread_mutex_lock(&buffer_lock);
 
            insertRear(job_buffer, cmd);

            //printJobList();

            pthread_mutex_unlock(&buffer_lock);
 
            if (h->msg_type == LOGOUT) {break;} // close client thread
        }
        free(h);
    }
    // // Close the socket at the end
    sleep(5); // allow threads to clean up
    // printf("Close current client connection\n");
    // close(client_fd);
    return NULL;
}

void run_server(int server_port){
    //accept client
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while(1){
        
        // Wait and Accept the connection from client

        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
        if (*client_fd < 0) {
            exit(EXIT_FAILURE); // TODO: send client error
        }
        else{

            petr_header* h = malloc(sizeof(petr_header));
            if (rd_msgheader(*client_fd, h) != -1) {
            
                pthread_t tid; // new thread for each client

                // get user login input
                char* newbuffer = malloc((h->msg_len+1));
                read(*client_fd, newbuffer, h->msg_len);
                printf("buffer is %s\n", newbuffer);
                printf("msg_len is %d\n", h->msg_len);
                int validate = get_user_info(newbuffer, *client_fd);
                petr_header* send = calloc(1, sizeof(petr_header));
                printf("called header\n");
                if (validate == 0) {
                    printf("newuser\n");
                    send->msg_type = OK;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(newbuffer);
                    newbuffer = NULL;
                    pthread_create(&tid, NULL, process_client, (void*)client_fd);
                }
                else if (validate == 1){ // User is already logged in
                    send->msg_type = EUSRLGDIN;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(newbuffer);
                    newbuffer = NULL;
                }
                else if (validate == 2) { // Password does not match
                    send->msg_type = EWRNGPWD;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(newbuffer);
                    newbuffer = NULL;
                }
                else if (validate == 3) {
                    printf("existing user\n");
                    send->msg_type = OK;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(newbuffer);
                    newbuffer = NULL;
                    pthread_create(&tid, NULL, process_client, (void*)client_fd);
                }
            }
        }
    }
    bzero(buffer, BUFFER_SIZE);
    close(listen_fd);
}



int main(int argc, char* argv[])
{
    int opt;
    unsigned int port = 0;
    char* auction_file = NULL;
    int num_job_threads = 2;

    initialize_shared_structures();

    while ((opt = getopt(argc, argv, "h:j:t:")) != -1) {
        switch (opt) {
        case 'j':
            num_job_threads = atoi(optarg); 
            break;
        case 't':
            dec_time = atoi(optarg);
            break;
        case 'h':
            printf(HELP_MSG);
            exit(EXIT_SUCCESS);
        default: /* '?' */
            fprintf(stderr, HELP_MSG);
            exit(EXIT_FAILURE);
        }
    }

    // getting non-option argument
    for (int index = optind; index < argc; index++) {
        if (port == 0) {port = atoi(argv[index]);}
        else if (auction_file == NULL) {auction_file = argv[index];}
    }

    // fail if non-optional arguments are missing
    if (port == 0 || auction_file == NULL){
        fprintf(stderr, "ERROR: Missing non-optional arguments.\n");
        fprintf(stderr, "Server Application Usage: %s [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // parse auction file
    if (setup_auctions(auction_file) == -1) {
        fprintf(stderr, "ERROR: Failed to parse %s for auctions.\n", auction_file);
        exit(EXIT_FAILURE);
    }
    
    // set up job threads and linked list of job threads to close upon exit
    for (int i = 0; i < num_job_threads; i++){ // create N job thread
        pthread_t job_thread;
        pthread_create(&job_thread, NULL, process_jobs, NULL);
        insertRear(threads, &job_thread);
    }

    // create ticker thread
    // pthread_t ticker_thread;
    //pthread_create(&ticker_thread, NULL, decrement_tickers, NULL);
    //insertRear(threads, &ticker_thread);
    run_server(port);

    // // clean up threads
    // node_t* temp = (node_t*)(threads->head);
    // while (temp != NULL) { // close job threads
    //     void ** retval = NULL;
    //     pthread_join((pthread_t)(temp->value), retval);
    // }

    return 0;
}