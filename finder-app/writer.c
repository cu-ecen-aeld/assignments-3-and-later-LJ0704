#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

/* References:
	Syslog: 
	https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/lecture/8Nxp6/logging-and-syslog 

	File operations: 
 	1)fputs - https://www.geeksforgeeks.org/c/how-to-write-in-a-file-using-fputs-in-c/
	2)fprintf - https://www.geeksforgeeks.org/c/fprintf-in-c/

	ChatGPT Chat : https://chatgpt.com/share/6976c125-fc2c-800e-9f8e-65186dd756cc
*/



int main(int argc, char *argv[])
{
	
	openlog(NULL,0,LOG_USER);
	if(argc != 3)
	{
			syslog(LOG_ERR, "Failed due to invalid parameters");
			fprintf(stderr, "Error: Missing Parameter\n");
			fprintf(stderr, "Fix: First parameter should be file name and second parameter should be string\n");
		
		return 1;
	}

	const char *writefile = argv[1];
	const char *writestr = argv[2];

	FILE *fptr = fopen(writefile, "w");
	
	if(fptr == NULL)
	{
		syslog(LOG_ERR, "Failed to open file %s: %s", writefile, strerror(errno));
		fprintf(stderr, "Error: Could not open file %s\n", writefile);
		closelog();		
		return 1;
	}
	
	syslog(LOG_DEBUG,"Writing %s to %s", writestr, writefile);
	
	int err_flag = fputs(writestr,fptr);
	
	if(err_flag == EOF)
	{
		syslog(LOG_ERR, "Failed to write to file %s: %s ", writefile, strerror(errno));
		fprintf(stderr, "Error: Could not write to file %s\n",writefile);
		fclose(fptr);
		closelog();
		return 1;
	}

	fclose(fptr);

	closelog();
	return 0;

}
