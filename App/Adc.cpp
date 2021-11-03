#include "Adc.h"

#include "adc.h"
#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>

//constexpr cannot use reinterpret_cast, so a macro has to be used
//stm32l152rc.pdf (DocID022799 Rev 13) page 59, table 16, 0x1FF8 0078-0x1FF8 0079
#define P_VREFINT_CAL reinterpret_cast<const uint16_t*>(VREFINT_CAL_ADDR_CMSIS)
//stm32l152rc.pdf (DocID022799 Rev 13) page 107, table 61,
#define P_TEMPSENSOR_CAL1 reinterpret_cast<const uint16_t*>(TEMPSENSOR_CAL1_ADDR_CMSIS)
#define P_TEMPSENSOR_CAL2 reinterpret_cast<const uint16_t*>(TEMPSENSOR_CAL2_ADDR_CMSIS)

/*
FULL_SCALE = 2^12-1=4095

V DDA = 3 V x VREFINT_CAL / VREFINT_DATA
V CHANNELx = V DDA/FULL_SCALE * ADC_DATA x
Temperature = (110c-30c)/(ts_cal2-ts_cal1)* ( TS_DATA – TS_CAL1 ) + 30°C
*/


void adc_test()
{
   ADC_HandleTypeDef* pAdcHandle = &hadc;
   HAL_StatusTypeDef status;

   //todo: look up number of cycles from datasheet
   //T S_temp = 10us
   //F_ADC=HSI16=16MHz,prescaler=1
   //10us*16MHz=160
   //ADC_SAMPLETIME_192CYCLES > 160

   ADC_ChannelConfTypeDef configVref = {ADC_CHANNEL_VREFINT, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_48CYCLES};
   ADC_ChannelConfTypeDef configTemperature = {ADC_CHANNEL_TEMPSENSOR, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_192CYCLES};
   ADC_ChannelConfTypeDef configIn1 = {ADC_CHANNEL_1, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_48CYCLES};

   int source = 0;
   status = HAL_ADC_ConfigChannel(pAdcHandle, &configVref);

   uint16_t adcValue;

   const uint32_t VREFINT_CAL_FP = uint32_t(*P_VREFINT_CAL) << 13;

   uint16_t VREFINT_DATA;
   uint16_t in1AdcValue;
   uint16_t temperatureAdcValue;

   uint32_t vrefFP;
   uint16_t vddAMilliVolt;


   const uint32_t temperatureScaler = ((110-30)<<13)/(*P_TEMPSENSOR_CAL2-*P_TEMPSENSOR_CAL1);
   uint32_t temperatureDegreeC_FP;

   uint32_t temperatureDegreeC_FP_Filtered = 0;
   bool filterInitialized = false;


   TickType_t xLastWakeTime;
   const TickType_t xPeriod = 100 / portTICK_PERIOD_MS;

   // Initialise the xLastWakeTime variable with the current time.
   xLastWakeTime = xTaskGetTickCount();


   while(1)
   {
        status = HAL_ADC_Start(pAdcHandle);

        do{
          vTaskDelay( 1 / portTICK_PERIOD_MS );
          status = HAL_ADC_PollForConversion(pAdcHandle,1000);

        }while( (status == HAL_BUSY) || (status == HAL_TIMEOUT));

        HAL_ADC_Stop(pAdcHandle);

        adcValue = HAL_ADC_GetValue(pAdcHandle);

        switch(source)
        {
            case 0:
            {
                VREFINT_DATA = adcValue;
                source = 1;
                status = HAL_ADC_ConfigChannel(pAdcHandle, &configIn1);
            }
            break;
            case 1:
            {
                in1AdcValue = adcValue;
                source = 2;
                status = HAL_ADC_ConfigChannel(pAdcHandle, &configTemperature);
            }
            break;
            case 2:
            {
                temperatureAdcValue = adcValue;

                //calculate all values:

                vrefFP = VREFINT_CAL_FP / VREFINT_DATA;

                vddAMilliVolt = (vrefFP * 3000) >> 13;
                printf("vddA: %d mV\n",vddAMilliVolt);

                printf("in1AdcValue: %d\n",in1AdcValue);

                printf("in1AdcValue: %d mv\n", vddAMilliVolt * in1AdcValue >> 12  ) ;

                //Temperature = (110c-30c)/(ts_cal2-ts_cal1)* ( TS_DATA – TS_CAL1 ) + 30°C
                //temperatureDegreeC = (110.0f-30)/(*P_TEMPSENSOR_CAL2-*P_TEMPSENSOR_CAL1)*(temperatureAdcValue-*P_TEMPSENSOR_CAL1) + 30;

                uint32_t compensated_adc_value = (temperatureAdcValue * vrefFP) >> 13;

                //temperatureDegreeC = ((compensated_adc_value-*P_TEMPSENSOR_CAL1)*temperatureScaler >> 13) + 30;
                //printf("temp: %f 'C\n",temperatureDegreeC);


                temperatureDegreeC_FP = ((compensated_adc_value-*P_TEMPSENSOR_CAL1)*temperatureScaler) + (30<<13);

                //printf("temp: %.1f 'C\n",float(temperatureDegreeC_FP)/(1<<13));


                if(filterInitialized)
                {
                    constexpr uint32_t k = (1<<13)*0.03561822695; // first order IIR 0.1Hz @10Hz
                    temperatureDegreeC_FP_Filtered = (temperatureDegreeC_FP_Filtered * ((1<<13)-k) + temperatureDegreeC_FP * k) >> 13;
                }
                else
                {
                    temperatureDegreeC_FP_Filtered = temperatureDegreeC_FP;
                    filterInitialized= true;
                }

                printf("temp: %.1f 'C\n",float(temperatureDegreeC_FP_Filtered)/(1<<13));

                source = 0;
                status = HAL_ADC_ConfigChannel(pAdcHandle, &configVref);

                vTaskDelayUntil( &xLastWakeTime, xPeriod );
            }
            break;
        }
   }

}
