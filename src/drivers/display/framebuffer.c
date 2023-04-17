#include "framebuffer.h"
#include "efidef.h"
#include <mem/pmm.h>

LIGHT_STATUS _clean_framebuffer (light_framebuffer_t* fb);
LIGHT_STATUS _clear_framebuffer (light_framebuffer_t* fb);
LIGHT_STATUS _query_fb_mode_info (light_framebuffer_t* fb, size_t mode);
LIGHT_STATUS _find_fb_mode (light_framebuffer_t* fb, size_t w, size_t h, uint16_t bpp);

light_framebuffer_t* init_framebuffer (size_t width, size_t height, uint16_t bpp) {

  light_framebuffer_t* ret = pmm_malloc(sizeof(light_framebuffer_t), MEMMAP_BOOTLOADER_RECLAIMABLE);

  ret->fCleanFramebuffer = _clean_framebuffer;
  ret->fClearFramebuffer = _clear_framebuffer;
  ret->fQueryGOModeInfo = _query_fb_mode_info;
  ret->fFindMode = _find_fb_mode;

  EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  EFI_HANDLE* handle_ptr;
  UINTN handle_size = sizeof(EFI_HANDLE);

  EFI_STATUS gop_stat = g_light_info.boot_services->LocateHandle(ByProtocol, &gop_guid, NULL, &handle_size, handle_ptr);

  if (gop_stat != EFI_SUCCESS && gop_stat != EFI_BUFFER_TOO_SMALL) {
    pmm_free(ret, sizeof(light_framebuffer_t));
    return NULL;
  }

  handle_ptr = pmm_malloc(handle_size, MEMMAP_BOOTLOADER_RECLAIMABLE);

  gop_stat = g_light_info.boot_services->LocateHandle(ByProtocol, &gop_guid, NULL, &handle_size, handle_ptr);

  if (gop_stat != EFI_SUCCESS) {
    pmm_free(handle_ptr, handle_size);
    pmm_free(ret, sizeof(light_framebuffer_t));
    return NULL;
  }

  ret->m_gop_handle = handle_ptr[0];

  // not needed anymore
  pmm_free(handle_ptr, handle_size);

  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

  gop_stat = g_light_info.boot_services->HandleProtocol(ret->m_gop_handle, &gop_guid, (void**)&gop);

  if (gop_stat != EFI_SUCCESS) {
    pmm_free(ret, sizeof(light_framebuffer_t));
    return NULL;
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
        pmm_free(ret, sizeof(light_framebuffer_t));
        return NULL;
      }
      gop_stat = ret->m_gop->QueryMode(ret->m_gop, mode_number, &mode_info_size, &mode_info);
    }
  }

  if (gop_stat != EFI_SUCCESS) {
    pmm_free(ret, sizeof(light_framebuffer_t));
    return NULL;
  }

  ret->m_current_mode = ret->m_gop->Mode->Mode;
  ret->m_mode_info = mode_info;

  if (!ret->fFindMode(ret, width, height, bpp)) {
    pmm_free(ret, sizeof(light_framebuffer_t));
    return NULL;
  }

  return ret;
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
      break;
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

