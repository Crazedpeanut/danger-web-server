#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include "request.h"

#define PORT "8080"
#define RECV_BUFFER_SIZE 1024
#define SEND_BUFFER_SIZE 1024
#define BACKLOG 10

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return (((struct sockaddr_in *)sa)->sin_port);

    return (((struct sockaddr_in6 *)sa)->sin6_port);
}

int bind_to_socket()
{
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    struct addrinfo *servinfo;
    int getaddrinfoResult;
    if ((getaddrinfoResult = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfoResult));
        return 1;
    }

    // loop through all the results and bind to the first we can
    int sockfd;
    struct addrinfo *current;
    int yes = 1;
    for (current = servinfo; current != NULL; current = current->ai_next)
    {
        if ((sockfd = socket(current->ai_family, current->ai_socktype,
                             current->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, current->ai_addr, current->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        char address[INET6_ADDRSTRLEN];

        inet_ntop(current->ai_family,
                  get_in_addr((struct sockaddr *)&current->ai_addr),
                  address, sizeof address);
        printf("Bound to %s:%d\n", address, ntohs(get_in_port((struct sockaddr *)current->ai_addr)));

        freeaddrinfo(servinfo);
        return sockfd;
    }

    perror("server: socket");
    exit(1);
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

char *translate_path(char *request_path)
{
    char *buffer = malloc(sizeof(char) * 1024);
    snprintf(buffer, 1024, ".%s", request_path);
    return buffer;
}

void process_request(struct sockaddr_storage clientaddr, int connectionfd)
{
    int numbytes;
    char recv_buffer[RECV_BUFFER_SIZE];
    char send_buffer[SEND_BUFFER_SIZE];

    if ((numbytes = recv(connectionfd, recv_buffer, RECV_BUFFER_SIZE - 1, 0)) == -1)
    {
        perror("recv");
        exit(1);
    }

    Request *request = read_request(recv_buffer);

    print_request(request);
    char *fspath = translate_path(request->path);

    struct stat pathstat = {};
    if (stat(fspath, &pathstat) == -1 || !S_ISREG(pathstat.st_mode))
    {
        snprintf(send_buffer, sizeof(send_buffer), "HTTP/1.1 404 NOT FOUND\nServer: HACKS\nConnection: Closed\n\n");
    }
    else
    {
        FILE *file = fopen(fspath, "r");
        char file_buffer[1024];
        size_t file_nbytes = fread(file_buffer, 1, 1024, file);
        snprintf(send_buffer, sizeof(send_buffer), "HTTP/1.1 200 OK\nServer: HACKS\nConnection: Closed\nContent-Length: %lu\n\n%s", file_nbytes, file_buffer);
    }

    printf("FS Path: %s\n", fspath);

    free(fspath);
    destroy_request(request);

    if (send(connectionfd, send_buffer, strlen(send_buffer), 0) == -1)
        perror("send");

    close(connectionfd);
}

int main(void)
{
    int sockfd = bind_to_socket();

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t connectionfd;
    socklen_t sin_size;
    char address[INET6_ADDRSTRLEN];

    while (1)
    { // main accept() loop
        sin_size = sizeof their_addr;
        connectionfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (connectionfd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  address, sizeof address);
        printf("server: got connection from %s\n", address);

        if (!fork())
        {                  // this is the child process
            close(sockfd); // child doesn't need the listener
            process_request(their_addr, connectionfd);
            exit(1);
        }
        close(connectionfd); // parent doesn't need this
    }

    return EXIT_SUCCESS;
}