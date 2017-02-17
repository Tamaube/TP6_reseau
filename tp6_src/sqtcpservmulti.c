#include <errno.h>
#include <err.h>
#include <sysexits.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <sys/select.h>
#include <sys/types.h>

#include "sqserv.h"

char *prog_name;

void usage() {
    printf("usage: %s [-p <port>]\n", prog_name);
    printf("\t-p <port> specify alternate port\n");
}

int main(int argc, char* argv[]) {

    long port = DEFAULT_PORT;
    struct sockaddr_in sin;
    int fd;
    size_t  data_len;
    ssize_t len;
    char *data, *end;
    long ch;
    long max_fd = FD_SETSIZE;

    /* get options and arguments */
    prog_name = strdup(basename(argv[0]));
    while ((ch = getopt(argc, argv, "?hp:")) != -1) {
        switch (ch) {
            case 'p': port = strtol(optarg, (char **)NULL, 10);break;
            case 'h':
            case '?':
            default: usage(); return 0;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage();
        return EX_USAGE;
    }

    
    /* get room for data */
    data_len = BUFFER_SIZE;
    if ((data = (char *) malloc(data_len)) < 0) {
        err(EX_SOFTWARE, "in malloc");
    }

    /* create and bind a socket */
    fd = socket(AF_INET, SOCK_STREAM, 6);
    if (fd < 0) {
        free(data);
        err(EX_SOFTWARE, "in socket");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY; // rather memcpy to sin.sin_addr
    sin.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        free(data);
        close(fd);
        err(EX_SOFTWARE, "in bind");
    }

    /* wait for connection */
    int res = listen(fd, 1);
    if (res < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("listening on port %d\n", (int)port);
    
    fd_set read_fds;
    fd_set select_fds;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    memcpy(&select_fds, &read_fds, sizeof(read_fds));
    max_fd = fd;

    printf("Entering select\n");
    struct sockaddr_in *client = malloc(sizeof(struct sockaddr_in));
    socklen_t *addrlen = malloc(sizeof(client));
    int current_fd;
    int i, j;
    int modified_fd;
    while(1) {
        printf("Monitoring fds\n");
        memcpy(&select_fds, &read_fds, sizeof(select_fds));
        modified_fd = select(max_fd + 1, &select_fds, NULL, NULL, NULL);
        if (modified_fd == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        printf("Passed select\n");
        printf("Nb modify: %d\n", modified_fd); 
        for (i = 0, j = 0; i <= max_fd || j <= modified_fd; ++i, ++j) {
            if(FD_ISSET(i, &select_fds)) {
                if(i == fd) {
                    /* accept connection */
                    current_fd = accept(fd, (struct sockaddr *) client, addrlen);
                    if (current_fd < 0) {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }    
                    FD_SET(current_fd, &read_fds);
                    if (current_fd > max_fd) {
                        max_fd = current_fd;
                    }
                    printf("New connection %s, file desc num: %d\n", inet_ntoa(client->sin_addr), current_fd);
                } else {
                    printf("reading data\n");
                    /* receive data */
                    len = recv(i, data, data_len, 0);
                    if (len < 0) {
                        FD_CLR(i, &read_fds);
                        close(i);
                        err(EX_SOFTWARE, "in recv");
                    }

                    ch = strtol(data, &end, 10);

                    switch (errno) {
                        case EINVAL: err(EX_DATAERR, "not an integer");
                        case ERANGE: err(EX_DATAERR, "out of range");
                        default: if (ch == 0 && data == end) errx(EX_DATAERR, "no value");  // Linux returns 0 if no numerical value was given
                    }
                    /* send data */
                    printf("integer value: %ld, its square: %ld\n", ch, ch*ch);
                    char towrite[data_len];
                    sprintf(towrite, "%ld", ch*ch);
                    /*len = sendto(fd, towrite, data_len, 0, (struct sockaddr *)&sin, sin_len);
                    if (len < 0) {
                        perror("sendto");
                    }*/
                }
            }
        }
        FD_ZERO(&select_fds);



        
    }
    /* cleanup */
    free(data);
    close(fd);
    return EX_OK;
}
