#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <jack/jack.h>
#include <string.h>
#define MAX_JACK_PORTS 32
#define MYBUF_SIZE 2048
#define MAX_INT 0x7fffffff
jack_port_t *output_port[MAX_JACK_PORTS];
jack_default_audio_sample_t mybuf[MYBUF_SIZE];
#define randf() (rand() / (float)MAX_INT)

static int
process (jack_nframes_t nframes, void *arg) {
  int i;
  for (i = 0; i < nframes; i++)
    mybuf[i] = (2 * randf() - 1) * 0.01;
  for (i = 0; i < 2; i++) {
    jack_default_audio_sample_t *out = jack_port_get_buffer(output_port[i], nframes);
    memcpy(out, mybuf, nframes * sizeof(jack_default_audio_sample_t));
  }
}

int main(void) {
  int i;

  /* Jack stuff */
  jack_client_t *jack_client = NULL;
  jack_status_t status;
  char *client_name = "foo";

  jack_client = jack_client_open (client_name, JackNullOption, &status, NULL);
  if (status & JackServerFailed) {
    error("JACK: unable to connect to JACK server");
    jack_client=NULL;
    return;
  }

  if (status) {
    if (status & JackServerStarted) {
      printf("JACK: started server\n");
    } else {
      printf("JACK: server returned status %d\n", status);
    }
  }
  int srate = jack_get_sample_rate (jack_client);
 
  jack_set_process_callback(jack_client,  process, 0);
  output_port[0] = jack_port_register (jack_client, "out0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  output_port[1] = jack_port_register (jack_client, "out1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);



  const char **jack_ports;
  char regex_pattern[100]; // big enough
  sprintf(regex_pattern, ".*");

  jack_ports = jack_get_ports(jack_client, regex_pattern, NULL, JackPortIsInput);
  for (i = 0; jack_ports[i]; i++) {
    printf("%s\n", jack_ports[i]);
  }

  jack_ports = jack_get_ports(jack_client, regex_pattern, NULL, JackPortIsOutput);
  for (i = 0; jack_ports[i]; i++) {
    printf("%s\n", jack_ports[i]);
  }

  jack_activate (jack_client);

  jack_connect(jack_client, "foo:out0", "system:playback_1");
  jack_connect(jack_client, "foo:out1", "system:playback_2");

  printf("status  %d\n", srate);

  /* Midi stuff */
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
	unsigned char c;
	int ret = read(fd, &c, 1);
	if (ret <= 0)
	  fprintf(stderr, "Midi read error\n");
	else printf("%d\n", c);
	printf("connected? %d\n", jack_port_connected_to(output_port[0], "system:playback_1"));
	did = 1;
      }
    }
  }
}

