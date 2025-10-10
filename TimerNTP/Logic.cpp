#include "NTPMachine.h"
#include "Logic.h"

void Logic::logicSetup(void) {
  watchdog_enable(WATCHDOG_TIME, false);

  debugInit();
 
  ntp.start();
  discover.start();

  initialized = true;

  deb("System has started.");
}

void Logic::logicLoop(void) {

  watchdog_update();

  if(!initialized) {
    return;
  }

  ntp.stateMachine();
  watchdog_update();

  discover.handleDiscoveryRequests();
  watchdog_update();
  
  m_delay(CORE_OPERATION_DELAY);  
  tight_loop_contents();
}

