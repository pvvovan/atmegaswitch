#include "job.h"
#include "main.h"


void job0(void *pvParameters)
{
	(void)pvParameters;
	for ( ; ; ) {
		HAL_Delay(250);
		HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
	}
}

void job1(void *pvParameters)
{
	(void)pvParameters;
	for ( ; ; ) {
		HAL_Delay(500);
		HAL_GPIO_TogglePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin);
	}
}

void job2(void *pvParameters)
{
	(void)pvParameters;
	for ( ; ; ) {
		HAL_Delay(1000);
		HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
	}
}
