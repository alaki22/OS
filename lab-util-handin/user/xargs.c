#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define MAXBUF 512

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: xargs command [args...]\n");
    exit(1);
  }

  char buf[MAXBUF];
  char *cmd[MAXARG];
  int n, i;


  for (i = 0; i < argc - 1 && i < MAXARG - 1; i++) {
    cmd[i] = argv[i + 1];
  }
  cmd[i] = 0;

  while ((n = read(0, buf, sizeof(buf))) > 0) {
    int pos = 0, start = 0;

    while (pos < n) {
      if (buf[pos] == '\n') {
        buf[pos] = 0; 

        cmd[i] = buf + start;
        cmd[i + 1] = 0;

        if (fork() == 0) {
          exec(cmd[0], cmd);
          fprintf(2, "exec %s failed\n", cmd[0]);
          exit(1);
        } else {
          wait(0);
        }

        start = pos + 1;
      }
      pos++;
    }
  }

  if (n < 0) {
    fprintf(2, "xargs: read error\n");
    exit(1);
  }

  exit(0);
}
