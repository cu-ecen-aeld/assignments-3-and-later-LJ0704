//References: Steps for socket is follwed as per the coursera lecture 
//Gemini Reference: https://gemini.google.com/share/07982bad0784
//Linux Manual Reference : https://linux.die.net/man/3/htons

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


#define PORT 9000 //PORT Number as mentioned in assignment
#define BACKLOG 10
#define BUF_SIZE 1024 //Buffer

static volatile int stop = 0;

void signal_handler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
	syslog(LOG_INFO, "Caught signal, exiting");
	stop =1;
	}
}

int check_daemon(int daemon_mode)
{
	if(daemon_mode)
	{	
    		pid_t pid = fork();
    		if(pid < 0)
    		{
    			perror("fork");
        		return -1;
        	}

    		if(pid > 0)
    		{
        		exit(0);
        	}

    		if(setsid()<0)
    		{
    			perror("setsid");
    			return -1;
    		}
    		
    		if(chdir("/") < 0)
    		{
    			perror("chdir");
    			return -1;
    		}

    		close(STDIN_FILENO);
    		close(STDOUT_FILENO);
    		close(STDERR_FILENO);
	}
	return 0;
}

int main(int argc, char *argv[])
{
        int sockfd;
        int client_fd;
        int result;
        int daemon_mode = 0;
        
        struct sockaddr_in sock_addr;
	struct sockaddr_in client_addr;
	
	socklen_t addr_size;
	
	char buf[BUF_SIZE];
	char ip[INET_ADDRSTRLEN];
	
	openlog("aesdsocket", LOG_PID, LOG_USER);
	
	// Register signals - Fixed using GPT
    	struct sigaction sa; 
    	sa.sa_handler = signal_handler;
    	sigemptyset(&sa.sa_mask);
    	sa.sa_flags = 0;
    	sigaction(SIGINT, &sa, NULL);
    	sigaction(SIGTERM, &sa, NULL);	
	
	if(argc > 1 && strcmp(argv[1], "-d") == 0)
	{
    		daemon_mode = 1;
    	}

	//Step1: Create socket
	sockfd = socket(AF_INET, SOCK_STREAM,0); // xxxx IPv4,Stream Socket, Protocol 0 
	
	if(sockfd == -1)
	{
		syslog(LOG_ERR,"socket failed");
		return -1;
	}
	
	memset(&sock_addr,0,sizeof(sock_addr));
	sock_addr.sin_family = AF_INET; //xxxx IPv4
	sock_addr.sin_port = htons(PORT);//host byte order to network byte order
	sock_addr.sin_addr.s_addr = INADDR_ANY;
	
	//Step 2: Binding
	result = bind(sockfd,(struct sockaddr *)&sock_addr,sizeof(sock_addr)) ;
	
	if(result == -1)
	{
		syslog(LOG_ERR,"Bind failed");
		close(sockfd);
		return -1;
	}

        result = check_daemon(daemon_mode);
        if(result == -1)
        {
        	return -1;
        }		
	//Step3: Listen to socket	
	if(listen(sockfd,BACKLOG) == -1)
	{
		syslog(LOG_ERR,"Listen failed");
		close(sockfd);
		return -1;
	}


	while(!stop)
	{
		addr_size = sizeof(client_addr);
	//Step 4: Accept
		client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
		if(client_fd < 0)
		{	
			continue;
		}
	
		inet_ntop(AF_INET, &client_addr.sin_addr,ip,sizeof(ip));
		//Logging accepting connection
		syslog(LOG_INFO, "Accepted connection from %s", ip);
	
        	char *packet = NULL;
        	size_t total = 0;
                ssize_t bytes_written =0;
        	//Wait till we get newline - Rewrote with GPT
		while(!stop)
		{
	     		//Receive Data
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
        					if(packet[total-1] != '\n')
        					{
            						char *with_nl = malloc(total + 1);
            						memcpy(with_nl, packet, total);
            						with_nl[total] = '\n';
            						bytes_written = write(fd, with_nl, total+1);
            						if(bytes_written == -1)
            						{
            							perror("write");
            						}
            						free(with_nl);
        						} else {
            						bytes_written = write(fd, packet, total);
            						if(bytes_written == -1)
            						{
            							perror("write");
            						}
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
	
