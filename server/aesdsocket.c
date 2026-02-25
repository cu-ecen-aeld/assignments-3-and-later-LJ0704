#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/stat.h>  // Added for umask
#include <errno.h>
#include <time.h>


//References: Steps for socket is follwed as per the coursera lecture 
//Gemini Reference: https://gemini.google.com/share/07982bad0784
//Linux Manual Reference : https://linux.die.net/man/3/htons
//Gemini Reference : https://gemini.google.com/share/c84c7f7c4547
//
// --- Fix for missing SLIST_FOREACH_SAFE ---
#ifndef SLIST_FOREACH_SAFE
#define SLIST_FOREACH_SAFE(var, head, field, tvar) \
    for ((var) = SLIST_FIRST((head)); \
        (var) && ((tvar) = SLIST_NEXT((var), field), 1); \
        (var) = (tvar))
#endif

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10
#define BUF_SIZE 1024

static volatile sig_atomic_t stop = 0;
static int global_sockfd = -1;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_info_s {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr_in client_addr;
    int thread_complete;
    SLIST_ENTRY(thread_info_s) entries;
};

SLIST_HEAD(slisthead, thread_info_s) head = SLIST_HEAD_INITIALIZER(head);

/* --- SIGNAL HANDLER --- */
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        stop = 1;
        if (global_sockfd != -1) {
            close(global_sockfd);
            global_sockfd = -1;
        }
    }
}


int check_daemon() {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    if (setsid() < 0) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    umask(0); // Now has <sys/stat.h>
    if (chdir("/") < 0) return -1;

    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
    return 0;
}

/* --- TIMESTAMP THREAD --- */
void* timestamp_handler(void* arg) {
    (void)arg; // Silence unused parameter warning
    while (!stop) {
        for (int i = 0; i < 10 && !stop; i++) sleep(1);
        if (stop) break;

        time_t rawtime;
        struct tm info;
        char time_buf[100];
        char final_output[150];

        time(&rawtime);
        localtime_r(&rawtime, &info);
        strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S %z", &info);
        int len = snprintf(final_output, sizeof(final_output), "timestamp:%s\n", time_buf);

        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (fd != -1) {
            write(fd, final_output, len);
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}


void* thread_handler(void* thread_param) {
    struct thread_info_s *data = (struct thread_info_s *)thread_param;
    char rx_buf[BUF_SIZE];
    char *packet = NULL;
    size_t total_received = 0;

    while (!stop) {
        ssize_t bytes = recv(data->client_fd, rx_buf, BUF_SIZE, 0);
        if (bytes <= 0) break;

        char *new_packet = realloc(packet, total_received + bytes);
        if (!new_packet) {
            free(packet);
            packet = NULL;
            break;
        }
        packet = new_packet;
        memcpy(packet + total_received, rx_buf, bytes);
        total_received += bytes;

        if (memchr(packet, '\n', total_received) != NULL) {
            pthread_mutex_lock(&file_mutex);
            int fd = open(DATA_FILE, O_CREAT | O_RDWR | O_APPEND, 0644);
            if (fd != -1) {
                write(fd, packet, total_received);
                lseek(fd, 0, SEEK_SET);
                ssize_t r;
                while ((r = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
                    send(data->client_fd, rx_buf, r, 0);
                }
                close(fd);
            }
            pthread_mutex_unlock(&file_mutex);

            free(packet);
            packet = NULL;
            total_received = 0;
        }
    }

    if (packet) free(packet);
    close(data->client_fd);
    data->thread_complete = 1;
    return NULL;
}


int main(int argc, char *argv[]) {
    int daemon_mode = (argc > 1 && strcmp(argv[1], "-d") == 0);
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    //Step1: Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM,0); // xxxx IPv4,Stream Socket, Protocol 0 
	
	if(sockfd == -1)
	{
		syslog(LOG_ERR,"socket failed");
		return -1;
	}
    global_sockfd = sockfd;

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

//Step 2: Binding
	int result = bind(sockfd,(struct sockaddr *)&addr,sizeof(addr)) ;
	
	if(result < 0)
	{
		syslog(LOG_ERR,"Bind failed");
		close(sockfd);
		return -1;
	}


    if (daemon_mode && check_daemon() != 0) 
    {
      return -1;
    }
 
	//Step3: Listen to socket	
	if(listen(sockfd,BACKLOG) == -1)
	{
		if (global_sockfd != -1){
		syslog(LOG_ERR,"Listen failed");
		close(sockfd);
		}
		return -1;
	}
    pthread_t time_thread;
    pthread_create(&time_thread, NULL, timestamp_handler, NULL);

    while (!stop) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);

        if (client_fd < 0) {
            if (stop) break;
            continue;
        }

        struct thread_info_s *new_t = calloc(1, sizeof(struct thread_info_s));
        new_t->client_fd = client_fd;
        new_t->client_addr = client_addr;

        if (pthread_create(&new_t->thread_id, NULL, thread_handler, new_t) != 0) {
            free(new_t);
            close(client_fd);
        } else {
            SLIST_INSERT_HEAD(&head, new_t, entries);
        }

        struct thread_info_s *it, *tmp;
        SLIST_FOREACH_SAFE(it, &head, entries, tmp) {
            if (it->thread_complete) {
                pthread_join(it->thread_id, NULL);
                SLIST_REMOVE(&head, it, thread_info_s, entries);
                free(it);
            }
        }
    }

    pthread_join(time_thread, NULL);
    struct thread_info_s *it, *tmp;
    SLIST_FOREACH_SAFE(it, &head, entries, tmp) {
        pthread_join(it->thread_id, NULL);
        SLIST_REMOVE(&head, it, thread_info_s, entries);
        free(it);
    }

    pthread_mutex_destroy(&file_mutex);
    if (global_sockfd != -1) close(global_sockfd);
    remove(DATA_FILE);
    closelog();

    return 0;
}
