#include "Logic.h"

void Logic::logicSetup(void) {
  watchdog_enable(WATCHDOG_TIME, false);

  debugInit();
 
  ntp.start(&hardware, &web);
  discover.start(&ntp, &hardware);

  initialized = true;

  deb("System has started.");
}

void Logic::logicLoop(void) {
  watchdog_update();

  if(!initialized) {
    return;
  }

  ntp.stateMachine();
  discover.handleDiscoveryRequests();
  
  m_delay(CORE_OPERATION_DELAY);  
  tight_loop_contents();
}

