/* generic code for supporting a light server */

#include "protocol.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct light_server {
  char name[32];
  int extra_data;
  void(*on_handler)(int lightid, int clientid);
  void(*off_handler)(int lightid, int clientid);
  void(*brightness_handler)(int lightid, int clientid, float brightness);
  void(*rgb_handler)(int lightid, int clientid, float r, float g, float b);
  void(*hsi_handler)(int lightid, int clientid, float h, float s, float i);
};

static struct light_server light_servers[256];
static int unused_light_server_id = 0;

void default_on_handler(int lightid, int clientid) {
  /* does nothing */
  printf("default\n");
}
void default_off_handler(int lightid, int clientid) {
  /* does nothing */
  printf("default\n");
}
void default_brightness_handler(int lightid, int clientid, float brightness) {
  /* by default, turns on/off based on brightness > 0.5 */
  if(brightness > 0.5) {
    light_servers[lightid].on_handler(lightid, clientid);
  } else {
    light_servers[lightid].off_handler(lightid, clientid);
  }
}
void default_rgb_handler(int lightid, int clientid, float r, float g, float b) {
  /* by default, uses red channel for brightness */
  light_servers[lightid].brightness_handler(lightid, clientid, r);
}
void default_hsi_handler(int lightid, int clientid, float h, float s, float i) {
  /* by default, just runs rgb handler */
  //float angle = 2 * M_PI * h;
  printf("should implement default hsi handler (in lights.c) to convert to rgb\n");
  exit(1);
}

static int light_msqid; /* the msg queue for the lights in this process */

static volatile sig_atomic_t lights_keep_running;

void lights_sigint_handler(int sig) {
  lights_keep_running = 0;
}

/* initializes the message queue unique to this process, and changes
   the sigint handler for the process. */
int squidlights_light_initialize(void) {
  if((light_msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT)) == -1) {
    perror("lights.c, initialization msgget");
    exit(1);
  }

  struct sigaction sa;
  sa.sa_handler = lights_sigint_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  if(sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction (couldn't register ^C handler)");
    exit(1);
  }
  
  return 0;
}

static int server_msqid; /* the msg queue to squidlights */

int squidlights_light_connect(char* name) {
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

  if((server_msqid = msgget(SQ_SERVER_MSG_ID, 0666)) == -1) {
    perror("server not running? msgget");
    return SQ_CONNECTION_ERROR;
  }

  /* attach light to server */
  struct light_init_msg msg;
  msg.mtype = SQ_LIGHT_SET_NAME;
  msg.lightid = lightid;
  msg.msqid = light_msqid;
  strcpy(msg.name, name);
  if(msgsnd(server_msqid, &msg, SIZEOF_MSG(struct light_init_msg), 0) == -1) {
    perror("lights.c, squidlights msgsnd");
    return SQ_CONNECTION_ERROR;
  }

  // and we're done!
  return lightid;
}

int squidlights_light_attach_data(int lightid, int extradata) {
  light_servers[lightid].extra_data = extradata;
  return 0;
}

int squidlights_light_attached_data(int lightid) {
  return light_servers[lightid].extra_data;
}

/* make sure to not modify result */
char* squidlights_light_getname(int lightid) {
  return light_servers[lightid].name;
}

int squidlights_light_add_on(int lightid, void(*new_on_handler)(int lightid, int clientid)) {
  light_servers[lightid].on_handler = new_on_handler;
  return 0;
}
int squidlights_light_add_off(int lightid, void(*new_off_handler)(int lightid, int clientid)) {
  light_servers[lightid].off_handler = new_off_handler;
  return 0;
}
int squidlights_light_add_brightness(int lightid, void(*new_brightness_handler)(int lightid, int clientid, float brightness)) {
  light_servers[lightid].brightness_handler = new_brightness_handler;
  return 0;
}
int squidlights_light_add_rgb(int lightid, void(*new_rgb_handler)(int lightid, int clientid, float r, float g, float b)) {
  light_servers[lightid].rgb_handler = new_rgb_handler;
  return 0;
}
int squidlights_light_add_hsi(int lightid, void(*new_hsi_handler)(int lightid, int clientid, float h, float s, float i)) {
  light_servers[lightid].hsi_handler = new_hsi_handler;
  return 0;
}

static float clamp(float x) {
  if(x < 0) return 0;
  if(x > 1) return 1;
  return x;
}

static int squidlights_handle_msg_buf(struct generic_msgbuf * buf) {
  struct light_brightness_msg * lbm_buf;
  struct light_rgb_msg * lrm_buf;
  struct light_hsi_msg * lhm_buf;
  switch(buf->mtype) {
  case SQ_LIGHT_ON :
    if(buf->lightid < 0 || buf->lightid >= unused_light_server_id) {
      printf("no such light %d\n", buf->lightid);
    } else {
      light_servers[buf->lightid].on_handler(buf->lightid, buf->clientid);
    }
    break;
  case SQ_LIGHT_OFF :
    if(buf->lightid < 0 || buf->lightid >= unused_light_server_id) {
      printf("no such light %d\n", buf->lightid);
    } else {
      light_servers[buf->lightid].off_handler(buf->lightid, buf->clientid);
    }
    break;
  case SQ_LIGHT_BRIGHTNESS :
    if(buf->lightid < 0 || buf->lightid >= unused_light_server_id) {
      printf("no such light %d\n", buf->lightid);
    } else {
      lbm_buf = (struct light_brightness_msg *) buf;
      light_servers[lbm_buf->lightid].brightness_handler(lbm_buf->lightid, lbm_buf->clientid,
							 clamp(lbm_buf->brightness));
    }
    break;
  case SQ_LIGHT_RGB :
    if(buf->lightid < 0 || buf->lightid >= unused_light_server_id) {
      printf("no such light %d\n", buf->lightid);
    } else {
      lrm_buf = (struct light_rgb_msg *) buf;
      light_servers[lrm_buf->lightid].rgb_handler(lrm_buf->lightid, lrm_buf->clientid,
						  clamp(lrm_buf->r), clamp(lrm_buf->g), clamp(lrm_buf->b));
    }
    break;
  case SQ_LIGHT_HSI :
    if(buf->lightid < 0 || buf->lightid >= unused_light_server_id) {
      printf("no such light %d\n", buf->lightid);
    } else {
      lhm_buf = (struct light_hsi_msg *) buf;
      light_servers[lhm_buf->lightid].hsi_handler(lhm_buf->lightid, lhm_buf->clientid,
						  lhm_buf->h, clamp(lhm_buf->s), clamp(lhm_buf->i));
    }
    break;
  case SQ_DIE :
    printf("Server-forced death.\nbllaaarrrrggghhh!!!\n");
    lights_keep_running = 0;
    break;
  default :
    printf("ignoring unknown message type %ld\n", buf->mtype);
  }
  return 1;
}

void squidlights_lights_cleanup(void) {
  /* cleanup! cleanup! everybody do your share! */
  printf("killing message queue\n");
  
  /* server will detect shutdown of queue */
  if(msgctl(light_msqid, IPC_RMID, NULL) == -1) {
    perror("msgctl");
  }
}

void squidlights_light_run(void) {
  struct generic_msgbuf buf;

  lights_keep_running = 1;
  
  printf("running...\n");

  while(lights_keep_running) {
    if(msgrcv(light_msqid, &buf, SIZEOF_MSG(struct generic_msgbuf), 0, 0) == -1) {
      perror("lights.c, run msgrcv");
      printf("server disconnected?");
      lights_keep_running = 0;
    } else {
      squidlights_handle_msg_buf(&buf);
    }
  }

  squidlights_lights_cleanup();
}

int squidlights_lights_handle_init(void) {
  lights_keep_running = 1;
  return 0;
}

int squidlights_lights_handle(char wait) {
  struct generic_msgbuf buf;
  struct msqid_ds msq;
  int ret = msgctl(light_msqid, IPC_STAT, &msq);
  while(ret != -1 && msq.msg_qnum > 0) {
    if(!msgrcv(light_msqid, &buf, SIZEOF_MSG(struct generic_msgbuf), 0, wait?0:IPC_NOWAIT)) {
      perror("lights.c handle msgrcv");
      return SQ_CONNECTION_ERROR;
    }
    squidlights_handle_msg_buf(&buf);
    ret = msgctl(light_msqid, IPC_STAT, &msq);
  }
  if(ret == -1) {
    perror("lights.c handle msgctl");
    printf("lights deciding to shut down message queue (ret==-1)\n");
    squidlights_lights_cleanup();
    return -1;
  }
  if(!lights_keep_running) {
    printf("lights deciding to shut down message queue (because interrupt)\n");
    squidlights_lights_cleanup();
    return -1;
  }
  return 0;
}
