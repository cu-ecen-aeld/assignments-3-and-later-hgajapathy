/**
 * A simple socket server to accept incoming connection
 * and echo received data back to client.
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEBUG
#ifdef DEBUG
#define SYSLOG_OPTIONS      (LOG_PERROR | LOG_NDELAY)
#else
#define SYSLOG_OPTIONS      (LOG_NDELAY)
#endif

#define MAX_ARGS            1
#define PORT                "9000"      // Port we're listening on
#define BACKLOG             10
#define MAX_LEN             100

volatile sig_atomic_t interrupted = 0;
const char *log_file = "/var/tmp/aesdsocketdata";

static void signal_handler(int signo)
{
    interrupted = 1;
    syslog(LOG_INFO, "Caught signal, exiting");
}

static int echo_data(int client)
{
    char *buf;
    int fd, ret;
    struct stat statbuf;

    fd = open(log_file, O_RDONLY, (S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH));
    if (fd < 0) {
        syslog(LOG_ERR, "Unable to open file %s", log_file);
        return -1;
    }

    fstat(fd, &statbuf);
    if ((buf = (char *)calloc(statbuf.st_size, sizeof(char))) == NULL) {
        syslog(LOG_ERR, "Failed to allocate memory for reading file: %s", strerror(errno));
        close(fd);
        return -1;
    }

    if ((ret = read(fd, buf, statbuf.st_size)) == -1) {
        syslog(LOG_ERR, "Failed to read file: %s", strerror(errno));
    } else {
        close(fd);
        if ((ret = send(client, buf, statbuf.st_size, 0)) == -1)
            syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
        free(buf);
    }

    return ret;
}

/**
 * Parse the data with newline delimiter and appaned the tokens to log_file.
 */
static int process_data(char *data)
{
    int fd;
    char *start, *end, *token;
    int ret = 0;    /* this is set to 0 to handle exit cases for stream w/o new line */

    fd = open(log_file,
              (O_WRONLY | O_CREAT | O_APPEND | O_DSYNC),
              (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    if (fd == -1) {
        syslog(LOG_ERR, "Unable to open file %s", log_file);
        return -1;
    }

    /* append packets to log_file when a new line present
     * in the stream */
    start = end = data;
    while ((end = strchr(start, '\n')) != NULL) {
        token = strsep(&start, "\n");
        if ((ret = write(fd, token, strlen(token))) == -1) {
            syslog(LOG_ERR, "Failed to write data to log file: %s", strerror(errno));
            break;
        }
        write(fd, "\n", sizeof(char));
        start = end + 1;
    }

    close(fd);

    return ret;
}

/**
 *  Creates a passive TCP socket and return socket fd to the calling
 *  function which will be used for accepting incoming connections.
 */
static int create_listener_socket(void)
{
    int sock;
    int opt_val = 1;
    struct addrinfo hints, *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;/* TCP socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
        syslog(LOG_ERR, "getaddrinfo() failed: %s!", strerror(errno));
        return -1;
    }

    /* Walk through returned list until we find an address structure
     * that can be used to successfully create and bind a socket. */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            syslog(LOG_ERR, "Failed to create an socket");
            continue;
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt_val,
                        sizeof(opt_val)) < 0) {
            syslog(LOG_ERR, "Failed to set options for socket");
        }

        if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sock);
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        syslog(LOG_ERR, "Could not bind socket to any address");
        return -1;
    }

    if (listen(sock, BACKLOG) != 0) {
        syslog(LOG_ERR, "Failed to mark socket to accept incoming connection requests");
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    struct sigaction sa;
    int opt;
    int run_as_daemon = 0;
    int socket, client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN] = {};
    char stream[MAX_LEN + 1] = {};
    ssize_t nb;
    char *buf;
    int buf_len = 0;
    int error = 0;
    pid_t pid;

    /* open a connection to system logger */
    openlog(NULL, SYSLOG_OPTIONS, LOG_USER);

    /* parse command-line args */
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd':
            run_as_daemon = 1;
            syslog(LOG_INFO, "%s server will run as a daemon\n", argv[0]);
            break;
        }
    }

    /* set-up handler for SIGINT & SIGTERM signals */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to change action for SIGINT!\n");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to change action for SIGTERM!\n");
        return -1;
    }

    if ((socket = create_listener_socket()) < 0)
        return -1;

    /* Close socket when any failure occurs during a new process
     * creation. */
    if (run_as_daemon) {
        pid = fork();
        if (pid < 0) {
            close(socket);
            syslog(LOG_ERR, "Failed to create a new process: %s", strerror(errno));
            return -1;
        }

        if (pid != 0)
            exit(EXIT_SUCCESS);

        if (setsid () == -1) {
            close(socket);
            syslog(LOG_ERR, "Failed to create a new session: %s", strerror(errno));
            return -1;
        }

        if (chdir("/")) {
            close(socket);
            syslog(LOG_ERR, "Failed to change to root dir: %s", strerror(errno));
            return -1;
        }

        /* redirect fd's 0,1,2 to /dev/null */
        open("/dev/null", O_RDWR);
        dup(0);
        dup(0);
    }

    while (!interrupted) {
        if ((client_fd = accept(socket, (struct sockaddr *) &client_addr,
                                &client_addrlen)) < 0)
            continue;

        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET6_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);

        memset(stream, 0, MAX_LEN);
        buf = malloc(sizeof(char));
        *buf = '\0';
        buf_len = 1;

        while ((nb = recv(client_fd, stream, MAX_LEN, 0)) > 0) {
            buf_len += nb;
            if ((buf = realloc(buf, (sizeof(char) * buf_len))) == NULL) {
                syslog(LOG_ERR, "Failed to reallocate memory for data stream: %s", strerror(errno));
                error = 1;
                break;
            }

            strncat(buf, stream, nb);
            if (process_data(buf) > 0) {
                if (echo_data(client_fd) < 0)
                    error = 1;
            }

            memset(stream, 0, MAX_LEN);
        }

        if (buf != NULL)
            free(buf);

        close(client_fd);

        /* break for any errors */
        if (error)
            break;
    }

    remove(log_file);
    close(socket);
    shutdown(socket, SHUT_RDWR);
    closelog();

    return ret;
}