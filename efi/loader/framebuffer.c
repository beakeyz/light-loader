#include "efiapi.h"
#include "efidef.h"
#include "efierr.h"
#include "efilib.h"
#include "efiprot.h"
#include "gfx.h"
#include "heap.h"
#include "stddef.h"
#include <memory.h>
#include <framebuffer.h>

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_HANDLE gop_handle;
static EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
static EFI_GUID conout_guid = EFI_CONSOLE_OUT_DEVICE_GUID;

#define RES_480P 0
#define RES_720P 1
#define RES_1080P 2
#define RES_4K 3

/*
 * named_resolution struct from FreeBSD at
 * https://github.com/freebsd/freebsd-src/blob/main/stand/efi/loader/framebuffer.c
 */
static const struct named_resolution {
	const char *name;
	const char *alias;
	uint32_t width;
	uint32_t height;
} resolutions[] = {
	{
		.name = "480p",
		.width = 640,
		.height = 480,
	},
	{
		.name = "720p",
		.width = 1280,
		.height = 720,
	},
	{
		.name = "1080p",
		.width = 1920,
		.height = 1080,
	},
	{
		.name = "2160p",
		.alias = "4k",
		.width = 3840,
		.height = 2160,
	},
};

/*
 * The 'optimal resolution' is just a arbitrary resolution that I choose, which
 * will give me support for high performance graphics, I hope =)))
 * NOTE: due to our probing logic, a maximum of 3 resolutions is the max!
 */
const struct named_resolution* optimal_resolution;
const struct named_resolution* optimal_resolution_2;
const struct named_resolution* optimal_resolution_3;

/*
 * Find the active framebuffer on the system
 */
void
init_framebuffer()
{
  size_t handlelist_size;
  EFI_STATUS status;
  EFI_HANDLE dummy_handle = NULL;
  EFI_HANDLE* handle_list = NULL;
  EFI_GUID gop_guid = gEfiGraphicsOutputProtocolGuid;

  size_t handle_count = locate_handle_with_buffer(ByProtocol, gop_guid, &handlelist_size, &handle_list);

  /* No GOP handles??? wtf */
  if (!handle_count)
    return;

  /* We like 480p the best (Since our framebuffer tech is kinda trash) =) */
  optimal_resolution = &resolutions[RES_720P];
  optimal_resolution_2 = &resolutions[RES_480P];
  optimal_resolution_3 = &resolutions[RES_1080P];

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

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* c_info;
  UINTN info_sz;

  /* Resize to the optimal resolution if we can */
  uint32_t best_mode = mode->Mode;

  for (uint32_t current_mode = 0; current_mode < gop->Mode->MaxMode; current_mode++) {
    status = gop->QueryMode(gop, current_mode, &info_sz, &c_info);

    if (c_info->VerticalResolution == optimal_resolution->height && 
        c_info->HorizontalResolution == optimal_resolution->width) {
      best_mode = current_mode;
      break;
    } else if (c_info->VerticalResolution == optimal_resolution_2->height && c_info->HorizontalResolution == optimal_resolution_2->width){
      best_mode = current_mode;
      /* If the third best mode is present AND we have not yet found a better mode, switch */
    } else if (c_info->VerticalResolution == optimal_resolution_3->height && c_info->HorizontalResolution == optimal_resolution_3->width && best_mode == mode->Mode) {
      best_mode = current_mode;
    }
  }

  if (best_mode != mode->Mode) {
    gop->SetMode(gop, best_mode);
  } else {
    /* We can try downsampeling, to get the resolution that is closest to our optimal one */

    for (uint32_t current_mode = 0; current_mode < gop->Mode->MaxMode; current_mode++) {
      status = gop->QueryMode(gop, current_mode, &info_sz, &c_info);

      if (c_info->HorizontalResolution < mode->Info->HorizontalResolution && c_info->HorizontalResolution >= optimal_resolution->width) {
        best_mode = current_mode;
      }
    }

    /* Try to set mode again */
    if (best_mode != mode->Mode) {
      gop->SetMode(gop, best_mode);
    }
  }

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

  gfx->back_fb_pages = EFI_SIZE_TO_PAGES(gfx->height * gfx->width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

  /* Allocate a backbuffer in high memory */
  status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, gfx->back_fb_pages, (EFI_PHYSICAL_ADDRESS*)&gfx->back_fb);

  if (status != EFI_SUCCESS) {

    status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, gfx->back_fb_pages, (EFI_PHYSICAL_ADDRESS*)&gfx->back_fb);

    if (status != EFI_SUCCESS) {
      gfx->back_fb = NULL;
      gfx->back_fb_pages = NULL;
    }
  }

  /* No trailing pixels =) */
  if (gfx->back_fb)
    memset((void*)gfx->back_fb, 0, gfx->back_fb_pages * EFI_PAGE_SIZE);

  /* Make sure to release the gfx again */
}
