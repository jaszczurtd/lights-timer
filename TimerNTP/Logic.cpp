#include <hal/hal.h>
#include "NTPMachine.h"
#include "Logic.h"

void Logic::logicSetup(void) {
#if ENABLE_FAULT_DIAGNOSTICS
  // Initialize retained fault/reset diagnostics before touching watchdog.
  hal_fault_subsystem_init();
#endif
  hal_watchdog_enable(WATCHDOG_TIME, false);

  ntp.start();
  discover.start();

  initialized = true;

  deb("System has started.");
}

void Logic::logicLoop(void) {

  hal_watchdog_feed();

  if(!initialized) {
    return;
  }

#if ENABLE_FAULT_DIAGNOSTICS
  // Keep brown-out heuristic marker fresh after early boot is complete.
  hal_alive_mark();
#endif
#if ENABLE_STACK_GUARD
  // Soft guard: triggers controlled reboot with stack-overflow reset reason.
  hal_stack_guard_check();
#endif

  ntp.stateMachine();
  hal_watchdog_feed();

  discover.handleDiscoveryRequests();
  hal_watchdog_feed();
  hal_debug_loop();
  
  m_delay(CORE_OPERATION_DELAY);  
  hal_idle();
}

