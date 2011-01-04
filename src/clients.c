/* generic code for supporting a client */

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

/* at this level, we only need a name */
struct lights_s {
  char name[32];
  int islight;
};

static struct lights_s light_servers[NUM_LIGHT_SERVERS];

int client_msqid;
int server_msqid;

/* initializes message queue for this process */
int squidlights_client_initialize(void) {
  if((client_msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT)) == -1) {
    perror("clients.c, initialization msgget");
    exit(1);
  }
}

static int client_add_light(struct light_init_msg * lim) {
  if(lim->msqid) {
    printf("got light %d %s\n", lim->lightid, lim->name);
  } else {
    printf("lost light %d\n", lim->lightid);
  }
  strcpy(light_servers[lim->lightid].name, lim->name);
  light_servers[lim->lightid].islight = lim->msqid;
}

// name actually isn't used at the moment...
int squidlights_client_connect(char* name) {
  struct client_init_msg msg;
  struct light_init_msg lim;
  char buff[256]; /* for padding, in case of errors with recv */

  if((server_msqid = msgget(SQ_SERVER_MSG_ID, 0666)) == -1) {
    perror("server not running? msgget");
    return SQ_CONNECTION_ERROR;
  }

  msg.mtype = SQ_CLIENT_SET_NAME;
  msg.clientid = 0; // not used...
  msg.msqid = client_msqid;
  strcpy(msg.name, name);

  if(msgsnd(server_msqid, &msg, SIZEOF_MSG(struct client_init_msg), 0) == -1) {
    perror("clients.c, squidclient msgsnd");
    return SQ_CONNECTION_ERROR;
  }

  printf("waiting for server to send lights... "); fflush(stdout);
  if(msgrcv(client_msqid, &lim, SIZEOF_MSG(struct light_init_msg), 0, 0) == -1) {
    perror("clients.c, connect1 msgrcv");
    exit(1);
  }
  printf(".");
  while(lim.lightid != -1) {
    client_add_light(&lim);
    if(msgrcv(client_msqid, &lim, SIZEOF_MSG(struct light_init_msg), 0, 0) == -1) {
      perror("clients.c, connect2 msgrcv");
      exit(1);
    }
    printf(".");
  }
  printf(" done\n");
}

char* squidlights_client_lightname(int lightid) {
  if(!light_servers[lightid].islight) {
    light_servers[lightid].name[0] = '\0';
  }
  return light_servers[lightid].name;
}

int squidlights_client_getlight(char* name) {
  for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
    if(light_servers[i].islight && strcmp(name, light_servers[i].name) == 0) {
      return i;
    }
  }
  return -1;
}

int squidlights_client_process_messages(void) {
  struct generic_msgbuf buf;
  struct msqid_ds msq;
  int ret = msgctl(client_msqid, IPC_STAT, &msq);
  while(ret != -1 && msq.msg_qnum > 0) {
    if(!msgrcv(client_msqid, &buf, SIZEOF_MSG(struct generic_msgbuf), 0, IPC_NOWAIT)) {
      perror("clients.c process msgrcv"); exit(1);
    }
    switch(buf.mtype) {
    case SQ_LIGHT_SET_NAME :
      client_add_light((struct light_init_msg *) &buf);
    case SQ_DIE :
      return -1;
    default :
      printf("ignoring unknown message type %ld\n", buf.mtype);
    }
    ret = msgctl(client_msqid, IPC_STAT, &msq);
  }
  if(ret == -1) {
    perror("clients.c process msgctl");
    exit(1);
  }
}

int squidlights_client_quit(void) {
  /* cleanup! cleanup! everybody do your share! */
  printf("killing message queue\n");
  
  /* server will detect shutdown of queue */
  if(msgctl(client_msqid, IPC_RMID, NULL) == -1) {
    perror("msgctl");
  }
}

static int send_msg(void* msg, int size) {
  if(msgsnd(server_msqid, msg, size, 0) == -1) {
    perror("msgsnd in send_mesg");
    return -1;
  } else {
    return 0;
  }
}

int squidlights_client_light_on(int clientid, int light) {
  struct generic_msgbuf msg;
  msg.mtype = SQ_LIGHT_ON;
  msg.lightid = light;
  msg.clientid = clientid;
  return send_msg(&msg, SIZEOF_MSG(struct generic_msgbuf));
}
int squidlights_client_light_off(int clientid, int light) {
  struct generic_msgbuf msg;
  msg.mtype = SQ_LIGHT_OFF;
  msg.lightid = light;
  msg.clientid = clientid;
  return send_msg(&msg, SIZEOF_MSG(struct generic_msgbuf));
}
int squidlights_client_light_set(int clientid, int light, float brightness) {
  struct light_brightness_msg msg;
  msg.mtype = SQ_LIGHT_BRIGHTNESS;
  msg.lightid = light;
  msg.clientid = clientid;
  msg.brightness = brightness;
  return send_msg(&msg, SIZEOF_MSG(struct light_brightness_msg));
}
int squidlights_client_light_rgb(int clientid, int light, float r, float g, float b) {
  struct light_rgb_msg msg;
  msg.mtype = SQ_LIGHT_RGB;
  msg.lightid = light;
  msg.clientid = clientid;
  msg.r = r;
  msg.g = g;
  msg.b = b;
  return send_msg(&msg, SIZEOF_MSG(struct light_rgb_msg));
}
int squidlights_client_light_hsi(int clientid, int light, float h, float s, float i) {
  struct light_hsi_msg msg;
  msg.mtype = SQ_LIGHT_HSI;
  msg.lightid = light;
  msg.clientid = clientid;
  msg.h = h;
  msg.s = s;
  msg.i = i;
  return send_msg(&msg, SIZEOF_MSG(struct light_hsi_msg));
}
