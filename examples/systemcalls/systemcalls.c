#include "systemcalls.h"
#include <sys/syscall.h>
#include "stdlib.h"
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <sys/stat.h>
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
	int ret = system(cmd);
	if (ret == -1)
	{
		perror("system() call failed");
		return false;
	}
	if (ret > 0)
	{
		perror("Command execution returned with status not zero");
		return false;
	}

    return true;
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
    fflush(stdout);
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    
	command[count] = NULL;
	va_end(args);   
	
	int status;
	pid_t pid;
	pid = fork();
	int ret;
	if (pid == -1)
	{
		perror("Forking has failed");
		va_end(args);
		return false;
	}
	else if (pid == 0)
	{	
		ret = execv(command[0], command);
		if (ret == -1)
		{		
			perror("Child: exec() failed");
		}
		exit(1);
	}
	
	
	if (wait( &status) == -1)
	{
		perror("wait returns error");
		return false;		
	}


	if (  WEXITSTATUS ( status))
	{
		perror(" Command exit status is not zero");
		return false;
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

    fflush(stdout);
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

	fflush(stdout);
	printf("This is redirect function\n");
   
    va_end(args);
    
 
    int fd = creat( outputfile, 0644);
    if (fd == -1)
    {
        perror("File could not be opened");
        return false;
    }
    int status;
	pid_t pid;
	int ret;

	
	switch (pid = fork())
	{
	    case -1:	
	    	perror("Forking has failed");
	    	close(fd);
            return false;
		case 0:
	        if (dup2(fd, 1) < 0) 
	        { 
    	        perror("dup2 failed");
                close(fd);                
                return false;
            }
    		ret = execv(command[0], command);
    		if (ret == -1)
	       	{		
			    perror("Child: exec() failed");
		    }
	    default:
	    close(fd);		
    	if (wait( &status) == -1)
	    {
	    	perror("wait returns error");
	    	return false;		
	    }
    	if (  WEXITSTATUS ( status))
       	{
		    perror(" Command exit status is not zero");
	    	return false;
	    }
    }	

    return true;
}
