#include "app_main.h"

#include "Adc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"


static TaskHandle_t appMainTaskHandle;
static constexpr UBaseType_t APP_MAIN_TASK_PRIORITY = ( UBaseType_t ) (osPriorityNormal) ;
static constexpr size_t APP_MAIN_TASK_STACK_SIZE_BYTES = 2048;


static void appMainTask(void* arg)
{
    adc_test();
}


extern "C" bool startAppMainTask()
{
    BaseType_t res = xTaskCreate(   appMainTask,
                                    "appMain",
                                    APP_MAIN_TASK_STACK_SIZE_BYTES/4,
                                    NULL,
                                    APP_MAIN_TASK_PRIORITY,
                                    &appMainTaskHandle );
    return res == pdPASS;
}
