#include "systemcalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
        
	//Fork a child process to run the command
	int result = system(cmd);
	
	if(result == -1)
	{
		perror("do_system: system() call failed");
		return false;
	}
		
	if(WIFEXITED(result))
	{
		int exit_status = WEXITSTATUS(result);
		if(exit_status == 0)
		{
			return true;
		}
		else 
		{
			fprintf(stderr, "Command Failed with Exit Code : %X\n", exit_status);
		}
	} else if (WIFSIGNALED(result))
	{
		fprintf(stderr, "Command terminated by signal : %X\n", WTERMSIG(result));
	}
			
    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    va_end(args);
    fflush(stdout);
    
    pid_t pid = fork();
    
    if(pid == -1)
    {
    	return false;
    }
    
    //Check if the process is a child
    if(pid == 0)
    {
    	//Path validation
        if (command[0][0] != '/') {
            fprintf(stderr, "Error: Command must be an absolute path\n");
            exit(EXIT_FAILURE);  // Child process terminates on failure
        }

        // Execute the command
        if (execv(command[0], command) == -1) {
            perror("execv failed");
            exit(EXIT_FAILURE);  // Child process terminates on failure
        }
    }else{ //Process is a Parent
    	int child_status;
    	
    	if(waitpid(pid, &child_status,0)==-1)
    	{
    		return false;
    	}
    	return WIFEXITED(child_status)&&WEXITSTATUS(child_status) ==0;
    }
    	
    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    va_end(args);
    fflush(stdout); // To avoid duplicate prints
    pid_t pid = fork();
    if(pid == -1)
    {
    	return false;
    }
    if(pid == 0)
    {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd == -1)
        {
        	exit(EXIT_FAILURE);
        }
        
        if(dup2(fd, STDOUT_FILENO) == -1)
        {
        	close(fd);
        	exit(EXIT_FAILURE);
        }
        close(fd);
    	if(execv(command[0], command)==-1)
    	{
    		return false;
    	}
    }else{
    	int status;
    	if(waitpid(pid, &status,0)==-1)
    	{
    		return false;
    	}
    	return WIFEXITED(status)&&WEXITSTATUS(status) ==0;
    }

    return true;
}
