#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int* fd) __attribute__((noreturn));  

void sieve(int* fd) {
    int prime;

  
    if (read(fd[0], &prime, sizeof(int)) == 0) {
        close(fd[0]);
        exit(0); 
    }

    printf("prime %d\n", prime);

    int fd2[2];  
    if (pipe(fd2) < 0) {
        fprintf(2, "pipe failed\n");
        close(fd[0]);
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(fd2[1]); 
        close(fd[0]);
        sieve(fd2); 
    } else {
        close(fd2[0]); 
        int num;
        while (read(fd[0], &num, sizeof(int)) > 0) {
            if (num % prime != 0) {
                if (write(fd2[1], &num, sizeof(int)) < 0) {
                    fprintf(2, "write failed\n");
                    exit(1);
                }
            }
        }

        close(fd[0]);   
        close(fd2[1]);  
        wait(0);        
        exit(0);        
    }
}

int main(int argc, char *argv[]) {
    int fd[2];  

    if (pipe(fd) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
    }

    int pid = fork();

    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(fd[1]);  
        sieve(fd);    
    } else {
        close(fd[0]);  
        for (int i = 2; i <= 280; i++) {
            if (write(fd[1], &i, sizeof(int)) < 0) {
                fprintf(2, "write failed\n");
                exit(1);
            }
        }
        close(fd[1]);  
        wait(0);       
        exit(0);      
    }

    return 0;
}
