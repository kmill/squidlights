/* test light.  Creates two "lights".  The first is on/off, the second
   is brightness controlled. */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "lo/lo.h"
#include <math.h>
#include <sys/time.h>

#define ELMO_UDP_PORT "2222"
#define ELMO_COMMAND "/light/color/set"

struct elmo_light_s {
  lo_address addr;
  int lightid;
  float r, g, b, i; /* always have r+g+b=1, and i is coefficient */
};

static struct elmo_light_s elmo_lights[16];
static int next_handle=0;

void elmo_rgb_handler(int lightid, int clientid, float r, float g, float b);
int elmo_light_send(struct elmo_light_s * handle, float r, float g, float b);

void elmo_brightness_handler(int lightid, int clientid, float brightness) {
  struct elmo_light_s * handle = &elmo_lights[squidlights_light_attached_data(lightid)];
  handle->i = brightness;
}
void elmo_on_handler(int lightid, int clientid) {
  elmo_brightness_handler(lightid, clientid, 1.0);
}
void elmo_off_handler(int lightid, int clientid) {
  elmo_brightness_handler(lightid, clientid, 0.0);
}
void elmo_rgb_handler(int lightid, int clientid, float r, float g, float b) {
  struct elmo_light_s * handle = &elmo_lights[squidlights_light_attached_data(lightid)];
  float sum = r+g+b;
  if(sum == 0) { /* if black, then reset to lovely purple */
    handle->r = 0.8;
    handle->g = 0.0;
    handle->b = 0.2;
  } else {
    handle->r = r/sum;
    handle->g = g/sum;
    handle->b = b/sum;
  }
  handle->i = sum;
}

static inline float deg_to_rad(float d) {
  return d*M_PI/180.0;
}
void elmo_hsi_handler(int lightid, int clientid, float h, float s, float i) {
  struct elmo_light_s * handle = &elmo_lights[squidlights_light_attached_data(lightid)];
  float r, g, b;
  h = fmod(h, 360.0);
  if(h < 120) {
    r = (1+s*cos(deg_to_rad(h))/cos(deg_to_rad(60-h)))/3;
    g = (1+s*(1-cos(deg_to_rad(h))/cos(deg_to_rad(60-h))))/3;
    b = (1-s)/3;
  } else if(h < 240) {
    h = h - 120;
    g = (1+s*cos(deg_to_rad(h))/cos(deg_to_rad(60-h)))/3;
    b = (1+s*(1-cos(deg_to_rad(h))/cos(deg_to_rad(60-h))))/3;
    r = (1-s)/3;
  } else {
    h = h - 240;
    b = (1+s*cos(deg_to_rad(h))/cos(deg_to_rad(60-h)))/3;
    r = (1+s*(1-cos(deg_to_rad(h))/cos(deg_to_rad(60-h))))/3;
    g = (1-s)/3;
  }
  handle->r = r;
  handle->g = g;
  handle->b = b;
}

int update_lights() {
  for(int i = 0; i < next_handle; i++) {
    struct elmo_light_s * handle = &elmo_lights[i];
    float r = handle->r, g = handle->g, b = handle->b, i = handle->i;
    elmo_light_send(handle, r*i, g*i, b*i);
  }
  return 0;
}

int initialize_elmo_light(char * address, char * name) {
  struct elmo_light_s * handle = elmo_lights+next_handle;
  handle->addr = lo_address_new(address, ELMO_UDP_PORT);
  handle->lightid = squidlights_light_connect(name);
  squidlights_light_attach_data(handle->lightid, next_handle);
  handle->r = 0.8;
  handle->g = 0.0;
  handle->b = 0.2;
  handle->i = 0.0;

  squidlights_light_add_on(handle->lightid, &elmo_on_handler);
  squidlights_light_add_off(handle->lightid, &elmo_off_handler);
  squidlights_light_add_brightness(handle->lightid, &elmo_brightness_handler);
  squidlights_light_add_rgb(handle->lightid, &elmo_rgb_handler);
  squidlights_light_add_hsi(handle->lightid, &elmo_hsi_handler);

  return next_handle++;
}

int elmo_light_send(struct elmo_light_s * handle, float r, float g, float b) {
  return lo_send(handle->addr, ELMO_COMMAND, "fff", r, g, b);
}

int main(void) {
  squidlights_light_initialize();

  int elmo0 = initialize_elmo_light("18.224.0.163", "elmo0"); // scheme.mit.edu
  if(elmo0 == SQ_CONNECTION_ERROR) exit(1);
  int elmo1 = initialize_elmo_light("18.224.0.168", "elmo1"); // haskell.mit.edu
  if(elmo1 == SQ_CONNECTION_ERROR) exit(1);
  //  elmo_light_send(&elmo_lights[elmo0], 1.0, 0.1, 0.9);
  //  squidlights_light_add_on(light0, &light0_on_handler);
  //  squidlights_light_add_off(light0, &light0_off_handler);

  //  int elmo1 = squidlights_light_connect("elmo1");
  //  if(elmo1 == SQ_CONNECTION_ERROR) exit(1);
  //  squidlights_light_add_on(light1, &light1_on_handler);
  //  squidlights_light_add_off(light1, &light1_off_handler);
  //  squidlights_light_add_brightness(light1, &light1_brightness_handler);

  squidlights_lights_handle_init();

  struct timeval tv, tv2;
  gettimeofday(&tv, NULL);
  while(squidlights_lights_handle(1) != -1) {
    gettimeofday(&tv2, NULL);
    if(tv2.tv_sec>tv.tv_sec || tv2.tv_usec-tv.tv_usec >= 33000) {
      update_lights();
      tv = tv2;
    }
  }
  squidlights_lights_cleanup();
}
