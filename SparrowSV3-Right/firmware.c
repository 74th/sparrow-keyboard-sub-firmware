#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include "i2c_slave.h"
#include <stdio.h>
#include <stdbool.h>

#define COL1 GPIOv_from_PORT_PIN(GPIO_port_D, 0)
#define COL2 GPIOv_from_PORT_PIN(GPIO_port_D, 3)
#define COL3 GPIOv_from_PORT_PIN(GPIO_port_D, 2)
#define COL4 GPIOv_from_PORT_PIN(GPIO_port_C, 7)
#define COL5 GPIOv_from_PORT_PIN(GPIO_port_D, 4)
#define COL6 GPIOv_from_PORT_PIN(GPIO_port_D, 5)
#define COL7 GPIOv_from_PORT_PIN(GPIO_port_D, 6)

#define ROW1 GPIOv_from_PORT_PIN(GPIO_port_C, 3)
#define ROW2 GPIOv_from_PORT_PIN(GPIO_port_C, 4)
#define ROW3 GPIOv_from_PORT_PIN(GPIO_port_C, 5)
#define ROW4 GPIOv_from_PORT_PIN(GPIO_port_A, 1)
#define ROW5 GPIOv_from_PORT_PIN(GPIO_port_C, 6)

#define I2C_ADDRESS 0x20

#define COLS_SIZE 7
#define ROWS_SIZE 5

uint16_t COL_PINS[COLS_SIZE] = {COL1, COL2, COL3, COL4, COL5, COL6, COL7};
uint16_t ROW_PINS[ROWS_SIZE] = {ROW1, ROW2, ROW3, ROW4, ROW5};

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
		GPIO_pinMode(COL_PINS[i], GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
	}

	for (int i = 0; i < ROWS_SIZE; i++)
	{
		GPIO_pinMode(ROW_PINS[i], GPIO_pinMode_I_pullDown, GPIO_Speed_10MHz);
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
	uint8_t buf[ROWS_SIZE] = {0};
	for (int c = 0; c < COLS_SIZE; c++)
	{
		for (int i = 0; i < COLS_SIZE; i++)
		{
			if (i == c)
			{
				GPIO_digitalWrite_1(COL_PINS[i]);
			}
			else
			{
				GPIO_digitalWrite_0(COL_PINS[i]);
			}
		}
		Delay_Us(10);

		uint8_t value = 0;
		for (int r = 0; r < ROWS_SIZE; r++)
		{
			uint8_t v = GPIO_digitalRead(ROW_PINS[r]);
			buf[r] |= v << c;
		}
	}

	memcpy(i2c_registers, buf, ROWS_SIZE);

	// printf("%x %x %x %x %x\r\n", i2c_registers[0], i2c_registers[1], i2c_registers[2], i2c_registers[3], i2c_registers[4]);
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
