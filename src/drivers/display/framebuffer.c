#include "framebuffer.h"
#include "efidef.h"
#include "efidevp.h"
#include "efiprot.h"
#include "lib/light_mainlib.h"
#include <mem/pmm.h>

LIGHT_STATUS _clean_framebuffer (light_framebuffer_t* fb);
LIGHT_STATUS _clear_framebuffer (light_framebuffer_t* fb);
LIGHT_STATUS _query_fb_mode_info (light_framebuffer_t* fb, size_t mode);
LIGHT_STATUS _find_fb_mode (light_framebuffer_t* fb, size_t w, size_t h, uint16_t bpp);

LIGHT_STATUS __init_framebuffer (light_framebuffer_t* ret, size_t width, size_t height, uint16_t bpp);

LIGHT_STATUS __prepare_gop_framebuffer(light_framebuffer_t* ret)
{
  EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  EFI_HANDLE* handle_ptr;
  UINTN handle_size = sizeof(EFI_HANDLE);

  /* Locate gop handle */
  EFI_STATUS gop_stat = g_light_info.boot_services->LocateHandle(ByProtocol, &gop_guid, NULL, &handle_size, handle_ptr);

  if (gop_stat != EFI_SUCCESS && gop_stat != EFI_BUFFER_TOO_SMALL) {
    return LIGHT_FAIL;
  }

  /* Allocate */
  handle_ptr = pmm_malloc(handle_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  /* Locate in the correct buffer */
  gop_stat = g_light_info.boot_services->LocateHandle(ByProtocol, &gop_guid, NULL, &handle_size, handle_ptr);

  if (gop_stat != EFI_SUCCESS) {
    pmm_free(handle_ptr, handle_size);
    return LIGHT_FAIL;
  }

  ret->m_gop_handle = handle_ptr[0];

  // not needed anymore
  pmm_free(handle_ptr, handle_size);

  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

  /* Grab the protocol */
  gop_stat = g_light_info.boot_services->HandleProtocol(ret->m_gop_handle, &gop_guid, (void**)&gop);

  if (gop_stat != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  ret->m_gop = gop;

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode_info;
  UINT32 mode_number = (gop->Mode == NULL) ? 0 : gop->Mode->Mode;
  UINTN mode_info_size;

  gop_stat = ret->m_gop->QueryMode(ret->m_gop, mode_number, &mode_info_size, &mode_info);

  if (gop_stat != EFI_SUCCESS) {
    // oof, try 0?
    if (mode_number != 0) {
      if (ret->m_gop->SetMode(ret->m_gop, 0)) {
        return LIGHT_FAIL;
      }
      gop_stat = ret->m_gop->QueryMode(ret->m_gop, mode_number, &mode_info_size, &mode_info);
    }
  }

  /* Still no success, sm went kinda wrong lol */
  if (gop_stat != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  /* Cache the mode and mode_info */
  ret->m_current_mode = ret->m_gop->Mode->Mode;
  ret->m_mode_info = mode_info;

  return LIGHT_SUCCESS;
}

struct fb_info {
  size_t width, height;
  uint16_t bpp;
};

/* Sorted from high to low */
static struct fb_info default_fb_infos[] = {
  { 1920, 1080, 32 },
  { 1680, 1050, 32 },
  { 1600, 900,  32 },
  { 1280, 1024, 32 },
  { 1280, 800,  32 },
  { 1280, 720,  32 },
  { 1024, 768,  32 },
  { 800, 600,   32 },
  { 720, 576,   32 },
  { 720, 480,   32 },
  { 640, 480,   32 },
};

static uint16_t default_fb_info_count = sizeof(default_fb_infos) / sizeof(*default_fb_infos);

light_framebuffer_t* init_optimal_framebuffer()
{

  struct fb_info* current;
  light_framebuffer_t* ret = pmm_malloc(sizeof(light_framebuffer_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

  if (__prepare_gop_framebuffer(ret) == LIGHT_FAIL)
    goto fail_and_out;

  for (uint16_t i = 0; i < default_fb_info_count; i++) {
    
    /* Select current framebuffer info we want to try */
    current = &default_fb_infos[i];
    
    /* Try to init */
    if (__init_framebuffer(ret, current->width, current->height, current->bpp) == LIGHT_SUCCESS)
      return ret;

    /* Try to init with a lower bpp, just in case */
    if (__init_framebuffer(ret, current->width, current->height, current->bpp - 8) == LIGHT_SUCCESS)
      return ret;

  }

fail_and_out:
  pmm_free(ret, sizeof(light_framebuffer_t));
  return NULL;
}

light_framebuffer_t* init_framebuffer (size_t width, size_t height, uint16_t bpp) {

  light_framebuffer_t* ret = pmm_malloc(sizeof(light_framebuffer_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

  if (__prepare_gop_framebuffer(ret) == LIGHT_FAIL)
    goto fail_and_out;

  if (__init_framebuffer(ret, width, height, bpp) == LIGHT_SUCCESS)
    return ret;

fail_and_out:
  pmm_free(ret, sizeof(light_framebuffer_t));
  return NULL;
}

LIGHT_STATUS __init_framebuffer (light_framebuffer_t* ret, size_t width, size_t height, uint16_t bpp) {

  if (!ret)
    return LIGHT_FAIL;

  ret->fCleanFramebuffer = _clean_framebuffer;
  ret->fClearFramebuffer = _clear_framebuffer;
  ret->fQueryGOModeInfo = _query_fb_mode_info;
  ret->fFindMode = _find_fb_mode;

  if (!ret->fFindMode(ret, width, height, bpp)) {
    return LIGHT_FAIL;
  }

  return LIGHT_SUCCESS;
}


LIGHT_STATUS _clean_framebuffer (light_framebuffer_t* fb) {
  return LIGHT_SUCCESS;
}

LIGHT_STATUS _clear_framebuffer (light_framebuffer_t* fb) {
  // credit to limine, I was too lazy to write this all down ;-;
  // TODO: rewrite
  for (size_t y = 0; y < fb->m_fb_height; y++) {
    switch (fb->m_fb_bpp) {
            case 32: {
                uint32_t *fbp = (void *)(uintptr_t)fb->m_fb_addr;
                size_t row = (y * fb->m_fb_pitch) / 4;
                for (size_t x = 0; x < fb->m_fb_width; x++) {
                    fbp[row + x] = 0;
                }
                break;
            }
            case 16: {
                uint16_t *fbp = (void *)(uintptr_t)fb->m_fb_addr;
                size_t row = (y * fb->m_fb_pitch) / 2;
                for (size_t x = 0; x < fb->m_fb_width; x++) {
                    fbp[row + x] = 0;
                }
                break;
            }
            default: {
                uint8_t *fbp = (void *)(uintptr_t)fb->m_fb_addr;
                size_t row = y * fb->m_fb_pitch;
                for (size_t x = 0; x < fb->m_fb_width * fb->m_fb_bpp; x++) {
                    fbp[row + x] = 0;
                }
                break;
            }
        }
  }

  return LIGHT_SUCCESS;
}

LIGHT_STATUS _query_fb_mode_info (light_framebuffer_t* fb, size_t mode) {

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* new_mode_info;
  UINTN new_mode_info_size;

  if (fb->m_gop->QueryMode(fb->m_gop, mode, &new_mode_info_size, &new_mode_info)) {
    return LIGHT_FAIL;
  }

  switch (new_mode_info->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
      fb->m_fb_bpp = 32;
      fb->m_red_mask_size = 8;
      fb->m_green_mask_size = 8;
      fb->m_blue_mask_size = 8;

      fb->m_red_mask_shift = 0;
      fb->m_green_mask_shift = 8;
      fb->m_blue_mask_shift = 16;
      break;
    case PixelBlueGreenRedReserved8BitPerColor:
      fb->m_fb_bpp = 32;
      fb->m_red_mask_size = 8;
      fb->m_green_mask_size = 8;
      fb->m_blue_mask_size = 8;

      fb->m_red_mask_shift = 16;
      fb->m_green_mask_shift = 8;
      fb->m_blue_mask_shift = 0;
      break;
    case PixelBitMask:
      return LIGHT_FAIL;
    default:
      return LIGHT_FAIL;
  }

  fb->m_fb_pitch = new_mode_info->PixelsPerScanLine * (fb->m_fb_bpp / 8);
  fb->m_fb_width = new_mode_info->HorizontalResolution;
  fb->m_fb_height = new_mode_info->VerticalResolution;

  return LIGHT_SUCCESS;
}

LIGHT_STATUS _find_fb_mode (light_framebuffer_t* fb, size_t w, size_t h, uint16_t bpp) {

  UINTN max_modes = fb->m_gop->Mode->MaxMode;

  for (size_t i = 0; i < max_modes; i++) {

    // we can't even convert the mode into the right framebuffer data -_-
    if (fb->fQueryGOModeInfo(fb, i) == LIGHT_FAIL) {
      continue;
    }

    bool check_w = fb->m_fb_width == w;
    bool check_h = fb->m_fb_height == h;
    bool check_bpp = fb->m_fb_bpp == bpp;

    if (!check_w || !check_h || !check_bpp) {
      continue;
    }

    // alright, we can work with this data
    if (i != fb->m_current_mode) {
      if (fb->m_gop->SetMode(fb->m_gop, i)) {
        // fail
        fb->m_current_mode = -1;
        continue;
      }
    }

    fb->m_current_mode = i;
    fb->m_fb_addr = fb->m_gop->Mode->FrameBufferBase;
    fb->fClearFramebuffer(fb);
    return LIGHT_SUCCESS;
  }

  return LIGHT_FAIL;
}

