#include <string.h>

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main() {
    int fd1[2];
    pde_t p;

    if (pipe(fd1) < 0) {
        fprintf(2, "Pipe failed");
        exit(1);
    }

    p = fork();

    if (p < 0) {
        fprintf(2, "Fork failed");
        exit(1);
    } else if (p > 0) {
        char* message_parent = "ping";
        char buffer_parent[strlen(message_parent) + 1];

        write(fd1[1], message_parent, sizeof(message_parent));
        close(fd1[1]);

        wait(0);

        read(fd1[0], buffer_parent, sizeof(buffer_parent));
        printf("%d: got %s\n", getpid(), buffer_parent);
        close(fd1[0]);
        exit(0);
    } else {
        char* message_child = "pong";
        char buffer_child[count_length(message_parent) + 1];

        read(fd1[0], buffer_child, sizeof(buffer_child));
        printf("%d: got %s\n", getpid(), buffer_child);
        
        write(fd1[1], message_child, sizeof(buffer_child));
        close(fd1[1]);
    }

}

