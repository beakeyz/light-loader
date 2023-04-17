#include "timer.h"
#include "drivers/display/framebuffer.h"
#include "frontend/loading_screen.h"
#include "lib/light_mainlib.h"

EFI_EVENT timer_event;
static void timer_callback(EFI_EVENT event, void* context);

LIGHT_STATUS init_periodic_timer() {

  EFI_STATUS status;
  g_light_info.system_ticks = 0;

  status = g_light_info.boot_services->CreateEvent(EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, timer_callback, NULL, &timer_event);

  if (status != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  status = g_light_info.boot_services->SetTimer(timer_event, TimerPeriodic, 0);

  if (status != EFI_SUCCESS) {
    return LIGHT_FAIL;
  }

  return LIGHT_SUCCESS;
}

void timer_callback(EFI_EVENT event, void* context) {
  g_light_info.system_ticks++;
  //light_log(L"Tick!");
}
