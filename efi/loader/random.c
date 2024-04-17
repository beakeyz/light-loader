#include "efiapi.h"
#include "efierr.h"
#include "efilib.h"
#include "efiprot.h"
#include "heap.h"
#include <random.h>
#include <stdio.h>

static EFI_HANDLE* rng_handles;
static EFI_RNG_PROTOCOL* rng_prot;

int 
init_efi_random()
{
  uint32_t count;
  size_t size;
  EFI_GUID guid = EFI_RNG_PROTOCOL_GUID;

  rng_handles = NULL;
  rng_prot = NULL;

  count = locate_handle_with_buffer(ByProtocol, guid, &size, &rng_handles);

  if (!count)
    return BS->LocateProtocol(&guid, NULL, (void**)&rng_prot);

  for (uint32_t i = 0; i < count; i++) {

    if (open_protocol(rng_handles[i], &guid, (void**)&rng_prot) == EFI_SUCCESS)
      break;
  }

  heap_free(rng_handles);
  return (rng_prot == nullptr ? -1 : 0);
}

static int
_fucked_random(uint32_t len, uint8_t* buf)
{
  static uintptr_t call_offset = 0;
  EFI_TIME time;
  uintptr_t seed;

  for (uint32_t i = 0; i < len; i++) {

    if (RT->GetTime(&time, NULL) != EFI_SUCCESS)
      return -1;

    seed = (uintptr_t)(time.Nanosecond | time.Second) ^ time.Day;

    buf[i] = (((call_offset ^ 669382104) + (seed) ^ 0xff3323) * 0x6939) % 255;

    call_offset += i;
  }

  return 0;
}

EFI_STATUS 
get_efi_random(uint32_t len, uint8_t* buf)
{
  if (!rng_prot)
    return _fucked_random(len, buf);

  return rng_prot->GetRNG(rng_prot, NULL, len, buf);
}
