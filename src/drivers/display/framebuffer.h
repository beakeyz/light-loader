#ifndef __LIGHTLOADER_FRAMEBUFFER__ 
#define __LIGHTLOADER_FRAMEBUFFER__
#include <lib/light_mainlib.h>

struct light_framebuffer;

typedef LIGHT_STATUS (*FRAMEBUFFER_CLEAN) (
  struct light_framebuffer* fb
);

typedef LIGHT_STATUS (*FRAMEBUFFER_CLEAR) (
  struct light_framebuffer* fb
);

typedef LIGHT_STATUS (*QUERY_FRAMEBUFFER_GOP_MODE_INFO) (
  struct light_framebuffer* fb,
  size_t mode
);

typedef LIGHT_STATUS (*FRAMEBUFFER_FIND_MODE_FOR_DIMENTIONS) (
  struct light_framebuffer* fb,
  size_t width,
  size_t height,
  uint16_t bpp
);

typedef struct light_framebuffer {
  uintptr_t m_fb_addr;

  uintptr_t m_fb_width;
  uintptr_t m_fb_height;
  uintptr_t m_fb_pitch;
  uintptr_t m_fb_bpp;

  uint8_t  m_red_mask_size;
  uint8_t  m_red_mask_shift;
  uint8_t  m_green_mask_size;
  uint8_t  m_green_mask_shift;
  uint8_t  m_blue_mask_size;
  uint8_t  m_blue_mask_shift;

  EFI_GRAPHICS_OUTPUT_PROTOCOL* m_gop;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m_mode_info;
  EFI_HANDLE m_gop_handle;

  size_t m_current_mode;

  FRAMEBUFFER_CLEAN fCleanFramebuffer;
  FRAMEBUFFER_CLEAR fClearFramebuffer;
  QUERY_FRAMEBUFFER_GOP_MODE_INFO fQueryGOModeInfo;
  FRAMEBUFFER_FIND_MODE_FOR_DIMENTIONS fFindMode;

} __attribute__((packed)) light_framebuffer_t;

light_framebuffer_t* init_framebuffer (size_t width, size_t height, uint16_t bpp);

#endif // !
