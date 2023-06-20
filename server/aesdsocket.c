#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#define PORT "9000"
#define RECVDATA "/var/tmp/aesdsocketdata"
#define BUF_SIZE 256

int fp = -1;
int fd_total = 0;
struct pollfd *pfds = NULL;

void free_all_resource()
{
    if(pfds)
    {
        for(int i = 0; i < fd_total; i++)
	{
	    //printf("server: close fd=%d\n", pfds[i].fd);
            if(pfds[i].fd > 2)
                close(pfds[i].fd);
	}

        free(pfds);
	pfds = NULL;
    }

    if(fp>=0) close(fp);
    unlink(RECVDATA);
}

void termination_handler (int signum)
{
    switch(signum)
    {
        case SIGINT:
        case SIGTERM:
            printf("server: caught signal\n");
	    syslog(LOG_INFO, "Caught signal, exiting\n");
	    free_all_resource();
            break;
        default:
	    break;
    }
}

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_listener_socket(void)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN | POLLOUT;

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int listener;
    bool is_daemon = false;
    pid_t d_proc_id = 0;
    pid_t sid = 0;
    int newfd;  // new connection on newfd
    struct sockaddr_storage remoteaddr; // connector's address information
    socklen_t addrlen;
    struct sigaction sa;
    char remoteIP[INET6_ADDRSTRLEN];
    char buf[BUF_SIZE]; // Buffer for client data
    char r_buf[BUF_SIZE]={0};
    bool intact_data=false;

    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int fd_size = 5;
    pfds = calloc(fd_size, sizeof *pfds);

    if(argc==2 && strcmp(argv[1], "-d")==0)
    {
        printf("server is running in daemon mode...\n");
        is_daemon = true;
    }
    else
        printf("server is running...\n");

    fp = open(RECVDATA, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR);
    if(fp==-1)
    {
        perror("open");
        goto out;
    }

    sigemptyset( &sa.sa_mask );
    sa.sa_flags = 0;
    sa.sa_handler = termination_handler;
    if(sigaction( SIGINT, &sa, NULL )!=0)
    {
        perror("sigaction SIGINT");
        goto out;
    }
    if(sigaction( SIGTERM, &sa, NULL )!=0)
    {
        perror("sigaction SIGTERM");
        goto out;
    }

    // Set up and get a listening socket
    listener = get_listener_socket();

    if (listener == -1) {
        syslog(LOG_ERR, "error getting listening socket\n");
        ret = 1;
        goto out;
    }

    // Creating a daemon process if specified
    if(is_daemon)
    {
        // Create child process
        d_proc_id = fork();

        // Indication of fork() failure
        if (d_proc_id < 0)
        {
            printf("fork failed!\n");
            // Return failure in exit status
            ret = 1;
            goto out;
        }
        // PARENT PROCESS. Need to kill it.
        if (d_proc_id > 0)
        {
            printf("process_id of daemon process is %d \n", d_proc_id);
            // return success in exit status
            exit(0);
        }

        //set new session
        sid = setsid();
        if(sid < 0)
        {
            // Return failure
            ret = 1;
            goto out;
        }

        // Change the current working directory to root.
        //chdir("/");

        // Close stdin. stdout and stderr
        //close(STDIN_FILENO);
        //close(STDOUT_FILENO);
        //close(STDERR_FILENO);
    }

    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    fd_total = 1; // For the listener

    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_total, -1);

        if (poll_count == -1) {
            perror("poll");
            goto out;
        }

        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_total; i++) {

            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!

                if (pfds[i].fd == listener) {
                    // If listener is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, newfd, &fd_total, &fd_size);

                        printf("Accepted connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
			syslog(LOG_INFO, "Accepted connection from %s\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN));
                    }

                    /*printf("sending message to client...\n");
                    char *msg = "Hello, from server!";
                    if (send(newfd, msg, strlen(msg), 0) == -1)
                        perror("send");*/
                } else {

                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);

                    int sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        // Got error or connection closed by client
                        if (nbytes == 0) {
                            // Connection closed
                            printf("pollserver: socket %d hung up\n", sender_fd);
			    syslog(LOG_INFO, "Closed connection from %s\n",
                                inet_ntop(remoteaddr.ss_family,
                                    get_in_addr((struct sockaddr*)&remoteaddr),
                                    remoteIP, INET6_ADDRSTRLEN));
                        } else {
                            perror("recv");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_total);

			//goto out;

                    } else {
                        //char newline[] = {'\r', '\n'};
			int end = nbytes % BUF_SIZE;

			//printf("server: receive new data '%s' %ld end=%d\n", buf, strlen(buf),end);
			lseek(fp, 0, SEEK_END);

                        if(buf[end+1]=='\0')
                        {
                            write(fp, buf, end);
                            //write(fp, newline, sizeof(newline));
			    intact_data = true;
	                }
		        else
                            write(fp, buf, BUF_SIZE);

                        memset(buf, 0, sizeof(buf));
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
	    else if (pfds[i].revents & POLLOUT) {

                int sender_fd = pfds[i].fd;
		struct stat sb;
		int f_offset = 0;

		if(stat(RECVDATA, &sb) == -1) {
                    perror("stat");
		    goto out;
                }

		// send back to client
		if(intact_data){
		    int r_len=0;
		    
		    //printf("POLLOUT\n");
		    lseek(fp, 0, SEEK_SET);
		    while((r_len=read(fp, r_buf, BUF_SIZE)))
		    {
		        if(r_len==-1)
			{
			    perror("read");
			    goto out;
			}
                        //printf("server: sending '%s' %lu back to client...\n",r_buf, strlen(r_buf));
                        //printf("server: r_len=%d, file=%ld\n", r_len, sb.st_size);
		        sendall(sender_fd, r_buf, &r_len);
			memset(r_buf, 0, sizeof(r_buf));
			f_offset += r_len;
			lseek(fp, f_offset, SEEK_SET);
		    }

		    intact_data = false;
		}

	    }
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
out:
    free_all_resource();

    return ret;
}

