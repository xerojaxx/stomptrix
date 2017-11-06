
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

uint8_t battery_get_percentage(void)
{
	typedef struct{
		uint16_t voltage_mv;
		uint8_t percent;
	}batt_percent_lookup_t;

	static batt_percent_lookup_t batt_lookup[] =
	{
			{4100u, 100u},
			{4000u, 90u},
			{3900u, 80u},
			{3800u, 70u},
			{3700u, 60u},
			{3600u, 50u},
			{3500u, 40u},
			{3400u, 30u},
			{3300u, 20u},
			{3200u, 10u}
	};
	uint16_t batt_mv = battery_get_millvolts();

	for(uint8_t i=0u; i<(sizeof(batt_lookup)/sizeof(batt_percent_lookup_t)); i++)
	{
		if( batt_mv > batt_lookup[i].voltage_mv)
		{
			return batt_lookup[i].percent;
		}
	}
	return 0u;
}
