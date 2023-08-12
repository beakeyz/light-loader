#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "efiprot.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include <framebuffer.h>

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_HANDLE gop_handle;
static EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
static EFI_GUID conout_guid = EFI_CONSOLE_OUT_DEVICE_GUID;

/*
 * Find the active framebuffer on the system
 */
void
init_framebuffer()
{
  size_t handlelist_size;
  EFI_STATUS status;
  EFI_HANDLE dummy_handle;
  EFI_HANDLE* handle_list;
  EFI_GUID gop_guid = gEfiGraphicsOutputProtocolGuid;

  size_t handle_count = locate_handle_with_buffer(ByProtocol, gop_guid, &handlelist_size, &handle_list);

  /* No GOP handles??? wtf */
  if (!handle_count)
    return;

  gop_handle = nullptr;

  for (uintptr_t i = 0; i < handle_count; i++) {

    status = BS->HandleProtocol(handle_list[i], &gop_guid, (void**)&gop);

    if (status != EFI_SUCCESS)
      continue;

    status = open_protocol(handle_list[i], &conout_guid, &dummy_handle);

    if (status == EFI_SUCCESS) {
      gop_handle = handle_list[i];
      break;
    }

    /* Lock in the first handle if we didn't find the one that ST->ConOut uses */
    if (!gop_handle)
      gop_handle = handle_list[i];
  }

  /* Make sure to release the handle buffer */
  heap_free(handle_list);

  /* FIXME: might be UGA :sigh: */
  if (!gop_handle)
    return;

  light_gfx_t* gfx;

  get_light_gfx(&gfx);

  gfx->type = GFX_TYPE_GOP;
  gfx->priv = gop;

  /* Get EDID */
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* mode = gop->Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = mode->Info;
  EFI_GRAPHICS_PIXEL_FORMAT format = info->PixelFormat;
  EFI_PIXEL_BITMASK pinfo = info->PixelInformation;


  /* Get gfx framebuffer info */
  gfx->phys_addr = gop->Mode->FrameBufferBase;
  gfx->size = gop->Mode->FrameBufferSize;
  gfx->width = gop->Mode->Info->HorizontalResolution;
  gfx->height = gop->Mode->Info->VerticalResolution;
  gfx->stride = gop->Mode->Info->PixelsPerScanLine;

  gfx->bpp = 0;
  gfx->red_mask = gfx->green_mask = gfx->blue_mask = gfx->alpha_mask = 0;

  switch (format) {
    case PixelBltOnly:
      gfx->bpp = 32;
      gfx->red_mask =     0x000000ff;
      gfx->green_mask =   0x0000ff00;
      gfx->blue_mask =    0x00ff0000;
      gfx->alpha_mask =   0xff000000;
      break;
	case PixelBlueGreenRedReserved8BitPerColor:
      gfx->bpp = 32;
      gfx->red_mask =     0x00ff0000;
      gfx->green_mask =   0x0000ff00;
      gfx->blue_mask =    0x000000ff;
      gfx->alpha_mask =   0xff000000;
      break;
	case PixelBitMask:
      gfx->red_mask =     pinfo.RedMask;
      gfx->green_mask =   pinfo.GreenMask;
      gfx->blue_mask =    pinfo.BlueMask;
      gfx->alpha_mask =   pinfo.ReservedMask;
      break;
	default:
      break;
  }

  /* Let's count the bits lol */
  if (!gfx->bpp) {
    uint32_t full_mask = gfx->red_mask | gfx->green_mask | gfx->blue_mask | gfx->alpha_mask;

    for (uint8_t i = 0; i < 32; i++) {
      /* Simply count the amount of bits set in the full mask */
      if (full_mask & (1 << i)) {
        gfx->bpp++;
      }
    }
  }

  /* Allocate a backbuffer (TODO: find a reason to implement it) */
  /*
  gfx->back_fb_pages = EFI_SIZE_TO_PAGES(gfx->height * gfx->width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

  status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, gfx->back_fb_pages, (EFI_PHYSICAL_ADDRESS*)&gfx->back_fb);

  if (status != EFI_SUCCESS) {
    gfx->back_fb = NULL;
    gfx->back_fb_pages = NULL;
  }
  */

  /* Make sure to release the gfx again */
  put_light_gfx();
}
