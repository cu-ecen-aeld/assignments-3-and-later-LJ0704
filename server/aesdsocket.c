#include <sys/types.h>
#include <sys/socket.h> //socket operations
#include <syslog.h> //syslogs
#include <stdio.h>
#include <string.h> //str operations
#include <netinet/in.h> //struct sockaddr_in
#include <fcntl.h> //open,read,write
#include <unistd.h>
#include <signal.h> //for signal()
#include <stdlib.h>
#include <arpa/inet.h>



#define PORT 9000
#define BACKLOG 10
#define BUF_SIZE 1024

static volatile uint32_t stop = 0;

void signal_handler(int sig)
{
	syslog(LOG_INFO, "Caught signal, exiting");
	stop =1;
}

int main(int argc, char *argv[])
{

       // unlink("/var/tmp/aesdsocketdata");

        int sockfd, client_fd;
        int result;
        
        struct sockaddr_in sock_addr;
	struct sockaddr_in client_addr;
	
	socklen_t addr_size;
	
	char buf[BUF_SIZE];
	char ip[INET_ADDRSTRLEN];
	
	openlog("aesdsocket", LOG_PID, LOG_USER);
	
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	

	//Step1: Create socket
	sockfd = socket(AF_INET, SOCK_STREAM,0); // xxxx IPv4,Stream Socket, Protocol 0 
	
	if(sockfd == -1)
	{
		syslog(LOG_ERR,"socket failed");
		return -1;
	}
	
	memset(&sock_addr,0,sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(PORT);
	sock_addr.sin_addr.s_addr = INADDR_ANY;
	
	//Step 2: Binding
	result = bind(sockfd,(struct sockaddr *)&sock_addr,sizeof(sock_addr)) ;
	
	if(result == -1)
	{
		syslog(LOG_ERR,"Bind failed");
		close(sockfd);
		return -1;
	}
	
	//Step3: Listen to socket	
	if(listen(sockfd,BACKLOG) == -1)
	{
		syslog(LOG_ERR,"Listen failed");
		close(sockfd);
		return -1;
	}
	int daemon_mode = 0;

if(argc > 1 && strcmp(argv[1], "-d") == 0)
    daemon_mode = 1;

/* after listen() succeeds */

if(daemon_mode)
{
    pid_t pid = fork();
    if(pid < 0)
        return -1;

    if(pid > 0)
        exit(0);

    setsid();
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}


	while(!stop)
	{
		addr_size = sizeof(client_addr);
	
		client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
		if(client_fd <0)
		{	
			continue;
		}
	
		inet_ntop(AF_INET, &client_addr.sin_addr,ip,sizeof(ip));
	//Logging accepting connection
		syslog(LOG_INFO, "Accepted connection from %s", ip);
	
        char *packet = NULL;
        size_t total = 0;
        
        //Wait till we get newline
	while(!stop)
	{
	     ssize_t bytes = recv(client_fd, buf, BUF_SIZE, 0);
	     if(bytes <= 0)
	     {
	     	break;
	     }
	     
	     char *newbuf = realloc(packet, total + bytes);
	     if(!newbuf)
	     {
	     	free(packet);
	     	packet = NULL;
	     	break;
	     }
	     
	     packet = newbuf;
	     memcpy(packet + total, buf, bytes);
	     total = total +bytes;
	     
	     if((memchr(packet, '\n', total)) != NULL)
	     {
	     	int fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_APPEND | O_WRONLY, 0644);
	     	if(fd >= 0)
	     	{
	     	
	     		 if(fd != -1)
    {
        // Ensure newline at the end
        if(packet[total-1] != '\n') {
            char *with_nl = malloc(total + 1);
            memcpy(with_nl, packet, total);
            with_nl[total] = '\n';
            write(fd, with_nl, total+1);
            free(with_nl);
        } else {
            write(fd, packet, total);
        }
        close(fd);
    }
	     	}
	     	
	     	//Send Entire File Back 
	     	fd = open("/var/tmp/aesdsocketdata",O_RDONLY);
	     	if(fd >=0)
	     	{
	     		ssize_t r;	     		
	     		while((r = read(fd, buf, sizeof(buf))) > 0)
	     		{
	     			send(client_fd, buf, r, 0);
	     		}
	     		
	     		close(fd);	     		
	     	}
	     	
	     	free(packet);
	     	packet = NULL;
	     	total = 0;
	      }
	  }
	  
	    
	  
	  
	  free(packet);
	  close(client_fd);
	  syslog(LOG_INFO,"Closed connection from %s", ip);
	}
	
	close(sockfd);
	remove("/var/tmp/aesdsocketdata");
	closelog();
	
	return 0;
}
	
