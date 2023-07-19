#ifndef __LIGHTLOADER_KBDRIVER__
#define __LIGHTLOADER_KBDRIVER__

#include <lib/light_mainlib.h>

typedef struct {
  EFI_KEY_DATA keyData;
  EFI_STATUS status;
} kb_press_packet_t;

// TODO: custom kb driver from a filehandle?
void init_kb_driver();
LIGHT_STATUS read_kb_press(kb_press_packet_t* pck);
void flush_kb_buffer();

#endif // !__LIGHTLOADER_KBDRIVER__
