#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"
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
#define BASE_REGISTER_ADDRESS 0x00

#define COLS_SIZE 7
#define ROWS_SIZE 5

uint16_t COL_PINS[COLS_SIZE] = {COL1, COL2, COL3, COL4, COL5, COL6, COL7};
uint16_t ROW_PINS[ROWS_SIZE] = {ROW1, ROW2, ROW3, ROW4, ROW5};

volatile uint8_t i2c_registers[32] = {0x00};

typedef struct
{
	uint8_t reg;
	uint8_t length;
} I2CMosiEvent;

void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void) __attribute__((interrupt));

void init_i2c_slave(uint8_t address)
{
	// https://github.com/cnlohr/ch32v003fun/blob/master/examples/i2c_slave/i2c_slave.h
	RCC->CFGR0 &= ~(0x1F << 11);

	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO;
	RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

	// PC1 is SDA, 10MHz Output, alt func, open-drain
	GPIOC->CFGLR &= ~(0xf << (4 * 1));
	GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 1);

	// PC2 is SCL, 10MHz Output, alt func, open-drain
	GPIOC->CFGLR &= ~(0xf << (4 * 2));
	GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 2);

	// Reset I2C1 to init all regs
	RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
	RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

	I2C1->CTLR1 |= I2C_CTLR1_SWRST;
	I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;

	// Set module clock frequency
	uint32_t prerate = 2000000; // I2C Logic clock rate, must be higher than the bus clock rate
	I2C1->CTLR2 |= (FUNCONF_SYSTEM_CORE_CLOCK / prerate) & I2C_CTLR2_FREQ;

	// Enable interrupts
	I2C1->CTLR2 |= I2C_CTLR2_ITBUFEN;
	I2C1->CTLR2 |= I2C_CTLR2_ITEVTEN; // Event interrupt
	I2C1->CTLR2 |= I2C_CTLR2_ITERREN; // Error interrupt

	NVIC_EnableIRQ(I2C1_EV_IRQn); // Event interrupt
	NVIC_SetPriority(I2C1_EV_IRQn, 2 << 4);
	NVIC_EnableIRQ(I2C1_ER_IRQn); // Error interrupt
	// Set clock configuration
	uint32_t clockrate = 1000000;																	 // I2C Bus clock rate, must be lower than the logic clock rate
	I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK / (3 * clockrate)) & I2C_CKCFGR_CCR) | I2C_CKCFGR_FS; // Fast mode 33% duty cycle
	// I2C1->CKCFGR = ((APB_CLOCK/(25*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_DUTY | I2C_CKCFGR_FS; // Fast mode 36% duty cycle
	// I2C1->CKCFGR = (APB_CLOCK/(2*clockrate))&I2C_CKCFGR_CCR; // Standard mode good to 100kHz

	// Set I2C address
	I2C1->OADDR1 = address << 1;

	// Enable I2C
	I2C1->CTLR1 |= I2C_CTLR1_PE;

	// Acknowledge the first address match event when it happens
	I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

uint16_t i2c_scan_start_position = 0;
uint16_t i2c_scan_length = 0;
bool i2c_firse_byte = true;
bool i2c_mosi_event = false;
bool on_mosi_event = false;
I2CMosiEvent latest_mosi_event = {0, 0};
bool address2matched = false;

void I2C1_EV_IRQHandler(void)
{
	uint16_t STAR1, STAR2 __attribute__((unused));
	STAR1 = I2C1->STAR1;
	STAR2 = I2C1->STAR2;

	// printf("EV STAR1: 0x%04x STAR2: 0x%04x\r\n", STAR1, STAR2);

	I2C1->CTLR1 |= I2C_CTLR1_ACK;

	if (STAR1 & I2C_STAR1_ADDR) // 0x0002
	{
		// 最初のイベント
		// read でも write でも必ず最初に呼ばれる
		address2matched = !!(STAR2 & I2C_STAR2_DUALF);
		// printf("ADDR %d\r\n", address2matched);
		i2c_firse_byte = true;
		i2c_mosi_event = false;
		i2c_scan_start_position = 0;
		i2c_scan_length = 0;
	}

	if (STAR1 & I2C_STAR1_RXNE) // 0x0040
	{
		if (i2c_firse_byte)
		{
			i2c_scan_start_position = I2C1->DATAR;
			i2c_scan_length = 0;
			i2c_firse_byte = false;
			i2c_mosi_event = true;
			// printf("RXNE write event: first_write 0x%x\r\n", i2c_scan_start_position);
		}
		else
		{
			uint16_t pos = i2c_scan_start_position + i2c_scan_length;
			uint8_t v = I2C1->DATAR;
			// printf("RXNE write event: pos:0x%x v:0x%x\r\n", pos, v);
			if (pos < 32)
			{
				i2c_registers[pos] = v;
				i2c_scan_length++;
			}
		}
	}

	if (STAR1 & I2C_STAR1_TXE) // 0x0080
	{
		if (i2c_firse_byte)
		{
			i2c_scan_start_position = I2C1->DATAR;
			i2c_scan_length = 0;
			i2c_firse_byte = false;
			i2c_mosi_event = true;
			// printf("TXE write event: first_write:%x\r\n", i2c_scan_start_position);
		}
		else
		{
			// 1byte の read イベント（slave -> master）
			// 1byte 送信
			uint16_t pos = i2c_scan_start_position + i2c_scan_length;
			uint8_t v = 0x00;
			if (pos < 32)
			{
				v = i2c_registers[pos];
			}
			// printf("TXE write event: pos:0x%x v:0x%x\r\n", pos, v);
			I2C1->DATAR = v;
			i2c_scan_length++;

			// 送信の度にWatch Dog Timerをリセット
			IWDG->CTLR = CTLR_KEY_Reload;
		}
	}

	if (STAR1 & I2C_STAR1_STOPF)
	{
		I2C1->CTLR1 &= ~(I2C_CTLR1_STOP);
		if (i2c_mosi_event)
		{
			// printf("on_mosi_event\r\n");
			latest_mosi_event.reg = i2c_scan_start_position;
			latest_mosi_event.length = i2c_scan_length;
			on_mosi_event = true;
		}
	}
}

void I2C1_ER_IRQHandler(void)
{
	uint16_t STAR1 = I2C1->STAR1;

	// printf("ER STAR1: 0x%04x\r\n", STAR1);

	if (STAR1 & I2C_STAR1_BERR)			  // 0x0100
	{									  // Bus error
		I2C1->STAR1 &= ~(I2C_STAR1_BERR); // Clear error
	}

	if (STAR1 & I2C_STAR1_ARLO)			  // 0x0200
	{									  // Arbitration lost error
		I2C1->STAR1 &= ~(I2C_STAR1_ARLO); // Clear error
	}

	if (STAR1 & I2C_STAR1_AF)			// 0x0400
	{									// Acknowledge failure
		I2C1->STAR1 &= ~(I2C_STAR1_AF); // Clear error
	}
}

void print_bits(uint8_t value)
{
	for (int i = 7; i >= 0; i--)
	{
		printf("%d", (value >> i) & 1);
	}
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

	init_i2c_slave(I2C_ADDRESS);

	for (uint8_t i = 0; i < 32; i++)
	{
		i2c_registers[i] = 0x00;
	}

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
		Delay_Ms(1);

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
