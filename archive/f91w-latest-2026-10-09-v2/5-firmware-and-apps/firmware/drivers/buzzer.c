#include "buzzer.h"
#include "board.h"
#include "hw_if.h"
#include "app_common.h"
#include "app_conf.h"

#if BOARD_HAS_BUZZER
static LPTIM_HandleTypeDef hlptim1;
static uint8_t timer_id;
static const buzzer_note_t *cur;

static void pin_af(int af)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = BOARD_MASK(PIN_BUZZER);
    g.Mode = af ? GPIO_MODE_AF_PP : GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF1_LPTIM1;
    HAL_GPIO_WritePin(BOARD_PORT(PIN_BUZZER), BOARD_MASK(PIN_BUZZER), GPIO_PIN_RESET);
    HAL_GPIO_Init(BOARD_PORT(PIN_BUZZER), &g);
}

static void tone(uint16_t hz)
{
    HAL_LPTIM_PWM_Stop(&hlptim1);
    if (hz == 0) { pin_af(0); return; }       /* Q1 off between notes: no DC through L3 */
    uint32_t period = (32768U + hz / 2) / hz;  /* LSE counts per cycle; 2048 Hz = 16 */
    if (period < 4) period = 4;
    pin_af(1);
    HAL_LPTIM_PWM_Start(&hlptim1, period - 1, period / 2 - 1);
}

static void next(void)
{
    if (!cur || cur->ms == 0) { buzzer_stop(); return; }
    tone(cur->hz);
    uint32_t ticks = (uint32_t)cur->ms * 1000U / CFG_TS_TICK_VAL;
    cur++;
    HW_TS_Start(timer_id, ticks ? ticks : 1);
}

void buzzer_init(void)
{
    RCC_PeriphCLKInitTypeDef clk = {0};
    clk.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
    clk.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&clk);
    __HAL_RCC_LPTIM1_CLK_ENABLE();
    hlptim1.Instance = LPTIM1;
    hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
    hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
    hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
    hlptim1.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
    hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
    hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
    hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
    hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
    HAL_LPTIM_Init(&hlptim1);
    /* runs from the timer server ISR; only touches LPTIM1 and GPIO */
    HW_TS_Create(CFG_TIM_PROC_ID_ISR, &timer_id, hw_ts_SingleShot, next);
}

void buzzer_play(const buzzer_note_t *tune)
{
    HW_TS_Stop(timer_id);
    cur = tune;
    next();
}

void buzzer_stop(void)
{
    HW_TS_Stop(timer_id);
    cur = 0;
    tone(0);
}
#else
void buzzer_init(void) {}
void buzzer_play(const buzzer_note_t *tune) { (void)tune; }
void buzzer_stop(void) {}
#endif

void buzzer_beep(void)
{
    static const buzzer_note_t click[] = {{4096, 20}, {0, 0}};
    buzzer_play(click);
}
