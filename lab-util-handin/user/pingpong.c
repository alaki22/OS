#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int fd[2];     
     if (pipe(fd) < 0) {  
        fprintf(2, "pipe failed\n");
        exit(1);
    }

    int pid = fork(); 
    if (pid == 0) {
        char received_byte;

        read(fd[0], &received_byte, sizeof(char));
        printf("%d: received ping\n", getpid());
        write(fd[1], &received_byte, sizeof(char));

        close(fd[0]);
    } else {
        char byte = 'A'; 
        char received_byte;

        write(fd[1], &byte, sizeof(char));
        close(fd[1]);
        wait(0);
        read(fd[0], &received_byte, sizeof(char));
        printf("%d: received pong\n", getpid());

    }

    exit(0);
}
