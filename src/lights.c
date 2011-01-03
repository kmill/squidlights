/* generic code for supporting a light server */

#include "protocol.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct light_server {
  char name[16];
  void(*on_handler)(int lightid, int clientid);
  void(*off_handler)(int lightid, int clientid);
  void(*brightness_handler)(int lightid, int clientid, float brightness);
  void(*rgb_handler)(int lightid, int clientid, float r, float g, float b);
  void(*hsi_handler)(int lightid, int clientid, float h, float s, float i);
};

void default_on_handler(int lightid, int clientid) {
  /* does nothing */
}
void default_off_handler(int lightid, int clientid) {
  /* does nothing */
}
void default_brightness_handler(int lightid, int clientid, float brightness) {
  /* does nothing */
}
void default_rgb_handler(int lightid, int clientid, float r, float g, float b) {
  /* does nothing */
}
void default_hsi_handler(int lightid, int clientid, float h, float s, float i) {
  float angle = 2 * M_PI * h;
  printf("should implement default hsi handler (in lights.c) to convert to rgb\n");
  exit(1);
}

static struct light_server light_servers[256];
static int unused_light_server_id = 0;

static int light_msqid; /* the msg queue for the lights in this process */

static volatile sig_atomic_t lights_keep_running;

void lights_sigint_handler(int sig) {
  lights_keep_running = 0;
}

/* initializes the message queue unique to this process, and changes
   the sigint handler for the process. */
int squidlights_light_initialize(void) {
  key_t key;
  if((key = ftok(SQ_LIGHT_SERVER_MSG_NAME, getpid())) == -1) {
    perror("lights.c, initialization ftok");
    exit(1);
  }
  if((light_msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
    perror("lights.c, initialization msgget");
    exit(1);
  }

  struct sigaction sa;
  sa.sa_handler = lights_sigint_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
}

static int server_msqid; /* the msg queue to squidlights */

int squidlights_light_connect(char* name) {
  key_t key;

  if(unused_light_server_id == 256) {
    printf("The dumb programmer didn't support more than 256 lights per process!\n");
    return SQ_CONNECTION_ERROR;
  }
 
  int lightid = unused_light_server_id++;

  /* set default handlers for this light server */
  strcpy(light_servers[lightid].name, name);
  light_servers[lightid].on_handler = default_on_handler;
  light_servers[lightid].off_handler = default_off_handler;
  light_servers[lightid].brightness_handler = default_brightness_handler;
  light_servers[lightid].rgb_handler = default_rgb_handler;
  light_servers[lightid].hsi_handler = default_hsi_handler;

  /* connect to server */
  /* it's ok this may get called many times */
  if((key = ftok(SQ_SERVER_MSG_NAME_LIGHTS, 22)) == -1) {
    perror("lights.c, squidlights ftok");
    return SQ_CONNECTION_ERROR;
  }

  if((server_msqid = msgget(key, 0666)) == -1) {
    perror("server not running? msgget");
    return SQ_CONNECTION_ERROR;
  }

  /* attach light to server */
  struct light_init_msg msg;
  msg.mtype = SQ_SET_NAME;
  msg.lightid = lightid;
  msg.msqid = light_msqid;
  strcpy(msg.name, name);
  if(msgsnd(server_msqid, &msg, 256, 0) == -1) {
    perror("lights.c, squidlights msgsnd");
    return SQ_CONNECTION_ERROR;
  }

  // and we're done!
}

/* make sure to not modify result */
char* squidlights_light_getname(int lightid) {
  return light_servers[lightid].name;
}

int squidlights_light_add_on(int lightid, void(*on_handler)(int lightid, int clientid)) {
  light_servers[lightid].on_handler = on_handler;
}
int squidlights_light_add_off(int lightid, void(*off_handler)(int lightid, int clientid)) {
  light_servers[lightid].off_handler = off_handler;
}
int squidlights_light_add_brightness(int lightid, void(*brightness_handler)(int lightid, int clientid, float brightness)) {
  light_servers[lightid].brightness_handler = brightness_handler;
}
int squidlights_light_add_rgb(int lightid, void(*rgb_handler)(int lightid, int clientid, float r, float g, float b)) {
  light_servers[lightid].rgb_handler = rgb_handler;
}
int squidlights_light_add_hsi(int lightid, void(*hsi_handler)(int lightid, int clientid, float h, float s, float i)) {
  light_servers[lightid].hsi_handler = hsi_handler;
}

void squidlights_light_run(void) {
  struct generic_msgbuf buf;
  struct light_brightness_msg * lbm_buf;

  lights_keep_running = 1;

  while(lights_keep_running) {
    if(msgrcv(server_msqid, &buf, 256, 0, 0) == -1) {
      perror("lights.c, run msgrcv");
      printf("server disconnected?");
      lights_keep_running = 0;
    } else {
      if(buf.lightid < 0 || buf.lightid >= unused_light_server_id) {
	printf("no such light %d\n", buf.lightid);
      } else {
	switch(buf.mtype) {
	case SQ_LIGHT_ON :
	  light_servers[buf.lightid].on_handler(buf.lightid, buf.clientid);
	  break;
	case SQ_LIGHT_OFF :
	  light_servers[buf.lightid].off_handler(buf.lightid, buf.clientid);
	  break;
	case SQ_LIGHT_BRIGHTNESS :
	  lbm_buf = (struct light_brightness_msg *) &buf;
	  light_servers[lbm_buf->lightid].brightness_handler(lbm_buf->lightid, lbm_buf->clientid, lbm_buf->brightness);
	  break;
	case SQ_LIGHT_RGB :
	  break;
	case SQ_LIGHT_HSI :
	  break;
	default :
	  printf("ignoring unknown message type %ld\n", buf.mtype);
	}
      }
    }
  }

  /* cleanup! cleanup! everybody do your share! */
  printf("killing message queue\n");
  
  /* server will detect shutdown of queue */
  if(msgctl(light_msqid, IPC_RMID, NULL) == -1) {
    perror("msgctl");
  }
}

int main(void) {
  return 0;
}
