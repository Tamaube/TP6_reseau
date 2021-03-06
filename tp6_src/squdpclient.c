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

#include "sqserv.h"

#undef LEGACY

char *prog_name;

void usage() {
    printf("usage: %s [-p <port>] <hostname>\n", prog_name);
    printf("\tremote host name as argument\n");
    printf("\t-p <port> specify alternate port\n");
}

int main(int argc, char* argv[]) {

    long port = DEFAULT_PORT;
    //struct sockaddr_in sin;
#ifdef LEGACY
    struct hostent *hp;
#else
    struct addrinfo hints, *ai;
    char service[15];
    char ipstr[INET6_ADDRSTRLEN];
    int error;
#endif
    //int fd;
    //size_t  data_len, header_len;
    //ssize_t len;
    //char *data;
    char *host;
    long ch;

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

    if (argc != 1) {
        usage();
        return EX_USAGE;
    }

    host = argv[0];

    printf("sending to %s:%d\n", host, (int)port);

    /* create a socket, no bind needed, let the system choose */
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* gethostbyname for information, getaddrinfo is prefered */
#ifdef LEGACY
    if((hp = gethostbyname(host)) == NULL) {
        errx(EX_NOHOST, "(gethostbyname) cannot resolve %s: %s", host, hstrerror(h_errno));
    }
    printf("gethostbyname: resolved name '%s' to IP address %s\n", hp->h_name, inet_ntoa(*(struct in_addr *)hp->h_addr));
#else
    /* getaddrinfo is the prefered method today */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME | AI_NUMERICSERV; // AI_PASSIVE
    hints.ai_family = AF_UNSPEC; // AF_INET AF_INET6
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(service, sizeof(service), "%ld", port);
    if ((error = getaddrinfo(host, service, &hints, &ai)) != 0) {
        errx(EX_NOHOST, "(getaddrinfo) cannot resolve %s: %s", host, gai_strerror(error));
    }

    struct addrinfo *tp = ai;
    for(; tp != NULL; tp = tp->ai_next) {
        switch (ai->ai_family) {
            case AF_INET:  inet_ntop(tp->ai_family, &(((struct sockaddr_in *)tp->ai_addr)->sin_addr), ipstr, sizeof(ipstr)); break;
            case AF_INET6: inet_ntop(tp->ai_family, &(((struct sockaddr_in6 *)tp->ai_addr)->sin6_addr), ipstr, sizeof(ipstr)); break;
            default: errx(EX_NOHOST, "(getaddrinfo) unknown address family: %d", tp->ai_family);
        }
        printf("getaddrinfo:   resolved name '%s' to IP address %s\n", tp->ai_canonname, ipstr);
    }
#endif

    /* send data */
    long int read;
    int res = scanf("%ld", &read);
    char tosend[res];
    sprintf(tosend, "%ld", read);
    ssize_t len = sendto(fd, tosend, res, 0, ai->ai_addr, ai->ai_addrlen);
    
    if(len < 0) {
        perror("sendto");
    }
    /* receive data */
    char data[res];
    len = recvfrom(fd, data, res*8, 0, ai->ai_addr, &ai->ai_addrlen);
    printf("Receive %s\n", data);
    /* cleanup */
    close(fd);

    return EX_OK;
}
