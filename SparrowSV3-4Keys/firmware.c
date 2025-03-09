#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "i2c_slave.h"
#include <stdio.h>
#include <stdbool.h>

#define COL1 GPIOv_from_PORT_PIN(GPIO_port_D, 4)
#define COL2 GPIOv_from_PORT_PIN(GPIO_port_D, 5)
#define COL3 GPIOv_from_PORT_PIN(GPIO_port_D, 6)
#define COL4 GPIOv_from_PORT_PIN(GPIO_port_A, 1)

#define I2C_ADDRESS 0x21

#define COLS_SIZE 4

uint16_t COL_PINS[COLS_SIZE] = {COL1, COL2, COL3, COL4};

volatile uint8_t i2c_registers[32] = {0x00};

void on_write(uint8_t reg, uint8_t length)
{
	IWDG->CTLR = CTLR_KEY_Reload;
}

void on_read(uint8_t reg)
{
	IWDG->CTLR = CTLR_KEY_Reload;
}

void setup()
{
	GPIO_port_enable(GPIO_port_A);
	GPIO_port_enable(GPIO_port_C);
	GPIO_port_enable(GPIO_port_D);

	for (int i = 0; i < COLS_SIZE; i++)
	{
		GPIO_pinMode(COL_PINS[i], GPIO_pinMode_I_pullUp, GPIO_Speed_10MHz);
	}

	for (uint8_t i = 0; i < 32; i++)
	{
		i2c_registers[i] = 0x00;
	}

	funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SDA
	funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SCL
	SetupI2CSlave(I2C_ADDRESS, i2c_registers, sizeof(i2c_registers), on_write, on_read, false);

	// Independent Watch Dog Timerを2sのカウンタでセット
	IWDG->CTLR = IWDG_WriteAccess_Enable;
	IWDG->PSCR = IWDG_Prescaler_128;
	IWDG->RLDR = 2000 & 0xfff;
	IWDG->CTLR = CTLR_KEY_Enable;

	Delay_Ms(1);
}

void main_loop()
{
	uint8_t buf = {0};
	for (int c = 0; c < COLS_SIZE; c++)
	{
		uint8_t v = GPIO_digitalRead(COL_PINS[c]);
		if (v == 0)
		{
			buf |= 1 << c;
		}
	}

	i2c_registers[0] = buf;

	// printf("%x \r\n", i2c_registers[0])
	Delay_Ms(1);
}

int main()
{
	SystemInit();
	funGpioInitAll();

	setup();
	// printf("Start!\r\n");

	while (1)
		main_loop();
}
