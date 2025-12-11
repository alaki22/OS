#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]){
  if(argc < 2){
    printf("ERROR! not enough arguments!\n");
    exit(1);
  }

  int ticks = atoi(argv[1]);
  sleep(ticks);
  exit(0);
}