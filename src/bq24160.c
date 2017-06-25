
#include "bq24160.h"

#include "i2cspm.h"


#define I2C_ADDR 0x6Bu

#define BQ24160_ADDR_BATTERY_CHG_VOLTAGE 3u


static void bq24160_set_charge_voltage(void)
{
	I2C_TransferSeq_TypeDef    seq;
	I2C_TransferReturn_TypeDef ret;
	uint8_t regid[1];
	uint8_t data[1] = {0x78}; // Set to 4.1V

	seq.addr  = I2C_ADDR << 1;
	seq.flags = I2C_FLAG_WRITE_WRITE;
	/* Select register to be read */
	regid[0]        = BQ24160_ADDR_BATTERY_CHG_VOLTAGE;
	seq.buf[0].data = regid;
	seq.buf[0].len  = 1;

	seq.buf[1].data = data;
	seq.buf[1].len  = 1;

	ret = I2CSPM_Transfer(I2C0, &seq);
}
void bq24160_init(void)
{
	I2CSPM_Init_TypeDef init = I2CSPM_INIT_DEFAULT;
	init.port = I2C0;
	init.portLocationScl = 6;// PC10 is SCL;// 6 PB12 is SCL.
	init.portLocationSda = 6;// 6 PB11 is SDA
	init.sclPin = 12;
	init.sclPort = gpioPortB;
	init.sdaPin = 11;
	init.sdaPort = gpioPortB;

	I2CSPM_Init(&init);

	bq24160_set_charge_voltage();

}

uint16_t bq24160_read_voltage(void)
{
	I2C_TransferSeq_TypeDef    seq;
	I2C_TransferReturn_TypeDef ret;
	uint8_t regid[1];
	uint8_t data[10];

	seq.addr  = I2C_ADDR << 1;
	seq.flags = I2C_FLAG_WRITE_READ;
	/* Select register to be read */
	regid[0]        = 0u;
	seq.buf[0].data = regid;
	seq.buf[0].len  = 1;


	/* Only 1 byte reg, clear upper 8 bits */

	seq.buf[1].data = data;
	seq.buf[1].len  = 5;

	ret = I2CSPM_Transfer(I2C0, &seq);
	if (ret != i2cTransferDone)
	{
		return(0u);
	}

	return (uint16_t)seq.buf[1].data[0];
}
