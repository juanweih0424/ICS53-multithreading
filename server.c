#include <pthread.h>
#include <signal.h>
#include <server.h>
#include <pthread.h>
#include <stdint.h>
#include <protocol.h>
#include <poll.h>
pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t job_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t num_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t auction_lock = PTHREAD_MUTEX_INITIALIZER;
int total_num_msg = 0;
int prev_num_msg = 0;
int listen_fd;
volatile sig_atomic_t exitRequested = 0;

void free_auction(list_t *auc)
{
    node_t *temp = auc->head;
    while (temp != NULL)
    {
        node_t *temp_1 = temp->next;
        auction *a = temp->value;
        free(a->item_name);
        if (a->winner != NULL)
        {
            free(a->winner);
        }

        if (a->creator != NULL)
        {
            free(a->creator);
        }

        node_t *wth = a->watchers->head;
        while (wth != NULL)
        {
            node_t *wth1 = wth->next;
            free(wth);
            wth = wth1;
            free(wth1);
        }
        if (wth != NULL)
        {
            free(wth);
            wth = NULL;
        }
        free(a);
        a = NULL;
        free(temp);
        temp = temp_1;
        if (temp_1 != NULL)
        {
            free(temp_1);
        }
    }
    if (temp != NULL)
    {
        free(temp);
        temp = NULL;
    }
}

void free_job_struct(job *j)
{
    free(j->item_name);
    if (((auction *)(j->completed_auction))->creator != NULL)
    {
        free(((auction *)(j->completed_auction))->creator);
    }
    if (((auction *)(j->completed_auction))->winner != NULL)
    {
        free(((auction *)(j->completed_auction))->winner);
    }
    free(((auction *)(j->completed_auction))->item_name);

    node_t *temp = ((auction *)(j->completed_auction))->watchers->head;
    while (temp != NULL)
    {
        node_t *temp_1 = temp->next;
        free(temp);
        temp = temp_1;
        free(temp_1);
    }
    if (temp != NULL)
    {
        free(temp);
        temp = NULL;
    }
}

void free_all()
{
    // FREEING USERS LINKED LIST
    node_t *temp = users->head;
    while (temp != NULL)
    {
        user *u = temp->value;
        free(u->fp);
        free(u->password);
        free(u->username);
        // free won_auctions linked list of auctions in user
        free_auction(u->won_auctions);
        // free completed_auctions linked list of auctions in user
        free_auction(u->completed_auctions);
        temp = temp->next;
    }
    free(temp);
    temp = NULL;

    // FREEING JOB BUFFER LINKED LIST
    temp = job_buffer->head;
    while (temp != NULL)
    {
        job *j = temp->value;
        free_job_struct(j);
        temp = temp->next;
    }
    temp = threads->head;
    while (temp != NULL)
    {
        node_t *temp2 = temp->next;
        free(temp->value);
        temp = temp2;
    }

    // free linked lists
    free(users);
    users = NULL;
    free(auctions);
    auctions = NULL;
    free(threads);
    threads = NULL;
    free(job_buffer);
    job_buffer = NULL;
}

void sigint_handler()
{
    printf("handler called\n");
    close(listen_fd);
    exitRequested = 1;
    printf("thread list length %d\n", threads->length);
    node_t *t = threads->head;
    pthread_t tid;
    while (t != NULL)
    {
        tid = ((thread *)t->value)->pid;
        void **retval = NULL;
        printf("in cleanning loop\n");
        printf("cleanning thread %ld\n", *(pthread_t *)t->value);
        pthread_join(tid, retval);
        t = t->next;
    }
    free_all();
    exit(0);
}

int comparatorASC(void *lhs, void *rhs)
{
    if (lhs == NULL || rhs == NULL)
    {
        return 0;
    }

    if (((auction *)lhs)->id < ((auction *)rhs)->id)
    {
        return -1;
    }
    else if (((auction *)lhs)->id > ((auction *)rhs)->id)
    {
        return 1;
    }
    else if (((auction *)lhs)->id == ((auction *)rhs)->id)
    {
        if (((auction *)lhs)->id < ((auction *)rhs)->id)
        {
            return -1;
        }
        else if (((auction *)lhs)->id > ((auction *)rhs)->id)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    return 5;
}

char *getUserNameFromClientFd(int client_fd)
{
    node_t *temp = (node_t *)users->head;
    while (temp != NULL)
    {
        user *u = (user *)temp->value;
        if (u->client_fd == client_fd)
        {
            return u->username;
        }
        temp = temp->next;
    }
    return NULL;
}

char *convertInt(int num)
{
    int count = 0;
    int n = num;
    while (n != 0)
    {
        n = n / 10;
        count++;
    }
    if (n == 0)
    {
        count++;
    }
    char *string = malloc(count + 1);
    sprintf(string, "%d", num);
    printf("sprintf\n");
    *(string + count) = '\0';
    printf("sprintfA\n");
    return string;
}

char *getAuctionList(int client_fd)
{
    //char* string = malloc(1);
    node_t *temp = auctions->head;
    char *auctionlist = malloc(1);
    strcpy(auctionlist, "");
    int count = 0;
    int value = 0;
    while (temp != NULL)
    {
        count++;
        auction *a = (auction *)temp->value;
        char *id = convertInt(a->id);
        char *bin = convertInt(a->bin_price);
        char *num_watch = convertInt(a->num_users);
        char *duration = convertInt(a->duration);
        char *highest = convertInt(a->winning_bid);
        temp = temp->next;
        char *event;
        int length = 7 + strlen(id) + strlen(bin) + strlen(num_watch) + strlen(duration) + strlen(highest) + strlen(a->item_name);
        event = calloc(length, sizeof(char));
        auctionlist = realloc(auctionlist, length + strlen(auctionlist));
        value = length + strlen(auctionlist);
        printf("Value is %d\n", value);
        int index = 0;
        for (int b = 0; b < strlen(id); b++)
        {
            *(event + index) = *(id + b);
            index++;
        }
        *(event + index) = ';';
        index++;

        for (int b = 0; b < strlen(a->item_name); b++)
        {
            *(event + index) = *(a->item_name + b);
            index++;
        }
        strcat(event, ";");
        strcat(event, bin);
        strcat(event, ";");
        strcat(event, num_watch);
        strcat(event, ";");
        strcat(event, highest);
        strcat(event, ";");
        strcat(event, duration);
        strcat(event, "\n");
        strcat(auctionlist, event);
    }
    if (count == 0)
    {
        return NULL;
    }
    *(auctionlist + value) = '\0';

    return auctionlist;
}

void removeWatcherFromAuctionList(int clientfd)
{
    node_t *temp = auctions->head;
    while (temp != NULL)
    {
        auction *a = (auction *)temp->value;
        node_t *b = a->watchers->head;
        int index = 0;
        while (b != NULL)
        {
            watcher *w = (watcher *)b->value;
            if (w->client_fd == clientfd)
            {
                removeByIndex(a->watchers, index);
            }
            b = b->next;
            index++;
        }

        temp = temp->next;
    }
}

void printJobList()
{
    printf("PRINTING JOB BUFFER LINKED LIST\n");
    node_t *temp = job_buffer->head;
    while (temp != NULL)
    {
        job *event = ((job *)(temp->value));
        printf("\nclient_fd: %d\njob_type: %d\nauction_id: %d\nitem: %s\nduration: %d\nbin price: %d\nbid: %d\n",
               event->client_fd, event->job_type, event->auction_id, event->item_name, event->duration, event->bin_price, event->bid);
        temp = temp->next;
    }
}

void initialize_shared_structures()
{
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

int setup_auctions(char *auction_file)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    auction *event;

    fp = fopen(auction_file, "r");
    if (fp == NULL)
    {
        return -1;
    }

    int index = 0; // index to keep track of lines

    while (getline(&line, &len, fp) != -1)
    {
        if (strcmp(line, "\n") != 0)
        {

            char *ptr = line;
            // remove newline from line
            for (char c = *ptr; c; c = *++ptr)
            {
                if (c == '\n' || c == '\r')
                {
                    *ptr = '\0';
                }
            }

            if (index == 0)
            { // setup initial auction variables
                event = malloc(sizeof(auction));
                event->id = next_auction_id;
                next_auction_id++;
                event->num_users = 0;
                event->winner = NULL;
                event->winning_bid = 0;
                event->watchers = malloc(sizeof(list_t));
                list_t *w = event->watchers;
                w->head = NULL;
                w->comparator = NULL;
                w->length = 0;
                event->creator = AUCTIONFILE; // added when validating user
                event->item_name = calloc(strlen(line) + 1, sizeof(char));
                strcpy(event->item_name, line);
                *(event->item_name + strlen(line)) = '\0';
                index++;
            }
            else if (index == 1)
            { // duration
                event->duration = atoi(line);
                index++;
            }
            else if (index == 2)
            { // bin_price
                event->bin_price = atoi(line);
                insertRear(auctions, event);
                index = 0;
            }
        }
    }
    //printAuctionList();
    fclose(fp);
    if (line)
    {
        free(line);
    }
    return 0;
}

void associate_auctions(char *username)
{
    node_t *temp = auctions->head;
    while (temp != NULL)
    {
        auction *event = ((auction *)(temp->value));
        if (event->creator == NULL)
        {
            event->creator = calloc(1, strlen(username) + 1);
            strcpy(event->creator, username);
        }
        temp = temp->next;
    }
    //printAuctionList();
}

char *formatAuctionCloseMessage(auction *a)
{
    char *id = convertInt(a->id);
    char *str = NULL;
    if (a->winner == NULL)
    {
        str = malloc(strlen(id) + 5);
        strcpy(str, id);
        strcat(str, "\r\n\r\n");
    }
    else
    {
        char *winning_bid = convertInt(a->winning_bid);
        str = malloc(strlen(id) + strlen(winning_bid) + strlen(a->winner) + 5);
        strcpy(str, id);
        strcat(str, "\r\n");
        strcat(str, a->winner);
        strcat(str, "\r\n");
        strcat(str, winning_bid);
    }
    *(str + strlen(str)) = '\0';
    return str;
}

void addCompletedAuctionToUser(auction *a)
{
    node_t *temp = (node_t *)users->head;
    while (temp != NULL)
    {
        user *u = (user *)temp->value;
        if (u->username == a->creator)
        {
            insertInOrder(((list_t *)(u->completed_auctions)), a);
            u->sold = u->sold + a->winning_bid;
            break;
        }
        temp = temp->next;
    }
}

void updateWinnerBalance(auction *a)
{
    node_t *temp = (node_t *)users->head;
    while (temp != NULL)
    {
        user *u = (user *)temp->value;
        if (u->username == a->winner)
        {
            insertInOrder(((list_t *)(u->won_auctions)), a);
            u->bought = u->bought + a->winning_bid;
            break;
        }
        temp = temp->next;
    }
}

void addUser(char *username, char *password, int client_fd)
{
    user *newUser = malloc(sizeof(user));
    newUser->username = username;
    newUser->password = password;
    newUser->logged_in = 1;
    newUser->bought = 0;
    newUser->completed_auctions = malloc(sizeof(list_t));
    list_t *a = (list_t *)(newUser->completed_auctions);
    a->head = NULL;
    a->comparator = &comparatorASC;
    a->length = 0;
    newUser->selling = 0;
    newUser->sold = 0;
    newUser->won_auctions = malloc(sizeof(list_t));
    list_t *w_a = (list_t *)(newUser->won_auctions);
    w_a->head = NULL;
    w_a->comparator = &comparatorASC;
    w_a->length = 0;
    newUser->fp = NULL;
    newUser->client_fd = client_fd;
    insertRear(users, newUser);
}

user *getUserwithUsername(char *username)
{
    node_t *temp = (node_t *)(users->head);
    while (temp != NULL)
    {
        user *u = (user *)(temp->value);
        if (strcmp(u->username, username) == 0)
        {
            return u;
        }
        temp = temp->next;
    }
    return NULL;
}

void logoutUser(int client_fd)
{
    node_t *temp = (node_t *)(users->head);
    while (temp != NULL)
    {
        user *u = (user *)(temp->value);
        if (u->client_fd == client_fd)
        {
            u->client_fd = -1;
            u->logged_in = 0;
            break;
        }
        temp = temp->next;
    }
}

int validate_login(char *username, char *password, int client_fd)
{
    // return 0 means new user, 3 means existed user both sends ok, 1 is EUSRLGDIN 2 is EWRNGPWD
    node_t *temp = users->head;
    printf("insisde validate_login\n");
    while (temp != NULL)
    {
        user *u = (user *)temp->value;
        if (strcmp(u->username, username) == 0)
        { // match username
            if (strcmp(u->password, password) != 0)
            {
                return 2;
            } // password does not match
            else
            { // password matches
                if (u->logged_in == 1)
                {
                    return 1;
                } // user is already logged in
                else
                {
                    u->logged_in = 1;
                    u->client_fd = client_fd;
                    return 3;
                }
            }
        }
        u = NULL;
        temp = temp->next;
    }
    temp = NULL;
    addUser(username, password, client_fd);

    return 0;
}

int get_user_info(char *buffer, int client_fd)
{
    int i;
    // find size of username "<username>\r\n<password>"
    // "\r" == 13, "\n" == 10
    for (i = 0; i < strlen(buffer); i++)
    {
        if (*(buffer + i) == 13 || *(buffer + i) == 10)
        {
            break;
        }
    }
    printf("buffer length is %ld\n", strlen(buffer));
    printf("counter is %d\n", i);
    char *username = calloc(i + 1, sizeof(char));
    // get username
    for (int x = 0; x < i; x++)
    {
        *(username + x) = *(buffer + x);
    }
    printf("username after is %s\n", username);
    printf("length of username %ld\n", strlen(username));
    char *password = calloc(strlen(buffer) - 2 - i + 1, sizeof(char));
    int index = 0;
    // get password
    for (int y = i + 2; y < strlen(buffer); y++)
    {
        *(password + index) = *(buffer + y);
        index++;
    }
    printf("password is %s\n", password);
    //associate_auctions(username); // list user as creator for auction items
    printf("associate called\n");
    pthread_mutex_lock(&user_lock);
    printf("grabbed usermutex\n");
    int validate = validate_login(username, password, client_fd);
    pthread_mutex_unlock(&user_lock);
    printf("put user mutex back\n");
    printf("validate called\n");
    return validate;
}

char *get_all_username(int client_fd)
{
    char *string = malloc(1);
    int count = 0;
    long length = 0;
    node_t *temp = users->head;
    while (temp != NULL)
    {
        if (((user *)temp->value)->client_fd != client_fd)
        {
            if (((user *)temp->value)->logged_in == 1)
            {
                length += strlen(((user *)temp->value)->username);
                length++;
                if (count == 0)
                {
                    string = realloc(string, length);
                    strcpy(string, ((user *)temp->value)->username);
                    strcat(string, "\n");
                }
                else
                {
                    string = realloc(string, length);
                    strcat(string, ((user *)temp->value)->username);
                    strcat(string, "\n");
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

int server_init(int server_port)
{
    // establish socket connection
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1)
    {
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int option = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEPORT, (char *)&option, sizeof(option)) < 0)
    {
        exit(EXIT_FAILURE);
    }
    if (bind(socketfd, (SA *)&servaddr, sizeof(servaddr)) != 0)
    {
        exit(EXIT_FAILURE);
    }
    if (listen(socketfd, 1) != 0)
    {
        exit(EXIT_FAILURE);
    }
    else
    {
    }

    return socketfd;
}

void decrement_tickers()
{
    node_t *temp = auctions->head;
    while (temp != NULL)
    {
        auction *event = ((auction *)(temp->value));
        event->duration--;
        if (event->duration == 0)
        {
            // appending auction event to job_buffer for ANCLOSED
            auction *closed_auction = malloc(sizeof(auction));
            closed_auction->bin_price = event->bin_price;
            closed_auction->creator = event->creator;
            closed_auction->duration = 0;
            closed_auction->id = event->id;
            closed_auction->item_name = event->item_name;
            closed_auction->num_users = event->num_users;
            closed_auction->winner = event->winner;
            closed_auction->winning_bid = event->winning_bid;
            closed_auction->watchers = event->watchers;

            job *cmd = malloc(sizeof(job));
            cmd->completed_auction = closed_auction;
            cmd->job_type = ANCLOSED;
            cmd->auction_id = event->id;
            cmd->client_fd = -1;
            cmd->item_name = event->item_name;
            cmd->duration = 0;
            cmd->bin_price = event->bin_price;
            cmd->bid = event->winning_bid;

            insertRear(job_buffer, cmd);
        }
        temp = temp->next;
    }
}

void parse_auction_info(char *line, job *cmd)
{
    int index = 0;
    while (*(line + index) != '\r')
    {
        index++;
    }
    char *item_name = malloc(index + 1);
    strncpy(item_name, line, index);
    *(item_name + index + 1) = '\0';
    index += 2;
    printf("index is %d\n", index);
    int count = 0;
    while (*(line + index) != '\r')
    {
        index++;
        count++;
    }
    char *duration = malloc(count + 1);
    strncpy(duration, line + index - count, count);
    *(duration + count + 1) = '\0';
    printf("duration is %s\n", duration);
    index += 2;
    count = 0;
    while (*(line + index) != '\0')
    {
        index++;
        count++;
    }
    char *bin = malloc(count + 1);
    strncpy(bin, line + index - count, count);
    *(bin + count + 1) = '\0';
    printf("bin is %s\n", bin);
    cmd->item_name = item_name;
    cmd->duration = atoi(duration);
    cmd->bin_price = atoi(bin);
    printf("bin_price %d\n", cmd->bin_price);
    cmd->auction_id = next_auction_id;
    next_auction_id++;
    cmd->bid = 0;
}

void *process_tickers(void *_)
{

    while (1)
    {
        if (exitRequested == 1)
        {
            break;
        }

        if (dec_time == -1)
        {
            size_t bufsize = 32;
            char *buffer = malloc(32);
            size_t characters;
            struct pollfd pfds;
            pfds.fd = STDIN_FILENO;
            pfds.events = POLLIN;
            int ready = poll(&pfds, 1, 1);
            if (ready > 0 && (pfds.revents & POLLIN))
            {
                characters = getline(&buffer, &bufsize, stdin);
                if (characters > 0)
                {
                    pthread_mutex_lock(&job_lock);
                    printf("job lock grabbed\n");
                    pthread_mutex_lock(&auction_lock);
                    printf("auction lock grabbed\n");
                    decrement_tickers();
                    pthread_mutex_unlock(&auction_lock);
                    printf("unlock auction\n");
                    pthread_mutex_unlock(&job_lock);
                    printf("unlock job\n");
                }
                fflush(stdin);
            }
            free(buffer);
            buffer = NULL;
        }
        else
        {
            sleep(dec_time);
            pthread_mutex_lock(&job_lock);
            pthread_mutex_lock(&auction_lock);
            decrement_tickers();
            pthread_mutex_unlock(&auction_lock);
            pthread_mutex_unlock(&job_lock);
        }
    }
    return NULL;
}

char *watch(int id, int client_fd)
{
    printf("client fd is %d\n", client_fd);
    node_t *temp = auctions->head;
    char *string = malloc(1);
    int length = 0;
    while (temp != NULL)
    {
        auction *event = (auction *)temp->value;
        printf("in while\n");
        if (event->id == id)
        {
            printf("if called in while\n");
            char *item_name = event->item_name;
            char *bin_price = convertInt(event->bin_price);
            printf("item name and bin price rertreived\n");
            if (event->num_users >= 20)
            {
                printf("too many users\n");
                return "FULL";
            }
            else
            {
                event->num_users += 1;
            }
            printf("if checked\n");
            watcher *w = malloc(sizeof(watcher));
            w->client_fd = client_fd;
            insertRear(event->watchers, w);
            printf("realloc success and new watcher added\n");
            length = strlen(item_name) + strlen(bin_price) + 3;
            string = realloc(string, length);
            strcpy(string, item_name);
            strcat(string, "\r\n");
            strcat(string, bin_price);
            *(string + length) = '\0';
            printf("ANWATCH string is %s\n", string);
            return string;
        }
        printf("go next;\n");
        temp = temp->next;
    }
    printf("outside while\n");
    free(string);
    string = NULL;
    return NULL;
}

int leave(int id, int client_fd)
{
    printf("inside leave\n");
    node_t *temp = auctions->head;
    int result = 0;
    while (temp != NULL)
    {
        if (((auction *)temp->value)->id == id)
        {
            result = 1;
            int index = 0;
            node_t *w = (node_t *)(((list_t *)((auction *)(temp->value))->watchers)->head);
            while (w != NULL)
            {
                if (((watcher *)(w->value))->client_fd == client_fd)
                {
                    removeByIndex(((list_t *)((auction *)(temp->value))->watchers), index);
                    break;
                }
                index++;
                w = w->next;
            }
            ((auction *)temp->value)->num_users -= 1;
        }
        temp = temp->next;
    }
    return result;
}

void parse_bid(char *line, job *cmd)
{
    int id;
    int index = 0;
    while (*(line + index) != '\r')
    {
        index++;
    }
    char *auction_id = malloc(index + 1);
    strncpy(auction_id, line, index);
    *(auction_id + index + 1) = '\0';
    id = atoi(auction_id);
    cmd->auction_id = id;
    printf("auction id is %d\n", cmd->auction_id);
    index + 2;
    int count = 0;
    while (*(line + index) != '\0')
    {
        index++;
        count++;
    }
    char *bid = malloc(count + 1);
    strncpy(bid, line + index - count, count);
    *(bid + count + 1) = '\0';
    int bidprice = atoi(bid);
    cmd->bid = bidprice;
    printf("bid price is %d\n", cmd->bid);
    cmd->item_name = NULL;
    cmd->duration = -1;
    cmd->bin_price = -1;
}

char *update_parse(auction *auction)
{
    char *id = convertInt(auction->id);
    char *item_name = auction->item_name;
    char *username = auction->winner;
    char *bid = convertInt(auction->winning_bid);
    int length = 7 + strlen(id) + strlen(item_name) + strlen(username) + strlen(bid);
    char *string = malloc(length);
    strcpy(string, id);
    strcat(string, "\r\n");
    strcat(string, item_name);
    strcat(string, "\r\n");
    strcat(string, username);
    strcat(string, "\r\n");
    strcat(string, bid);
    *(string + length) = '\0';
    return string;
}

int validate_auction(int auction_id, int bid, int client_fd)
{
    node_t *temp = auctions->head;
    //EANNOTFOUND
    int result = 0;
    while (temp != NULL)
    {
        if (((auction *)temp->value)->id == auction_id)
        {
            //send OK
            result = 1;
            node_t *w = (node_t *)((list_t *)(((auction *)temp->value)->watchers)->head);
            int found = 0;
            while (w != NULL)
            {
                if (((watcher *)w->value)->client_fd == client_fd)
                {
                    found = 1;
                }
                w = w->next;
            }
            if (found == 1) // checking if the user is a creator
            {
                char *u = getUserNameFromClientFd(client_fd);
                if (strcmp(u, ((auction *)temp->value)->creator) == 0)
                {
                    //EANDENIED
                    result = 3;
                }
                // result = 1 here
            }
            else
            {
                //EDENIED // not in watch list
                result = 2;
            }
            if (result == 1)
            {
                if (bid > ((auction *)temp->value)->winning_bid)
                {
                    auction *a = (auction *)temp->value;
                    a->winning_bid = bid;
                    a->winner = getUserNameFromClientFd(client_fd);
                    node_t *watch = (node_t *)((list_t *)((a->watchers)->head));
                    while (watch != NULL)
                    {
                        petr_header *h = malloc(sizeof(petr_header));
                        h->msg_type = ANUPDATE;
                        char *string = update_parse(a);
                        h->msg_len = strlen(string) + 1;
                        printf("client fd for watcher %d\n", ((watcher *)watch->value)->client_fd);
                        wr_msg(((watcher *)watch->value)->client_fd, h, string);

                        char *u = getUserNameFromClientFd(((watcher *)watch->value)->client_fd);
                        if (strcmp(u, (a->winner)) == 0)
                        {
                            petr_header *h1 = malloc(sizeof(petr_header));
                            h1->msg_type = OK;
                            h1->msg_len = 0;
                            wr_msg(((watcher *)watch->value)->client_fd, h1, NULL);
                            printf("called times\n");
                        }
                        watch = watch->next;
                    }

                    if (bid >= (a->bin_price) && (a->bin_price != 0)) // should add a job into the job_buffer
                    {
                        auction *closed_auction = malloc(sizeof(auction));
                        closed_auction->bin_price = a->bin_price;
                        closed_auction->creator = a->creator;
                        closed_auction->duration = 0;
                        closed_auction->id = a->id;
                        closed_auction->item_name = a->item_name;
                        closed_auction->num_users = a->num_users;
                        closed_auction->winner = a->winner;
                        closed_auction->winning_bid = a->winning_bid;
                        closed_auction->watchers = a->watchers;

                        job *cmd = malloc(sizeof(job));
                        cmd->completed_auction = closed_auction;
                        cmd->job_type = ANCLOSED;
                        cmd->auction_id = a->id;
                        cmd->client_fd = -1;
                        cmd->item_name = a->item_name;
                        cmd->duration = 0;
                        cmd->bin_price = a->bin_price;
                        cmd->bid = a->winning_bid;

                        insertRear(job_buffer, cmd);
                    }
                }
                else
                {
                    //EBIDLOW
                    result = 4;
                }
            }
        }
        temp = temp->next;
    }
    return result;
}

char *getBalance(int client_fd)
{
    node_t *temp = users->head;
    int balance = 0;
    while (temp != NULL)
    {
        if (((user *)temp->value)->client_fd == client_fd)
        {
            balance = ((user *)temp->value)->sold - ((user *)temp->value)->bought;
        }
        temp = temp->next;
    }
    printf("outside while\n");
    printf("balance is %d\n", balance);
    char *b = convertInt(balance);
    return b;
}

char *formatUserWins(int client_fd)
{
    node_t *temp = users->head;
    char *str = NULL;
    int index = 0;
    while (temp != NULL)
    {
        user *u = (user *)temp->value;
        if (u->client_fd == client_fd)
        {
            node_t *temp2 = u->won_auctions->head;
            while (temp2 != NULL)
            {
                auction *a = temp2->value;
                char *aid = convertInt(a->id);
                char *item_name = a->item_name;
                char *winning_bid = convertInt(a->winning_bid);
                int length = strlen(aid) + strlen(item_name) + strlen(winning_bid) + 3;
                index += length;
                char *strtemp = malloc(length);
                strcpy(strtemp, aid);
                strcat(strtemp, ";");
                strcat(strtemp, item_name);
                strcat(strtemp, ";");
                strcat(strtemp, winning_bid);
                strcat(strtemp, "\n");
                if (str == NULL)
                {
                    str = malloc(index);
                    strcpy(str, strtemp);
                }
                else
                {
                    str = realloc(str, index);
                    strcat(str, strtemp);
                }
                temp2 = temp2->next;
            }
        }
        temp = temp->next;
    }
    str = realloc(str, index + 1);
    *(str + index + 1) = '\0';
    return str;
}

char *getSale(int client_fd)
{
    node_t *temp = users->head;
    char *str = NULL;
    int index = 0;
    while (temp != NULL)
    {
        user *u = (user *)temp->value;
        if (u->client_fd == client_fd)
        {
            node_t *temp2 = u->completed_auctions->head;
            while (temp2 != NULL)
            {
                auction *a = temp2->value;
                char *aid = convertInt(a->id);
                char *item_name = a->item_name;
                char *winner = a->winner;
                char *winning_bid = convertInt(a->winning_bid);
                if (a->winner == NULL)
                {
                    winner = "None";
                    winning_bid = "None";
                }

                int length = strlen(aid) + strlen(item_name) + strlen(winning_bid) + 4 + strlen(winner);
                index += length;
                char *strtemp = malloc(length);
                strcpy(strtemp, aid);
                strcat(strtemp, ";");
                strcat(strtemp, item_name);
                strcat(strtemp, ";");
                strcat(strtemp, winner);
                strcat(strtemp, ";");
                strcat(strtemp, winning_bid);
                strcat(strtemp, "\n");
                if (str == NULL)
                {
                    str = malloc(index);
                    strcpy(str, strtemp);
                }
                else
                {
                    str = realloc(str, index);
                    strcat(str, strtemp);
                }
                temp2 = temp2->next;
            }
        }
        temp = temp->next;
    }
    str = realloc(str, index + 1);
    *(str + index + 1) = '\0';
    return str;
}

void *process_jobs(void *_)
{
    printf("job thread started\n");
    printf("thread id is %ld\n", pthread_self());
    while (1)
    {
        if (exitRequested == 1)
        {
            break;
        }
        pthread_mutex_lock(&job_lock);
        if (job_buffer->head != NULL)
        {
            node_t *job_item = (node_t *)(job_buffer->head);
            job *cmd = (job *)(job_item->value);
            int client_fd = cmd->client_fd;
            petr_header *send = calloc(1, sizeof(petr_header));
            switch (cmd->job_type)
            {
            case LOGOUT:;
                pthread_mutex_lock(&user_lock);
                printf("case LOGOUT, locked!\n");
                pthread_mutex_lock(&auction_lock);
                //removeWatcherFromAuctionList(client_fd);
                printf("split\n");
                logoutUser(client_fd);
                pthread_mutex_unlock(&auction_lock);
                pthread_mutex_unlock(&user_lock);
                printf("case LOGOUT, unlocked!\n");

                send->msg_len = 0;
                send->msg_type = OK;

                wr_msg(client_fd, send, NULL);

                // close client
                close(client_fd);
                break;
            case ANCREATE:;
                printf("ancreate called\n");

                if (cmd->bin_price < 0 || cmd->duration < 1 || strcmp(cmd->item_name, "") == 0)
                {
                    send->msg_len = 0;
                    send->msg_type = EINVALIDARG;
                    wr_msg(client_fd, send, NULL);
                    break;
                }

                auction *event = malloc(sizeof(auction));
                event->id = cmd->auction_id;
                pthread_mutex_lock(&user_lock);
                printf("ancreate lock grab user\n");
                event->creator = getUserNameFromClientFd(client_fd);
                pthread_mutex_unlock(&user_lock);
                printf("ancreate lock unlocked user\n");
                event->item_name = cmd->item_name;
                event->bin_price = cmd->bin_price;
                event->num_users = 0;
                event->winner = NULL;
                event->duration = cmd->duration;
                event->watchers = calloc(0, sizeof(int));
                event->winning_bid = 0;

                pthread_mutex_lock(&auction_lock);
                printf("add new auction lock\n");
                insertRear(auctions, event);
                pthread_mutex_unlock(&auction_lock);
                printf("add new auction unlocked\n");

                char *auction_id = convertInt(cmd->auction_id);
                send->msg_len = strlen(auction_id) + 1;
                auction_id = realloc(auction_id, send->msg_len);
                *(auction_id + send->msg_len) = '\0';
                send->msg_type = ANCREATE;

                wr_msg(client_fd, send, auction_id);
                break;
            case ANCLOSED:;
                auction *a = (auction *)(cmd->completed_auction);
                printf("ANclosed\n");
                // update user infos
                pthread_mutex_lock(&user_lock);

                printf("anclose user grabbed\n");
                addCompletedAuctionToUser(a);
                updateWinnerBalance(a);
                pthread_mutex_unlock(&user_lock);
                printf("anclose user ungrabbed\n");

                // remove auction from list
                pthread_mutex_lock(&auction_lock);
                printf("anclose auction grabbed\n");
                node_t *temp = (node_t *)auctions->head;
                int index = 0;
                while (temp != NULL)
                {
                    auction *event = (auction *)temp->value;
                    if (event->id == a->id)
                    {
                        removeByIndex(auctions, index);
                        break;
                    }
                    index++;
                    temp = temp->next;
                }
                temp = NULL;

                char *send_msg = formatAuctionCloseMessage(a);
                printf("sendmsg %s\n", send_msg);
                send->msg_len = strlen(send_msg) + 1;
                send->msg_type = ANCLOSED;
                node_t *w = (node_t *)(((list_t *)((cmd->completed_auction)->watchers))->head);
                while (w != NULL)
                {
                    int c_fd = (int)((watcher *)w->value)->client_fd;
                    petr_header *h = malloc(sizeof(petr_header));
                    h->msg_type = ANCLOSED;
                    char *string = formatAuctionCloseMessage(cmd->completed_auction);
                    h->msg_len = strlen(string) + 1;
                    wr_msg(c_fd, h, string);
                    w = w->next;
                }
                pthread_mutex_unlock(&auction_lock);
                break;
            case ANLIST:;
                pthread_mutex_lock(&auction_lock);

                char *auctionlist = getAuctionList(client_fd);
                pthread_mutex_unlock(&auction_lock);
                if (auctionlist == NULL)
                {
                    send->msg_len = 0;
                }
                else
                {
                    send->msg_len = strlen(auctionlist) + 1;
                }
                send->msg_type = ANLIST;
                wr_msg(client_fd, send, auctionlist);
                break;
            case ANWATCH:;
                pthread_mutex_lock(&auction_lock);
                printf("grabbed\n");
                char *string = watch(cmd->auction_id, client_fd);
                pthread_mutex_unlock(&auction_lock);
                if (string == NULL)
                {
                    send->msg_type = EANNOTFOUND;
                    send->msg_len = 0;
                    wr_msg(client_fd, send, NULL);
                }
                else if (strcmp(string, "FULL") == 0)
                {
                    send->msg_type = EANFULL;
                    send->msg_len = 0;
                    wr_msg(client_fd, send, NULL);
                }
                else
                {
                    send->msg_type = ANWATCH;
                    send->msg_len = strlen(string) + 1;
                    wr_msg(client_fd, send, string);
                }
                break;
            case ANLEAVE:;
                printf("ANLEAVE\n");
                pthread_mutex_lock(&auction_lock);
                int result = leave(cmd->auction_id, client_fd);
                pthread_mutex_unlock(&auction_lock);
                printf("found auction? %d\n", result);
                if (result == 1)
                {
                    send->msg_len = 0;
                    send->msg_type = OK;
                }
                if (result == 0)
                {
                    send->msg_len = 0;
                    send->msg_type = EANNOTFOUND;
                }
                wr_msg(client_fd, send, NULL);

                break;
            case ANBID:;
                printf("anbid case\n");
                pthread_mutex_lock(&auction_lock);
                pthread_mutex_lock(&user_lock);
                int result1 = validate_auction(cmd->auction_id, cmd->bid, client_fd);
                pthread_mutex_unlock(&user_lock);
                pthread_mutex_unlock(&auction_lock);
                if (result1 == 0)
                {
                    printf("didnt find auction\n");
                    send->msg_type = EANNOTFOUND;
                    send->msg_len = 0;
                    wr_msg(client_fd, send, NULL);
                }
                else if (result1 == 2 || result1 == 3)
                {
                    printf("Denied\n");
                    send->msg_type = EANDENIED;
                    send->msg_len = 0;
                    wr_msg(client_fd, send, NULL);
                }
                else if (result1 == 4)
                {
                    send->msg_type = EBIDLOW;
                    send->msg_len = 0;
                    wr_msg(client_fd, send, NULL);
                }

                break;
            case USRLIST:;
                pthread_mutex_lock(&user_lock);
                printf("case USRLIST, locked!\n");
                char *userlist = get_all_username(client_fd);
                pthread_mutex_unlock(&user_lock);
                printf("case USRLIST, unlocked!\n");
                petr_header *send = calloc(1, sizeof(petr_header));
                if (userlist == NULL)
                {
                    send->msg_len = 0;
                    send->msg_type = USRLIST;
                }
                else
                {
                    send->msg_len = strlen(userlist) + 1;
                    send->msg_type = USRLIST;
                }
                wr_msg(client_fd, send, userlist);
                break;
            case USRWINS:;
                pthread_mutex_lock(&user_lock);
                char *winauction = formatUserWins(client_fd);
                pthread_mutex_unlock(&user_lock);

                petr_header *win = malloc(sizeof(petr_header));
                if (winauction == NULL)
                {
                    win->msg_len = 0;
                    win->msg_type = USRWINS;
                    wr_msg(client_fd, win, NULL);
                }
                else
                {
                    win->msg_type = USRWINS;
                    win->msg_len = strlen(winauction) + 1;
                    wr_msg(client_fd, win, winauction);
                }

                break;
            case USRSALES:;
                pthread_mutex_lock(&user_lock);
                char *sale = getSale(client_fd);
                pthread_mutex_unlock(&user_lock);
                petr_header *saleinfo = malloc(sizeof(petr_header));
                if (sale == NULL)
                {
                    saleinfo->msg_len = 0;
                    saleinfo->msg_type = USRSALES;
                    wr_msg(client_fd, saleinfo, NULL);
                }
                else
                {
                    saleinfo->msg_len = strlen(sale) + 1;
                    saleinfo->msg_type = USRSALES;
                    wr_msg(client_fd, saleinfo, sale);
                }

                break;
            case USRBLNC:
                printf("balance checking\n");
                pthread_mutex_lock(&user_lock);
                char *balance = getBalance(client_fd);
                pthread_mutex_unlock(&user_lock);
                petr_header *balanceInfo = malloc(sizeof(petr_header));
                balanceInfo->msg_len = strlen(balance) + 1;
                balanceInfo->msg_type = USRBLNC;

                printf("balance isB %s\n", balance);
                wr_msg(client_fd, balanceInfo, balance);
                printf("balance isC %s\n", balance);
                break;
            default:
                break;
            }
            removeFront(job_buffer); // get new job
        }
        pthread_mutex_unlock(&job_lock); // unlock thread after modifying job_buffer structure
    }
    return NULL;
}

void *process_client(void *clientfd_ptr)
{
    // read header, add jobs to linked list
    int client_fd = *(int *)clientfd_ptr;
    free(clientfd_ptr);
    while (1)
    {
        if (exitRequested == 1)
        {
            close(client_fd);
            break;
        }
        petr_header *h = malloc(sizeof(petr_header));
        if (rd_msgheader(client_fd, h) != -1)
        {
            printf("logged in \n");
            pthread_mutex_lock(&num_lock);
            total_num_msg++;
            pthread_mutex_unlock(&num_lock);
            // TODO: if-block should be moved to process_job
            // process_client should be creating job struct and adding to job_buffer struct for job threads to use
            job *cmd = malloc(sizeof(job));
            cmd->client_fd = client_fd;
            cmd->job_type = h->msg_type;

            // TODO: create functions to parse arguments and add them to job buffer
            if (h->msg_type == ANCREATE)
            {
                char *line = malloc((h->msg_len + 1));
                read(cmd->client_fd, line, h->msg_len);
                parse_auction_info(line, cmd);
            }
            else if (h->msg_type == ANWATCH)
            {
                int id;
                char *line = malloc((h->msg_len + 1));
                read(cmd->client_fd, line, h->msg_len);
                id = atoi(line);
                printf("auction id is %d\n", id);
                printf("%d\n", h->msg_type);
                cmd->auction_id = id;
                cmd->item_name = NULL;
                cmd->bin_price = -1;
                cmd->bid = -1;
                cmd->duration = -1;
            }
            else if (h->msg_type == ANLEAVE)
            {
                int id;
                char *line = malloc((h->msg_len + 1));
                read(cmd->client_fd, line, h->msg_len);
                id = atoi(line);
                printf("leave id is%d\n", id);
                printf("leanve? %d\n", h->msg_type);
                cmd->auction_id = id;
                cmd->item_name = NULL;
                cmd->bin_price = -1;
                cmd->bid = -1;
                cmd->duration = -1;
            }

            else if (h->msg_type == ANBID)
            {
                char *line = malloc((h->msg_len + 1));
                read(cmd->client_fd, line, h->msg_len);
                printf("anbid %s\n", line);
                parse_bid(line, cmd);
                printf("parsed_bid\n");
            }
            else
            {
                cmd->item_name = NULL;
                cmd->duration = -1;
                cmd->bin_price = -1;
                cmd->auction_id = -1;
                cmd->bid = -1;
            }
            pthread_mutex_lock(&job_lock);
            printf("insert into job \n");
            insertRear(job_buffer, cmd);
            //printJobList();
            pthread_mutex_unlock(&job_lock);
            printf("job lock back\n");

            if (h->msg_type == LOGOUT)
            {
                break;
            } // close client thread
        }
        free(h);
        h = NULL;
    }
    // // Close the socket at the end
    sleep(5); // allow threads to clean up
    // printf("Close current client connection\n");
    //
    return NULL;
}

void run_server(int server_port)
{
    //accept client
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    // signal(SIGINT, sigint_handler);
    while (1)
    {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA *)&client_addr, &client_addr_len);
        if (*client_fd < 0)
        {
            free(client_fd);
            client_fd = NULL;
        }
        else
        {
            petr_header *h = malloc(sizeof(petr_header));
            if (rd_msgheader(*client_fd, h) != -1)
            {

                pthread_t tid; // new thread for each client

                // get user login input
                char *login = malloc((h->msg_len + 1));

                read(*client_fd, login, h->msg_len);
                printf("buffer is %s\n", login);
                printf("msg_len is %d\n", h->msg_len);

                int validate = get_user_info(login, *client_fd);
                petr_header *send = calloc(1, sizeof(petr_header));
                printf("called header\n");
                if (validate == 0)
                {
                    printf("newuser\n");
                    send->msg_type = OK;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(login);
                    login = NULL;
                    pthread_create(&tid, NULL, process_client, (void *)client_fd);
                    thread *client = malloc(sizeof(thread));
                    client->pid = tid;
                    insertRear(threads, client);
                }
                else if (validate == 1)
                { // User is already logged in
                    send->msg_type = EUSRLGDIN;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(login);
                    login = NULL;
                }
                else if (validate == 2)
                { // Password does not match
                    send->msg_type = EWRNGPWD;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(login);
                    login = NULL;
                }
                else if (validate == 3)
                {
                    printf("existing user\n");
                    send->msg_type = OK;
                    send->msg_len = 0;
                    wr_msg(*client_fd, send, NULL);
                    free(login);
                    login = NULL;
                    pthread_create(&tid, NULL, process_client, (void *)client_fd);
                    thread *client = malloc(sizeof(thread));
                    client->pid = tid;
                    insertRear(threads, client);
                }
                free(send);
                send = NULL;
            }
            free(h);
            h = NULL;
        }
    }
    close(listen_fd);
}

int main(int argc, char *argv[])
{
    int opt;
    unsigned int port = 0;
    char *auction_file = NULL;
    int num_job_threads = 2;
    pthread_mutex_init(&user_lock, NULL);
    pthread_mutex_init(&job_lock, NULL);
    pthread_mutex_init(&num_lock, NULL);
    pthread_mutex_init(&auction_lock, NULL);
    initialize_shared_structures();

    while ((opt = getopt(argc, argv, "h:j:t:")) != -1)
    {
        switch (opt)
        {
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
    for (int index = optind; index < argc; index++)
    {
        if (port == 0)
        {
            port = atoi(argv[index]);
        }
        else if (auction_file == NULL)
        {
            auction_file = argv[index];
        }
    }

    // fail if non-optional arguments are missing
    if (port == 0 || auction_file == NULL)
    {
        fprintf(stderr, "ERROR: Missing non-optional arguments.\n");
        fprintf(stderr, "Server Application Usage: %s [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // parse auction file
    if (setup_auctions(auction_file) == -1)
    {
        fprintf(stderr, "ERROR: Failed to parse %s for auctions.\n", auction_file);
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, sigint_handler);
    // set up job threads and linked list of job threads to close upon exit
    for (int i = 0; i < num_job_threads; i++)
    { // create N job thread
        pthread_t job_thread;
        pthread_create(&job_thread, NULL, process_jobs, NULL);
        thread *t = malloc(sizeof(thread));
        t->pid = job_thread;
        insertRear(threads, t);
        printf("insert thread id %ld\n", job_thread);
    }

    pthread_t ticker_thread;
    pthread_create(&ticker_thread, NULL, process_tickers, NULL);
    thread *ticker = malloc(sizeof(thread));
    ticker->pid = ticker_thread;
    insertRear(threads, ticker);

    run_server(port);

    return 0;
}