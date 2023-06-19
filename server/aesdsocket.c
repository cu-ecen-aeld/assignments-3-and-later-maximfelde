#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BACKLOG 10
#define MAXDATASIZE 50000
#define MAXFILESIZE 50000

//#define DEBUG

FILE *output_file;
//char output_file_location[] = "aesdsocketdata.txt";
char output_file_location[] = "/tmp/var/aesdsocketdata";
char pathname[]="/tmp/var/";
int s, new_fd;
struct addrinfo *servinfo;  // will point to the results

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void signal_handler (int signo)
{
    if ((signo == SIGINT) || (signo == SIGTERM))
    {
        syslog(LOG_INFO | LOG_DEBUG, "Caught signal, exiting");
        printf("\nCaught signal, exiting\n");

        remove(output_file_location);
        close(new_fd);
        close(s);
        exit (EXIT_SUCCESS);
    }

}

//int main (int argc, char *argv[])
int main (int argc, char *argv[])
{

    openlog(NULL,0,LOG_USER);
    mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);

    int status, numbytes;
    struct addrinfo hints;
   
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    
    char ipstr[INET6_ADDRSTRLEN];   
    int yes=1;


    memset(&hints, 0, sizeof ( hints )); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((status = getaddrinfo("localhost", "9000", &hints, &servinfo)) != 0) {
        syslog(LOG_ERR | LOG_DEBUG, "getaddrinof failed:%s",gai_strerror(status));
        return 1;                
    }

#ifdef DEBUG
    struct addrinfo *p;
    char ipstr[INET6_ADDRSTRLEN];
    for(p = servinfo; p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof (ipstr));
        printf("  %s: %s\n", ipver, ipstr);
    }
#endif

    if ( (s = socket (servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        syslog(LOG_ERR | LOG_DEBUG, "get socket error: %s\n", strerror(errno));
        return 1;
    }
    

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
        syslog(LOG_ERR | LOG_DEBUG, "setsockopt() error: %s\n", strerror(errno));
        return 1;
    }

    if (-1 == bind(s, servinfo->ai_addr, servinfo->ai_addrlen))
    {
       syslog(LOG_ERR | LOG_DEBUG, "bind socket error: %s\n", strerror(errno));
       return 1;
    }

    int parent = 0;
    int daemon_is_invoced = 0;
    if ((argc > 1 )){
    if (!strcmp(argv[1] , "-d"))
    {        
        printf("Forking");
        daemon_is_invoced = 1;
        parent = fork();        
    }
    }

    if (daemon_is_invoced && parent)
    {
        printf("Starting daemon and exit.");
        freeaddrinfo(servinfo);
        close(s);
        exit(EXIT_SUCCESS);
    }



    if (-1 == listen(s, BACKLOG)){
        syslog(LOG_ERR | LOG_DEBUG, "listen socket error: %s\n", strerror(errno));
        return 1;
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    freeaddrinfo(servinfo);

    while(1) {  // main accept() loop
        addr_size = sizeof (their_addr);
        new_fd = accept(s, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {            
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof (ipstr));
        //printf("server: got connection from %s\n", ipstr);
        syslog(LOG_DEBUG | LOG_INFO, "Accepted connection from: %s\n", ipstr);
        printf ("Accepted connection from %s\n", ipstr);
 
        char buf[MAXDATASIZE];
        char file_buf[MAXFILESIZE];
        buf[0] = '\0';
        file_buf[0] = '\0';
       
        while ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) != 0)
        {   
            if (numbytes > 0) { 
                buf[numbytes] = '\0';
                printf ("Recieved: %s", buf);
                output_file = fopen(output_file_location, "a");
                fprintf(output_file, "%s", buf);
                buf[0] = '\0';
                fclose(output_file);
                output_file = fopen(output_file_location, "r");
                size_t size = fread(file_buf, 1, MAXFILESIZE, output_file);
                printf("File contance:%s. Lenght:%lu", file_buf, size);
                
                fclose(output_file);                    
                send(new_fd,file_buf, size, 0);
                }            
        }
        if (numbytes == 0)
        {
            printf("Connection closed from: %s\n", ipstr);

        }
        
        close(new_fd);  // parent doesn't need this
    }
        
    close(s);
}