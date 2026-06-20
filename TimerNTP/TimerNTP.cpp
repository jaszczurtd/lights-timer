#include "Logic.h"

#include <hal/hal_app.h>

static Logic logic;

extern "C" void app_start(void) {
  logic.logicSetup();
}

extern "C" void app_task0(void) {
  logic.logicLoop();
}
