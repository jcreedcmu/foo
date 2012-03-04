#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int main(void) {
  int fd = open("/dev/midi1", O_RDONLY | O_NDELAY);
  printf("%d\n", fd);
  while (1) {
    int i, throttle = 100;
    struct timeval timout;
    int did = 1, maxfd = 0;
    while (did) {
      fd_set readset, writeset, exceptset;
      did = 0;
      if (throttle-- < 0)
	break;
      timout.tv_sec = 1;
      timout.tv_usec = 0;
	
      FD_ZERO(&writeset);
      FD_ZERO(&readset);
      FD_ZERO(&exceptset);
      FD_SET(fd, &readset);

      select(fd+1, &readset, &writeset, &exceptset, &timout);
      if (FD_ISSET(fd, &readset)) {
	int c;
	int ret = read(fd, &c, 1);
	if (ret <= 0)
	  fprintf(stderr, "Midi read error\n");
	else printf("%d\n", c);
	did = 1;
      }
    }
  }
}

