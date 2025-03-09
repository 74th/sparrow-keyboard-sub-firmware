#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_ADDRESS 0x20
#define BASE_REGISTER_ADDRESS 0x00

#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

#define ROWS_NUM 5

#define SLEEP_MS 100
#define LOOP_MS 300

void print_bits(uint8_t v)
{
    printf("0b");
    for (int i = 0; i < 8; i++)
    {
        printf("%d", (v >> (7 - i)) & 1);
    }
}

void test_read()
{
    // printf("test read\r\n");
    uint8_t buf[1] = {BASE_REGISTER_ADDRESS};
    uint8_t rbuf[ROWS_NUM] = {1, 2, 3, 4, 5};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 1, false);
    i2c_read_blocking(I2C_PORT, I2C_ADDRESS, rbuf, ROWS_NUM, false);

    // for (int i = 0; i < ROWS_NUM; i++)
    // {
    //     printf("COL%d: ", i);
    //     print_bits(rbuf[i]);
    //     printf("\r\n");
    // }
    printf("%x %x %x %x %x\r\n", rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4]);
}

void init()
{
    stdio_init_all();

    printf("init start\r\n");

    i2c_init(I2C_PORT, 100 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

int main()
{
    init();
    int i = 0;

    while (true)
    {
        i++;
        printf("%8d:", i);
        test_read();
        sleep_ms(SLEEP_MS);
    }
}
