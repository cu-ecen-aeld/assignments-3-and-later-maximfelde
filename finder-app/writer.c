#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>

int main (int argc, char *argv[])
{

	openlog(NULL,0,LOG_USER);

	if (argc < 3)
	{
		printf("Number of arguments must be 2!\n");
		syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
		return 1;
	}
	const char* writefile = argv[1];
	const char* writestr = argv[2];

	int fd;

	fd = creat(writefile, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
	if (fd == -1) 
	{
		syslog(LOG_ERR, "File could not be created");
		printf("File could not be created!\n");
		close(fd);
		return 1;
	}
	ssize_t nr;
	nr = write(fd, writestr, strlen(writestr));
	
	if (nr != strlen(writestr)){
		syslog(LOG_ERR,"File could not be written");
		printf("File could not be written!\n");
		close(fd);
		return 1;
	}
	
	
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	close(fd);
	return 0;

}	
