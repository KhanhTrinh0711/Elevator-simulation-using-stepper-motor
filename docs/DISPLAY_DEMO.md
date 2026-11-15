# DisplayTask: SSD1306 128x64 countdown

Branch: `feature/task-display`, based on `develop`.

## Files and ownership

- `app/inc/task_display.h`: creation API and countdown timing constants.
- `app/src/task_display.c`: task creation, countdown and bounded error retry.
- `hal/inc/hal_oled.h`: public OLED interface and fixed module configuration.
- `hal/src/hal_oled.c`: controller commands and digit rendering.

Public declarations belong in headers. Font storage, task handle and private
helpers belong in C files under BARR-C 4.2.c / 1.8.a. No shared variables are
defined in headers. HAL names and types keep the existing project exceptions.

SSD1306 command source: [Solomon Systech datasheet, sections 8 and 10 and
charge-pump application note](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf).
This driver is for SSD1306, not SH1106, with an internal charge pump and a module
power-on reset circuit. Address is the unshifted 7-bit value 0x3C.

## Behavior

Shows `10, 9, ..., 1, 0, 10, ...`. Each completed image is held for 1000 ms;
I2C transfer time and scheduling add a small amount to the update interval.
Zero is visible for one period. The driver clears the old digits, including
the tens digit when changing from 10 to 9. Digits are enlarged 4x.

One task owns OLED controller state. Data packets carry at most 16 pixels' column
bytes plus one control byte. The existing BSP I2C mutex is released after each
packet so the encoder can use the bus; do not create another OLED mutex.
There is no full-screen framebuffer on the task stack.

On initialization/draw failure, the task waits 1000 ms, reinitializes and retries
the same count. It does not advance the count for a failed drawing. A missing
OLED does not cause a tight loop, but a disconnected panel cannot display errors.

Uses `APP_DISPLAY_PRIORITY` (1) and `APP_DISPLAY_STACK_WORDS` (256) from app_cfg.h.
The 200 ms application display period is unchanged: this independent countdown
uses its own 1000 ms interval. It does not consume DisplayQueue or drive LEDs.

## Wiring

- PB6 -> SCL; PB7 -> SDA; GND shared.
- Use a module compatible with 3.3 V supply/logic and pull SDA/SCL up to 3.3 V.
- The task waits 100 ms after scheduler start before initializing the panel.

## Running on the board

Per the task-development workflow, production main.c is not edited by this
feature. `make` builds the source but the current main still runs the PC13 blink.
For a dedicated kit experiment or the integration branch, use the following
startup flow instead of the blink loop. Do not create the other motor test tasks
for this isolated display experiment.

```c
#include "main.h"
#include "stm32f1xx_hal.h"
#include "bsp_i2c.h"
#include "task_display.h"
#include "task.h"

int
main (void)
{
    SystemCoreClockUpdate();
    configASSERT(HAL_OK == HAL_Init());
    configASSERT(HAL_OK == BSP_I2C1_Init());
    configASSERT(pdPASS == task_display_create());

    vTaskStartScheduler();
    configASSERT(0);

    for (;;)
    {
        /* Scheduler startup failed; stop for inspection. */
    }
}
```

No IPC queue is required to run this standalone demo. For the final application,
initialize IPC and other tasks before starting the scheduler as usual. Replace
the countdown logic with snapshot consumption when integrating the elevator UI.

Check on hardware: both 10 and 0 are visible; 10->9 has no ghost digit; the count
repeats; disconnected OLED does not starve other tasks; reconnection permits
retry; measure `uxTaskGetStackHighWaterMark` before accepting the stack budget.

## Validation performed during implementation

- Firmware build and strict compilation of the two new C modules.
- Temporary host mocks; no tests/ directory or test target added to the repo.
- OLED mock verifies cleared RAM, distinct 0..10 images, no drawing outside
  the number area, removal of the old tens digit, invalid argument rejection,
  and propagation of errors at every initialization/draw transaction.
- Task mock verifies invalid context/duplicate creation, retry delays, retaining
  the value after errors, and the sequence 10..0 then 10.
- These checks do not establish electrical operation or measured task stack use.
