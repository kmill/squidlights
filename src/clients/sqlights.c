/* light control by the command line */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void print_usage(char* prgname) {
    printf("usage: %s\n"
	   "\tlist\n"
	   "\ton (lightname)\n"
	   "\toff (lightname)\n"
	   "\tset (lightname) (brightness)\n"
	   "\trgb (lightname) (r) (g) (b)\n"
	   "\thsi (lightname) (h) (s) (i)\n\n"
	   "use . for lightname to send the signal to all lights\n",
	   prgname);
}

float read_arg_float(int argc, char** argv, int i) {
  if(i < 0) return 0;
  if(i >= argc) return 0;
  return (float)atof(argv[i]);
}

// assumes enough arguments.
void handle_command(int clientid, int argc, char** argv, int lightid) {
  if(strcmp(argv[1], "on")==0) {
    printf("turning %s on\n", squidlights_client_lightname(lightid));
    squidlights_client_light_on(clientid,lightid);
  } else if(strcmp(argv[1], "off")==0) {
    printf("turning %s off\n", squidlights_client_lightname(lightid));
    squidlights_client_light_off(clientid,lightid);
  } else if(strcmp(argv[1], "set")==0) {
    float b = read_arg_float(argc, argv, 3);
    printf("turning %s to %f\n", squidlights_client_lightname(lightid), b);
    squidlights_client_light_set(clientid,lightid, b);
  } else if(strcmp(argv[1], "rgb")==0) {
    float r = read_arg_float(argc, argv, 3);
    float g = read_arg_float(argc, argv, 4);
    float b = read_arg_float(argc, argv, 5);
    printf("turning %s to rgb=(%f,%f,%f)\n", squidlights_client_lightname(lightid), r,g,b);
    squidlights_client_light_rgb(clientid,lightid, r, g, b);
  } else if(strcmp(argv[1], "hsi")==0) {
    float h = read_arg_float(argc, argv, 3);
    float s = read_arg_float(argc, argv, 4);
    float i = read_arg_float(argc, argv, 5);
    printf("turning %s to hsi=(%f,%f,%f)\n", squidlights_client_lightname(lightid), h,s,i);
    squidlights_client_light_hsi(clientid,lightid, h,s,i);
  }
}

int main(int argc, char** argv) {
  if(squidlights_client_initialize() == -1) {
    printf("Something's wrong\n");
    exit(1);
  }
  
  int clientid = squidlights_client_connect("sqlights");
  squidlights_client_process_messages();

  if(argc == 1) {
    print_usage(argv[0]);
  } else if(strcmp(argv[1], "list") == 0) {
    printf("Registered lights:\n\n");
    for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
      char* lightname = squidlights_client_lightname(i);
      if(lightname[0] != '\0') {
	printf("%s ", lightname);
      }
    }
    printf("\n");
  } else {
    if(argc < 3) {
      print_usage(argv[0]);
    } else {
      if(strcmp(argv[2], ".") == 0) {
	for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
	  char* lightname = squidlights_client_lightname(i);
	  if(lightname[0] != '\0') {
	    handle_command(clientid, argc, argv, i);
	  }
	}
      } else {
	int lightid = squidlights_client_getlight(argv[2]);
	if(lightid == SQ_UNDEFINED_LIGHT) {
	  printf("No such light\n");
	} else {
	  handle_command(clientid, argc, argv, lightid);
	}
      }
    }
  }

  printf("\nquitting... ");
  squidlights_client_quit();
}
