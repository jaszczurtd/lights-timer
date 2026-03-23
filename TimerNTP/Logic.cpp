#include <hal/hal.h>
#include "NTPMachine.h"
#include "Logic.h"

void Logic::logicSetup(void) {
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

  ntp.stateMachine();
  hal_watchdog_feed();

  discover.handleDiscoveryRequests();
  hal_watchdog_feed();
  
  m_delay(CORE_OPERATION_DELAY);  
  hal_idle();
}

