#ifndef __LIGHTLOADER_MOUSE__
#define __LIGHTLOADER_MOUSE__

#include <stdint.h>

#define MOUSE_LEFTBTN (1 << 0)
#define MOUSE_RIGHTBTN (1 << 1)
#define MOUSE_MIDDLEBTN (1 << 2)
#define MOUSE_SIDEBTN0 (1 << 3)
#define MOUSE_SIDEBTN1 (1 << 4)

typedef struct light_mousepos {
  int32_t x, y;
  uint8_t btn_flags;
} light_mousepos_t;

void init_mouse();
bool has_mouse();
void reset_mousepos(uint32_t x, uint32_t y, uint32_t x_limit, uint32_t y_limit);

void get_mouse_delta(light_mousepos_t pos, int* dx, int* dy);

void limit_mousepos(light_mousepos_t* pos);

void set_previous_mousepos(light_mousepos_t mousepos);
void get_previous_mousepos(light_mousepos_t* pos);

#endif // !__LIGHTLOADER_MOUSE__
