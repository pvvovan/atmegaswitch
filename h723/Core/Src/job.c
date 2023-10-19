#include "job.h"
#include "main.h"

void job1(void *param)
{
    (void)param;
    for ( ; ; ) {
        HAL_Delay(500);
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
}

void job2(void *param)
{
    (void)param;
    for ( ; ; ) {
        HAL_Delay(250);
        HAL_GPIO_TogglePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin);
    }
}
