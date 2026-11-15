# RC522 driver and standalone RFID test

Branch: `test/rfid`, created from `origin/develop`.

## Files

- `hal/inc/hal_rc522.h`: UID/status types and public driver API.
- `hal/src/hal_rc522.c`: register access, bounded reset, single-card selection.
- `app/inc/task_rfid_test.h`: task creation and diagnostic snapshot API.
- `app/src/task_rfid_test.c`: polling, error retry, test LEDs and snapshot.

Headers declare public contracts; private state and helpers remain in C files.
Existing HAL/FreeRTOS names and types retain the project's BARR-C exceptions.

## Hardware and supported scope

| STM32 | RC522 module |
|---|---|
| PA5 | SCK |
| PA6 | MISO |
| PA7 | MOSI |
| PB0 | NSS, often labelled SDA/SS on the SPI module |
| PB1 | RST |
| 3.3 V / GND | Supply / common ground |

The BSP releases RST high and initializes SPI before scheduler start. The test
waits 50 ms, then performs a software reset from its task. Only MFRC522 version
0x91/0x92 is accepted. Other versions, including clones, report UNSUPPORTED with
the observed version in the snapshot; verify the module before relaxing this.

The reader supports one ISO14443A card with a 4-, 7- or 10-byte UID. It checks
anticollision BCC, select-response CRC_A, complete byte lengths and cascade bits.
It reports collisions instead of resolving a multi-card field. It does not read
card memory, authenticate Crypto1, support ISO14443B or authorize elevator use.

Sources: [NXP MFRC522 datasheet, sections 8-10](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf)
and [NXP AN10834 card selection](https://www.nxp.com/docs/en/application-note/AN10834.pdf).
The software CRC_A uses preset 0x6363 and reflected polynomial 0x8408; HLTA frame
50 00 has CRC bytes 57 CD. RF timer is approximately 25 ms with a 40 ms software
polling guard. These are not strict transaction deadlines under preemption.

## Expected result

- PB14 green: a complete UID has been read successfully.
- Both LEDs off: no card answered the wake-up request.
- PB15 red: SPI, reader, collision or protocol error.
- Each successful read halts the card; WUPA wakes it on the next poll, so holding
  a card in place permits repeated reads. successful_reads counts polls, not taps.
- A fault waits 1000 ms then reinitializes. Normal polls wait APP_RFID_PERIOD_MS
  (50 ms) after completion. No tight error loop and no HAL_Delay in the task.

The task uses APP_INPUT_PRIORITY and APP_INPUT_STACK_WORDS (3 / 384 words).
This is a temporary stand-in for InputTask, not a sixth production task.
Only this task may use the RC522 multi-frame protocol. The SPI mutex serializes
individual frames, not entire selections. Do not run other LED-writing tasks or
another RFID consumer during this isolated test. No queue or UART is required.

## Running the isolated test

Production main.c is left unchanged according to the independent-task workflow.
The default firmware still runs its existing main. For a local kit experiment
or integration branch, use this startup flow instead of the blink example:

```c
#include "main.h"
#include "stm32f1xx_hal.h"
#include "bsp_spi.h"
#include "task_rfid_test.h"
#include "task.h"

int
main (void)
{
    SystemCoreClockUpdate();
    configASSERT(HAL_OK == HAL_Init());
    configASSERT(HAL_OK == BSP_SPI1_Init());
    configASSERT(pdPASS == task_rfid_test_create());

    vTaskStartScheduler();
    configASSERT(0);

    for (;;)
    {
        /* Scheduler startup failed; stop for inspection. */
    }
}
```

No GPIO motor initialization or motor task is needed. The test configures only
the two LED pins; SPI configures its own pins. No DisplayTask is needed.

## Reading the UID

In another task, call `task_rfid_test_snapshot(&snapshot)` using a local
`task_rfid_snapshot_t snapshot`. Check pdPASS, then status == HAL_RC522_OK before
using uid.bytes[0..uid.length-1]. Bytes are binary, not a NUL-terminated string.
The getter copies under a short critical section so readers see a coherent state.
Only the owning task accesses the RC522 itself.

For a debugger-only test, break at `rfid_publish` in task_rfid_test.c and inspect
`*p_snapshot`, especially status, version, uid.length, uid.bytes and
successful_reads. This avoids calling RTOS APIs from a halted debugger.
No printf/UART backend is silently assumed or added.

## Checks before PR merge

- Compare observed UID with a known card; test card removal and repeated scans.
- Test reader disconnected, unsupported version, SPI errors and multiple cards.
- Confirm green/red meaning, no motor output changes and no CPU starvation.
- Measure task stack high-water mark on the board. Initial stack is not a proof.

Implementation checks use temporary host mocks, not a new tests/ directory:
SPI emulation covers all UID lengths, no card, collision, invalid BCC/CRC,
incorrect response length, timeout and failures at every SPI call in a UID read.
A task mock covers init retry, UID snapshot, LED states, duplicate creation and
bounded delays. A separate ARM link checks the startup example above.
These checks do not replace operation on a physical RC522/card.

## Commit only this feature

The assistant has already created test/rfid; do not create it again. For a future
clean feature the equivalent command is:

```sh
git fetch origin
git switch --no-track -c test/rfid origin/develop
```

Stage only the five files listed for this feature (including this document),
commit, push with `git push -u origin test/rfid`, then open a PR with base develop
and compare test/rfid. Existing CMake/I2C local edits are not part of this change.
