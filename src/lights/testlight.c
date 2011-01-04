/* test light.  Creates two "lights".  The first is on/off, the second
   is brightness controlled. */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>

void print_states(void);

static int light0_state = 0;

void light0_on_handler(int lightid, int clientid) {
  light0_state = 1;
  print_states();
}
void light0_off_handler(int lightid, int clientid) {
  light0_state = 0;
  print_states();
}

static int light1_brightness = 0;
void light1_brightness_handler(int lightid, int clientid, float brightness) {
  light1_brightness = (int)(254*brightness);
  print_states();
}
void light1_on_handler(int lightid, int clientid) {
  light1_brightness_handler(lightid, clientid, 1.0);
}
void light1_off_handler(int lightid, int clientid) {
  light1_brightness_handler(lightid, clientid, 0.0);
}

void print_states(void) {
  if(light0_state) {
    printf("0:*\t1:");
  } else {
    printf("0: \t1:");
  }
  for(int i = 0; i < light1_brightness; i += 20) {
    printf("+");
  }
  printf("\n");
}

int main(void) {
  squidlights_light_initialize();
  int light0 = squidlights_light_connect("testlight_light0");
  if(light0 == SQ_CONNECTION_ERROR) exit(1);
  printf("light0=%d\n", light0);
  squidlights_light_add_on(light0, &light0_on_handler);
  squidlights_light_add_off(light0, &light0_off_handler);

  int light1 = squidlights_light_connect("testlight_light1");
  if(light1 == SQ_CONNECTION_ERROR) exit(1);
  printf("light1=%d\n", light1);
  squidlights_light_add_on(light1, &light1_on_handler);
  squidlights_light_add_off(light1, &light1_off_handler);
  squidlights_light_add_brightness(light1, &light1_brightness_handler);
  
  print_states();
  squidlights_light_run();
}
