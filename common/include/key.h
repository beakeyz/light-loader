#ifndef __LIGHTLOADER_KEY__
#define __LIGHTLOADER_KEY__

#include <stdint.h>

typedef struct light_key {
    uint16_t typed_char;
    uint16_t scancode;
} light_key_t;

void init_keyboard();
bool has_keyboard();
void get_keyboard_flags(uint32_t* flags);

void reset_keyboard();

#endif // !__LIGHTLOADER_KEY__
