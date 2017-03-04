/*
 * Run an echo server per https://tools.ietf.org/html/rfc862
 *
 * I took inspiration and code snippets from
 * https://github.com/mafintosh/echo-servers.c/blob/master/tcp-echo-server.c
 *  and
 * http://www.paulgriffiths.net/program/c/srcs/echoservsrc.html
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

/* Constants */
#define PORT 7
//#define PORT 22 /* Purposely cause an error. */
#define BUFFER_SIZE 32


int main() {
    int server_fd, client_fd; /* Our server's fd and an fd for client connections */
    struct sockaddr_in server_address, client_address;

    /* Create a socket for our server */
    int protocol = 0; /* Use default protocol for the socket type. */
    server_fd = socket(AF_INET, SOCK_STREAM, protocol);
    if (server_fd < 0) {
        /* TODO: Print errno; try perror() on. */
        fprintf(stderr, "Unable to create socket! ERROR: %s (errno=%d)\n", strerror(errno), errno);
        exit(1);
    }

    /* TODO: Set a diff address family, to see how this breaks. */
    server_address.sin_family       = AF_INET;    /* Address family */
    /* Convert PORT and INADDR_ANY to network byte-ordered values.
     * See https://www.gnu.org/software/libc/manual/html_node/Byte-Order.html
     */
    server_address.sin_port         = htons(PORT);
    server_address.sin_addr.s_addr  = htonl(INADDR_ANY); /* Is this necessary given it's all zeros? */

    /* TODO: Look at setsockopt() to understand if we want to set an values (i.e. SO_REUSEPORT). */

    /* Bind our socket to an IP/port pair. */
    int bind_error;
    /* server_address is cast as a sockaddr pointer in the call to bind().
     * Good explanation of how sockaddr_in is a type of sockaddr here:
     * http://stackoverflow.com/questions/9312113/why-can-we-cast-sockaddr-to-sockaddr-in
     */
    bind_error = bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address));
    if (bind_error < 0) {
        fprintf(stderr, "Unable to bind socket! ERROR: %s (errno=%d)\n", strerror(errno), errno);
        exit(1);
    }

    int listen_error;
    int listen_backlog = 10; /* Should be enough for anyone! */
    listen_error = listen(server_fd, listen_backlog);
    if (listen_error < 0) {
        fprintf(stderr, "Unable to listen on socket! ERROR: %s (errno=%d)\n", strerror(errno), errno);
        exit(1);
    }

    /* Use inet_ntop() to print INADDR_ANY as a string.
     * The function stores the string value of the binary address it's provided.
     * INET_ADDRSTRLEN is a macro defining the max length of an IPv4 address. */
    char server_address_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_address.sin_addr), server_address_str, INET_ADDRSTRLEN);
    printf("Started server is listening on %s:%d\n", server_address_str, PORT);

    while(1) {
        /* Cast client_address as a sockaddr, just like server_address in bind().
         * accept() requires the last arg (address_length) to be a *pointer*.
         * The last 2 args for accept() can be NULL. Play with that. */
        socklen_t client_address_length = sizeof(client_address);
        client_fd = accept(server_fd, (struct sockaddr *) &client_address, &client_address_length);
        if (client_fd < 0) {
            fprintf(stderr, "Unable to accept client connection! ERROR: %s (errno=%d)\n", strerror(errno), errno);
        }

        /* It's nice to know who's talking to us. */
        char client_address_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), client_address_str, INET_ADDRSTRLEN);
        int client_port = ntohs(client_address.sin_port);
        char client_tuple[sizeof(client_address_str) + sizeof(client_port) + 1];
        sprintf(client_tuple, "%s:%d", client_address_str, client_port);
        printf("Accepted connection from client at %s\n", client_tuple);

        /* Without this loop we can only handle a single message per connection. */
        while(1) {
            char recv_buffer[BUFFER_SIZE]; /* A place to hold our message. */
            int recv_flags = 0;
            int read_length = recv(client_fd, recv_buffer, BUFFER_SIZE, recv_flags);
            /* Client likely disconnected.
             * Using strace, I noticed that without this test/break, closing a
             * telnet session resulted in the server constantly looping on a
             * zero-length result from recv(). */
            if (!read_length) {
                break;
            }
            if (read_length < 0) {
                fprintf(stderr, "Failed to read input from client! ERROR: %s (errno=%d)\n", strerror(errno), errno);
            }
            /* TODO: Set up logging and write this output somewhere other than the console. */
            printf("%s:%d said: %s", client_address_str, client_port, recv_buffer);

            int send_flags = 0;
            /* 'send_buffer' will be used to build a string like the following:
             * You (1.2.3.4:5678) said: <message>
             * |-5-|            |---8--|
             */
            char send_buffer[5 + sizeof(client_tuple) + 8 + sizeof(recv_buffer)] = {0};
            sprintf(send_buffer, "You (%s) said: %s", client_tuple, recv_buffer);
            int send_length = send(client_fd, send_buffer, sizeof(send_buffer), send_flags);
            if (send_length < 0) {
                fprintf(stderr, "Failed to echo input back to client! ERROR: %s (errno=%d)\n", strerror(errno), errno);
            }
            /* FIXME: I dunno if there is a buffer overflow issues, or what, but given
             * enough test messages (long ones for certain) the echo behavior gets
             * wonky, showing things like the following:
             * You (14:51445) said: ) said
             *   and
             * You (: ) said: ) said:
             */
            //memset(recv_buffer, '\0', sizeof(recv_buffer));
            //memset(send_buffer, '\0', sizeof(send_buffer));
        }
    }
}
