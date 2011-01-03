/* the server mediator thing which connects clients with light
   servers.  The point is to make sure things go where they're
   supposed to go. */

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

#define NUM_LIGHT_SERVERS 256

struct client_s {
  char name[32];
};

struct light_server_s {
  char name[32];
  char islight;
  int lightid; /* this is for the light server.  the client refers to
		  a different id */
  int light_msqid;
};

struct light_server_s light_servers[NUM_LIGHT_SERVERS];

int get_free_light_id(void) {
  for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
    if(!light_servers[i].islight) {
      return i;
    }
  }
  return -1;
}

void kill_lights_and_clients(void) {
  struct generic_msgbuf msg;
  msg.mtype = SQ_DIE;
  for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
    if(light_servers[i].islight) {
      light_servers[i].islight = 0;
      msgsnd(light_servers[i].light_msqid, &msg, 256, 0);
    }
  }
}

static int server_msqid;

static volatile sig_atomic_t lights_keep_running;

void server_sigint_handler(int sig) {
  lights_keep_running = 0;
}

void run(void) {
  int id;
  struct generic_msgbuf buf;
  int tries = 0;
  printf("server running...\n");
  lights_keep_running = 1;
  while(lights_keep_running && tries < 5) {
    if(msgrcv(server_msqid, &buf, 256, 0, 0) == -1) {
      perror("server.c, run msgrcv");
      printf("something catastrophic happened to the message queue?\n");
      tries++;
    } else {
      tries = 0;
      switch(buf.mtype) {
      case SQ_LIGHT_SET_NAME :
	printf("adding light...\n");
	id = get_free_light_id();
	if(id == -1) {
	  printf("can't.  too many lights already.\n");
	} else {
	  struct light_init_msg * buf2 = (struct light_init_msg *) &buf;
	  strcpy(light_servers[id].name, buf2->name);
	  light_servers[id].islight = 1;
	  light_servers[id].lightid = buf2->lightid;
	  light_servers[id].light_msqid = buf2->msqid;

	  printf("Added light %d \"%s\" with id %d.\n", id, light_servers[id].name,
		 light_servers[id].lightid);
	}
	break;
      case SQ_LIGHT_ON :
      case SQ_LIGHT_OFF :
      case SQ_LIGHT_BRIGHTNESS :
      case SQ_LIGHT_RGB :
      case SQ_LIGHT_HSI :
	printf("forwarding...");
	if(!light_servers[buf.lightid].islight) {
	  printf("not a light\n");
	}
	/* change the id to something the light server understands
	   (and reuse the data structure). */
	id = buf.lightid;
	buf.lightid = light_servers[id].lightid;
	if(msgsnd(light_servers[id].light_msqid, &buf, 256, 0) == -1) {
	  printf("Lost light %d. Removing...\n", id);
	  light_servers[id].islight = 0;
	}
      }
    }
  }
  kill_lights_and_clients();
}

int main(void) {
  struct sigaction sa;
  sa.sa_handler = server_sigint_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  if(sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction");
  }

  server_msqid = msgget(SQ_SERVER_MSG_ID, 0666 | IPC_CREAT);
  if(server_msqid == -1) {
    perror("msgget");
    printf("Server couldn't open the message queue...");
    exit(1);
  }

  for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
    light_servers[i].islight = 0;
  }

  run();

  printf("Closing messaging queue...\n");
  if(msgctl(server_msqid, IPC_RMID, NULL) == -1) {
    perror("msgctl");
  }
}
