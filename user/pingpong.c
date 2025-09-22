#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

void child_process(int fd[], char buffer[]) {
  char* message = "pong";

  read(fd1[0], buffer, sizeof(buffer));
  printf("%d: got %s\n", getpid(), buffer);
  
  write(fd[1], message, strlen(message) + 1);
  close(fd[1]);
}

void parent_process(int fd[], char buffer[]) {
  char* message = "ping";
  char buffer[BUFFER_SIZE];

  write(fd[1], message, strlen(message) + 1);
  close(fd[1]);

  wait(0);

  read(fd[0], buffer, sizeof(buffer));
  printf("%d: got %s\n", getpid(), buffer);
}

int main() {
    char buffer[16];
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
        parent_process(fd1, buffer);
    } else {
        child_process(fd1, buffer);
    }
    exit(0);

}


