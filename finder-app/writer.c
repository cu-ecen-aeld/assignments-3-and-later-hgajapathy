#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_ARGS    3

// A local errno var to save system errno on error (this
// approach is suggested in System Programming and Error Handling
// video.
int writer_errno = 0;

// Write a string and return the number of bytes written to the file.
static ssize_t writer(const char *writefile, const char *writestr)
{
    int fd = -1;
    ssize_t nbytes = -1;

    // Create the file with file permission as 0644 (if it does
    // not exists).
    fd = open(writefile, (O_WRONLY | O_CREAT | O_TRUNC),
                (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    if (fd == -1) {
        writer_errno = errno;
        syslog(LOG_ERR, "%s", strerror(writer_errno));
        return -1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    nbytes = write(fd, writestr, strlen(writestr));
    if (nbytes == -1)
        syslog(LOG_ERR, "%s", strerror(writer_errno));

    close(fd);

    return nbytes;
}

int main(int argc, char *argv[])
{
    openlog(NULL, LOG_PERROR, LOG_USER);

    if (argc != MAX_ARGS) {
        syslog(LOG_ERR, "Invalid arguments!");
        printf("Usage: %s [writefile] [writestr]\n", argv[0]);
        printf("writefile : full path to a file (including filename) on the filesystem\n");
        printf("writestr  : a text string which will be written within this file\n");

        exit(1);
    }

    // argv[1] - writefile, argv[2] - writestr
    writer(argv[1], argv[2]);

    closelog();

    return 0;
}