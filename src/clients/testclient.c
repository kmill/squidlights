/* test client.  prints the lights and dies. */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  if(squidlights_client_initialize() == -1) {
    printf("Something's wrong\n");
    exit(1);
  }
  
  int clientid = squidlights_client_connect("testclient");
  squidlights_client_process_messages(); {
    printf("Lights:\n");
    for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
      char* lightname = squidlights_client_lightname(i);
      if(lightname[0] != '\0') {
 	printf("%d. %s\n", i, lightname);
      }
    }
  }

  int light0 = squidlights_client_getlight("testlight_light0");
  int light1 = squidlights_client_getlight("testlight_light1");
  
  int i = 0;
  int ok = 0;
  while(ok != -1) {
    printf(".\n");
    ok = squidlights_client_light_set(clientid, light0, i++%2==0?0.0:1.0);
    ok &= squidlights_client_light_set(clientid, light1, (i%5)/5.0);
    usleep(200000);
  }

  squidlights_client_quit();
}
