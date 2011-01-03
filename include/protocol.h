#ifndef _squidlights_protocol_h
#define _squidlights_protocol_h

/* Here lies the squidlights protocol (the connection protocol and
   data protocol for both clients [things which want to blink lights]
   and servers [things which want to be blinked]).

   The data protocol is reminiscent to MIDI in that clients sequence
   brightness/on/off changes to lights. */

/*** Return messages (and errors) ***/
#define SQ_NOT_CONNECTED -2200  /* if trying to interact when haven't
				   yet connected */
#define SQ_CONNECTION_ERROR -2201
#define SQ_NAME_TAKEN -2202
#define SQ_UNDEFINED_LIGHT -2203

/*** Protocol ***/
#define SQ_SERVER_MSG_ID 222220

#define SQ_LIGHT_SET_NAME 1 /* this message must be sent only once */
#define SQ_LIGHT_ON 2
#define SQ_LIGHT_OFF 3
#define SQ_LIGHT_BRIGHTNESS 4
#define SQ_LIGHT_RGB 5
#define SQ_LIGHT_HSI 6
#define SQ_DIE 7 /* sent by the server to kill everything */
#define SQ_CLIENT_SET_NAME 8 /* again, only once */

struct generic_msgbuf {
  long mtype;
  int lightid;
  int clientid;
  char mtext[256];
};

struct light_init_msg {
  long mtype;
  int lightid;
  int msqid;
  char name[100]; /* name is actually 32 bytes.  padding for safety! */
};

struct light_brightness_msg {
  long mtype;
  int lightid;
  int clientid;
  float brightness;
};

struct light_rgb_msg {
  long mtype;
  int lightid;
  int clientid;
  float r;
  float g;
  float b;
};

struct light_hsi_msg {
  long mtype;
  int lightid;
  int clientid;
  float h;
  float s;
  float i;
};

/*** client functions ***/

/* connect to squidlights and register with with identifier "name".
   Returns either when there is an error, or once the connection is
   established.  The value it returns is the client id.

   Errors:
   
   * if no server is running, SQ_CONNECTION_ERROR is returned

   * if the name is taken, SQ_NAME_TAKEN is returned. */
int squidlights_client_connect(char* name);

/* Get a pointer to the list of light names */
char** squidlights_client_lightnames(int clientid);

/* Gets the id of the light with a particular name. Returns -1 if no
   such light. */
int squidlights_client_getlight(int clientid, char* name);

/* Send a signals to a particular light by id. brightness, r, g, b, h,
   s, and i are floats in the range [0,1].

   Errors:

   * if no such light id, returns SQ_UNDEFINED_LIGHT */
int squidlights_client_light_on(int clientid, int light);
int squidlights_client_light_off(int clientid, int light);
int squidlights_client_light_set(int clientid, int light, float brightness);
int squidlights_client_light_rgb(int clientid, int light, float r, float g, float b);
int squidlights_client_light_hsi(int clientid, int light, float h, float s, float i);

/*** light server functions ***/

/** note: in these, lightid is local to the process **/

/* initializes the light system for this process */
int squidlights_light_initialize(void);

/* connect to squidlights and register with identifier "name". Returns
   either when there is an error, or once the connection is
   established.  Returns the light's id number.

   Errors:

   * if no server is running, SQ_CONNECTION_ERROR is returned

   * if the name is taken, SQ_NAME_TAKEN is returned */
int squidlights_light_connect(char* name);

/* gets the name of a light server by id.  Returns SQ_UNDEFINED_LIGHT
   if doesn't exist. */
char* squidlights_light_getname(int lightid);

/* set an on/off handler, attaching to the light's id.  must be a function
   which takes a clientid (integer).  Returns SQ_UNDEFINED_LIGHT if no
   such light. */
int squidlights_light_add_on(int lightid, void(*on_handler)(int lightid, int clientid));
int squidlights_light_add_off(int lightid, void(*off_handler)(int lightid, int clientid));

/* set brightness/rgb/hsi handler.  Similar principle to on/off handlers. */
int squidlights_light_add_brightness(int lightid, void(*brightness_handler)(int lightid, int clientid, float brightness));
int squidlights_light_add_rgb(int lightid, void(*rgb_handler)(int lightid, int clientid, float r, float g, float b));
int squidlights_light_add_hsi(int lightid, void(*hsi_handler)(int lightid, int clientid, float h, float s, float i));

/* once set up, just runs the lights */
void squidlights_light_run(void);

#endif
