
#include "ext_lion_battery.h"

#include "em_adc.h"
#include "em_cmu.h"


void lion_battery_init(void)
{
	CMU_ClockEnable(cmuClock_ADC0, true);

	/* Base the ADC configuration on the default setup. */
	ADC_Init_TypeDef       init  = ADC_INIT_DEFAULT;
	ADC_InitSingle_TypeDef sInit = ADC_INITSINGLE_DEFAULT;

	/* Initialize timebases */
	init.timebase = ADC_TimebaseCalc(0);
	init.prescale = ADC_PrescaleCalc(400000, 0);
	ADC_Init(ADC0, &init);

	/* Set input to temperature sensor. Reference must be 1.25V */
	sInit.reference   = _ADC_SINGLECTRL_REF_2V5;
	sInit.acqTime     = adcAcqTime8;
	sInit.posSel      = adcPosSelAPORT4XCH9;
	ADC_InitSingle(ADC0, &sInit);

}

uint16_t battery_get_millvolts(void)
{
	ADC_Start(ADC0, adcStartSingle);
	while ( ( ADC0->STATUS & ADC_STATUS_SINGLEDV ) == 0 ){}
	uint16_t raw_adc = (uint16_t)ADC_DataSingleGet(ADC0);

	// 2.5V ref, 12bit adc, 100k/100k voltage divider on battery.
	return ((uint32_t)raw_adc * 5000uL) / 4096uL;
}
