#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <jack/jack.h>
#include <string.h>
#include <math.h>

#define PI 3.14159265
#define MAX_JACK_PORTS 32
#define MYBUF_SIZE 2048
#define MAX_INT 0x7fffffff
jack_client_t *jack_client = NULL;
jack_port_t *output_port[MAX_JACK_PORTS];
jack_default_audio_sample_t mybuf[MYBUF_SIZE];
jack_default_audio_sample_t passbuf[MYBUF_SIZE];
#define randf() (rand() / (float)MAX_INT)
float lastPassbuf = 0;

int sample_rate = 48000;
float f = 440.0;

typedef struct {
  float f;
  float tt;
  int pitch;
  int active;
} osc_t;

#define NUM_OSCS 16
osc_t oscs[NUM_OSCS];

void init_oscs() {
  int i;
  for (i = 0; i < NUM_OSCS; i++) {
    oscs[i].active = 0;
  }
}

void new_osc(int pitch) {
  int i;

  for (i = 0; i < NUM_OSCS; i++) {
    if (!oscs[i].active) {
      oscs[i].active = 1;
      oscs[i].tt = 0.0;
      oscs[i].f = 440.0 * pow(2.0, (pitch - 69) / 12.0);
      oscs[i].pitch = pitch;
      return;
    }
  }
  // if we get here, no more polyphony left!
}

void del_osc(int pitch) {
  int i;

  for (i = 0; i < NUM_OSCS; i++) {
    if (oscs[i].pitch == pitch) {
      oscs[i].active = 0;
    }
  }
}

static int
process (jack_nframes_t nframes, void *arg) {
  int i, j;
  memset(mybuf, 0, nframes * sizeof(jack_default_audio_sample_t));
  for (i = 0; i < NUM_OSCS; i++) {
    if (!oscs[i].active) continue;
    osc_t* this = &oscs[i];
    float f = this->f;
    for (j = 0; j < nframes; j++) {
      float tt = this->tt;
      tt += 1.0 / sample_rate;
      mybuf[j] += sin(2 * PI * f * tt) ;
      this->tt = tt;
    }
  }

#define SMOOTH 0.005
  float diff = mybuf[0] - lastPassbuf;
    if (fabs(diff) > SMOOTH) {
       passbuf[0] = lastPassbuf + SMOOTH * (diff > 0 ? 1 : -1);
    }
    else passbuf[0] = mybuf[0];

  for (i = 1; i < nframes; i++) {
    float diff = 2 * mybuf[i] - passbuf[i-1];
    if (fabs(diff) > SMOOTH) {
       passbuf[i] = passbuf[i-1] + SMOOTH * (diff > 0 ? 1 : -1);
    }
    else passbuf[i] = 2 * mybuf[i];
  }

  lastPassbuf = passbuf[nframes-1];
  for (i = 0; i < 2; i++) {
    jack_default_audio_sample_t *out = jack_port_get_buffer(output_port[i], nframes);
    memcpy(out, passbuf, nframes * sizeof(jack_default_audio_sample_t));
  }
}

int main(void) {
  int i;

  /* Jack stuff */
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
  sample_rate = jack_get_sample_rate (jack_client);
 
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

  //  printf("status  %d\n", srate);

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
	unsigned char c[3];
	int ret = read(fd, &c, 3);
	if (ret <= 0)
	  fprintf(stderr, "Midi read error\n");
	else printf("%d %d\n", c[1], c[2]);
	if (c[2]) new_osc(c[1]); else del_osc(c[1]);
	//	printf("connected? %d\n", jack_port_connected_to(output_port[0], "system:playback_1"));
	did = 1;
      }
    }
  }
}

