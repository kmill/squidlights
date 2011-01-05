/* this is a squidlight client for puredata. */

#include "m_pd.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQLIGHT_CLIENT_UPDATE_MSEC 500

static t_class *sqlight_class;

typedef struct _sqlight {
  t_object x_obj;
  t_symbol * i_light;
  t_int i_connected;
  t_int i_clientid;
  t_int i_type; /* 0=on/off 1=brightness 2=rgb 3=hsi */
  t_int i_main_type; /* for data going into active inlet */
  t_int i_onq; /* is it on? */
  t_float i_bright; /* brightness */
  t_float i_cr, i_cg, i_cb; /* rgb colors */
  t_float i_ch, i_cs, i_ci; /* hsi colors */
} t_sqlight;

#define SQLIGHT_NO_TYPE (-1)
#define SQLIGHT_ON_OFF 0
#define SQLIGHT_BRIGHT 1
#define SQLIGHT_RGB 2
#define SQLIGHT_HSI 3

static t_clock * sqlight_clock;

static char sqlight_initialized = 0;
static long sqlight_reconnected_id = 0;

void sqlight_send_data(t_sqlight *x);
void sqlight_connect(t_sqlight *x);

void sqlight_client_on(t_sqlight *x);
void sqlight_client_off(t_sqlight *x);
void sqlight_client_switch(t_sqlight *x, t_floatarg onq);
void sqlight_client_set(t_sqlight *x, t_floatarg b);
void sqlight_client_rgb(t_sqlight *x, t_floatarg r, t_floatarg g, t_floatarg b);
void sqlight_client_hsi(t_sqlight *x, t_floatarg h, t_floatarg s, t_floatarg i);

void sqlight_send_data(t_sqlight *x) {
  if(x->i_connected < sqlight_reconnected_id) {
    sqlight_connect(x);
  }
  if(x->i_connected == sqlight_reconnected_id) {
    int lightid = squidlights_client_getlight(x->i_light->s_name);
    if(lightid == SQ_UNDEFINED_LIGHT) {
      post("sqlight: no such light \"%s\"", x->i_light->s_name);
      return;
    }
    switch(x->i_type) {
    case SQLIGHT_ON_OFF :
      if(x->i_onq) {
	squidlights_client_light_on(x->i_clientid, lightid);
      } else {
	squidlights_client_light_off(x->i_clientid, lightid);
      }
      break;
    case SQLIGHT_BRIGHT :
      squidlights_client_light_set(x->i_clientid, lightid, x->i_bright);
      break;
    case SQLIGHT_RGB :
      squidlights_client_light_rgb(x->i_clientid, lightid, x->i_cr, x->i_cg, x->i_cb);
      break;
    case SQLIGHT_HSI :
      squidlights_client_light_hsi(x->i_clientid, lightid, x->i_ch, x->i_cs, x->i_ci);
      break;
    default :
      error("sqlight: no such light type %d", (int)x->i_type);
    }
  } else {
    post("sqlight: not sending since not connected");
  }
}

void sqlight_bang(t_sqlight *x) {
  sqlight_send_data(x);
}

void sqlight_client_print_lights(t_sqlight *x) {
  post("Hello world \"%s\"!!", x->i_light->s_name);
  post("Lights:");
  for(int i = 0; i < NUM_LIGHT_SERVERS; i++) {
    char* lightname = squidlights_client_lightname(i);
    if(lightname[0] != '\0') {
      post("%d. %s", i, lightname);
    }
  }
}

void sqlight_float(t_sqlight *x, t_floatarg f) {
  switch(x->i_main_type) {
  case SQLIGHT_NO_TYPE :
    error("sqlight: first input doesn't handle floats unless instantiated with switch, bright, rgb, or hsi");
    break;
  case SQLIGHT_ON_OFF :
    sqlight_client_switch(x, f);
    sqlight_send_data(x);
    break;
  case SQLIGHT_BRIGHT :
    sqlight_client_set(x, f);
    sqlight_send_data(x);
    break;
  case SQLIGHT_RGB :
    sqlight_client_rgb(x, f, x->i_cg, x->i_cb);
    sqlight_send_data(x);
    break;
  case SQLIGHT_HSI :
    sqlight_client_hsi(x, f, x->i_cs, x->i_ci);
    sqlight_send_data(x);
    break;
  default :
    error("sqlight: no such light type %d", (int)x->i_main_type);
  }
}

void sqlight_client_on(t_sqlight *x) {
  x->i_type = SQLIGHT_ON_OFF;
  x->i_onq = 1;
}
void sqlight_client_off(t_sqlight *x) {
  x->i_type = SQLIGHT_ON_OFF;
  x->i_onq = 0;
}
void sqlight_client_switch(t_sqlight *x, t_floatarg onq) {
  x->i_type = SQLIGHT_ON_OFF;
  x->i_onq = (int)onq;
}
void sqlight_client_set(t_sqlight *x, t_floatarg b) {
  x->i_type = SQLIGHT_BRIGHT;
  x->i_bright = b;
}
void sqlight_client_rgb(t_sqlight *x, t_floatarg r, t_floatarg g, t_floatarg b) {
  x->i_type = SQLIGHT_RGB;
  x->i_cr = r;
  x->i_cg = g;
  x->i_cb = b;
}
void sqlight_client_hsi(t_sqlight *x, t_floatarg h, t_floatarg s, t_floatarg i) {
  x->i_type = SQLIGHT_HSI;
  x->i_ch = h;
  x->i_cs = s;
  x->i_ci = i;
}

void sqlight_connect(t_sqlight *x) {
  if(x->i_connected == sqlight_reconnected_id) {
    post("sqlight already connected");
  } else {
    /* generate name */
    char name[128];
    sprintf(name, "pd-%hi,%hi", x->x_obj.te_xpix, x->x_obj.te_ypix);
    post("sqlight: adding client %s", name);
    int ret = squidlights_client_connect(name);
    if(ret == SQ_CONNECTION_ERROR) {
      post("sqlight couldn't connect to server.");
    } else {
      x->i_connected = sqlight_reconnected_id;
      x->i_clientid = ret;
      
      sqlight_client_print_lights(x);
    }
  }
}

void *sqlight_new(t_symbol * light, t_symbol * method) {
  t_sqlight *x = (t_sqlight *)pd_new(sqlight_class);
  x->i_light = light;
  
  if(sqlight_initialized) {
    sqlight_connect(x);
  }

  if(method == gensym("switch")) {
    x->i_main_type = SQLIGHT_ON_OFF;
  } else if(method == gensym("bright")) {
    x->i_main_type = SQLIGHT_BRIGHT;
  } else if(method == gensym("rgb")) {
    x->i_main_type = SQLIGHT_RGB;
    floatinlet_new(&x->x_obj, &x->i_cg);
    floatinlet_new(&x->x_obj, &x->i_cb);
  } else if(method == gensym("hsi")) {
    x->i_main_type = SQLIGHT_HSI;
    floatinlet_new(&x->x_obj, &x->i_cs);
    floatinlet_new(&x->x_obj, &x->i_ci);
  } else if(method == gensym("")) {
    x->i_main_type = SQLIGHT_NO_TYPE;
  } else {
    x->i_main_type = SQLIGHT_NO_TYPE;
    error("sqlight: no such type \"%s\".  use switch, bright, rgb, or hsi.", method->s_name);
  }


  symbolinlet_new(&x->x_obj, &x->i_light);

  return (void *)x;
}

void sqlight_free(t_sqlight *x) {
  post("sqlight freed");
}

static void sqlight_clock_handler(t_object * x) {
  if(squidlights_client_process_messages()) {
    post("sqlight: couldn't process messages (assuming server died.  will work after reconnect.)");
    sqlight_reconnected_id++; /* force sqlights to reconnect.  probably server death */
  } else {
    //    post("sqlight: updated.");
  }
  clock_delay(sqlight_clock, SQLIGHT_CLIENT_UPDATE_MSEC);
}

void sqlight_cleanup(void) {
  squidlights_client_quit();
}

void sqlight_setup(void) {
  if(atexit(sqlight_cleanup)) {
    error("sqlight: Couldn't add exit handler.  Not initializing.");
    sqlight_initialized = 0;
  } else {
    if(squidlights_client_initialize()) {
      error("Couldn't initialize squidlights client.  You'll need to restart PD.");
      sqlight_initialized = 0;
    } else {
      sqlight_initialized = 1;
    }
  }

  sqlight_reconnected_id = 1;

  sqlight_clock = clock_new((t_object*)NULL, (t_method)sqlight_clock_handler);
  clock_delay(sqlight_clock, SQLIGHT_CLIENT_UPDATE_MSEC);

  sqlight_class = class_new(gensym("sqlight"),
			    (t_newmethod)sqlight_new,
			    (t_method)sqlight_free, sizeof(t_sqlight),
			    CLASS_DEFAULT, A_DEFSYMBOL, A_DEFSYMBOL, 0);
  class_addbang(sqlight_class, sqlight_bang);
  class_addfloat(sqlight_class, sqlight_float);
  class_addmethod(sqlight_class, (t_method)sqlight_client_print_lights, gensym("lights"), 0);
  class_addmethod(sqlight_class, (t_method)sqlight_client_on, gensym("on"), 0);
  class_addmethod(sqlight_class, (t_method)sqlight_client_off, gensym("off"), 0);
  class_addmethod(sqlight_class, (t_method)sqlight_client_switch, gensym("switch"), A_DEFFLOAT, 0);
  class_addmethod(sqlight_class, (t_method)sqlight_client_set, gensym("set"), A_DEFFLOAT, 0);
  class_addmethod(sqlight_class, (t_method)sqlight_client_rgb, gensym("rgb"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
  class_addmethod(sqlight_class, (t_method)sqlight_client_hsi, gensym("hsi"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
  //  class_addlist(sqlight_class, (t_method)sqlight_setsignal);
}
