#include "battery.h"
#include "stm32wbxx_hal.h"
#include "stm32wbxx_ll_adc.h"

uint16_t battery_mv(void)
{
    ADC_HandleTypeDef h = {0};
    ADC_ChannelConfTypeDef ch = {0};
    uint32_t raw = 0;

    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_SYSCLK);
    h.Instance = ADC1;
    h.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
    h.Init.Resolution = ADC_RESOLUTION_12B;
    h.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    h.Init.ScanConvMode = ADC_SCAN_DISABLE;
    h.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    h.Init.NbrOfConversion = 1;
    h.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    h.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    if (HAL_ADC_Init(&h) == HAL_OK) {
        ch.Channel = ADC_CHANNEL_VREFINT;
        ch.Rank = ADC_REGULAR_RANK_1;
        ch.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;  /* VREFINT needs >= 4 us */
        ch.SingleDiff = ADC_SINGLE_ENDED;
        ch.OffsetNumber = ADC_OFFSET_NONE;
        HAL_ADC_ConfigChannel(&h, &ch);
        HAL_ADCEx_Calibration_Start(&h, ADC_SINGLE_ENDED);
        HAL_ADC_Start(&h);
        if (HAL_ADC_PollForConversion(&h, 5) == HAL_OK) raw = HAL_ADC_GetValue(&h);
        HAL_ADC_Stop(&h);
        HAL_ADC_DeInit(&h);
    }
    __HAL_RCC_ADC_CLK_DISABLE();
    if (raw == 0) return 0;
    return (uint16_t)__LL_ADC_CALC_VREFANALOG_VOLTAGE(raw, LL_ADC_RESOLUTION_12B);
}

uint8_t battery_percent(uint16_t mv)
{
    if (mv >= 3000) return 100;
    if (mv <= 2500) return 0;
    return (uint8_t)((mv - 2500U) / 5U);
}
