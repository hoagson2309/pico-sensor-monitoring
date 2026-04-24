#include "pico/stdlib.h" //timing
#include "hardware/spi.h" //SPI communication
#include "hardware/adc.h" //analog-digital conversion
#include "hardware/i2c.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PIN_SCK 18  //or pico default spi sck pin
#define PIN_MOSI 19 //or pico default spi tx pin
#define PIN_MISO 16 //or pico default spi rx pin
#define PIN_CS 17 //or pico default spi csn pin (io expander)
#define BUTTON_PIN 15

#define SDA 4  //send n receive data in I2C communication
#define SCL 5   // clock signal in I2C.
#define OS 6    //interrupt output from the LM75B temperature sensor
#define INT 7   //interrupt output from the APDS-9306 light sensor

//Address
#define TEMP_ADDR 0x49  
#define LIGHT_ADDR 0x52
#define MCP_ADDR 0x40

//temp reg list
#define TEMP 0x00
#define CONF 0x01
#define THYST 0x02 //lower threshold
#define TOS 0x03 //upper threshold

//upper threshold
#define TEMP_THRESHOLD 27

//light reg list
#define MAIN_CTRL        0x00
#define ALS_MEAS_RATE    0x04
#define ALS_GAIN         0x05
#define MAIN_STATUS      0x07
#define ALS_DATA_0       0x0D
#define INT_CFG          0x19
#define INT_PERSIST      0x1A
#define ALS_THRES_UP_0   0x21
#define ALS_THRES_UP_1   0x22
#define ALS_THRES_UP_2   0x23
#define ALS_THRES_LOW_0  0x24
#define ALS_THRES_LOW_1  0x25
#define ALS_THRES_LOW_2  0x26

static volatile bool mode_switched = true;
static volatile bool hot = false;
static volatile bool light_alert = false;


// CS functions for MCP23S17 (io expander)
static inline void cs_select()
{
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 0);  //pulling low - active low (enable the devices)
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect()
{
    asm volatile("nop \n nop \n nop");
    gpio_put(PIN_CS, 1); //pulling CS HIGH (disables the device)
    asm volatile("nop \n nop \n nop");
}

//Write byte to MCP
void io_exp_write(uint8_t reg, uint8_t data)
{
    uint8_t buf[3] = {MCP_ADDR, reg, data}; // SPI write opcode
    cs_select(); 
    spi_write_blocking(spi0, buf, 3); //This sends the 3-byte array through the SPI0 peripheral to the MCP23S08 device
    cs_deselect();
    sleep_ms(10);
} //Ex:
//0x40 => Control byte (write command)
//0x09 => Register address (GPIO)
//0xFF => Data to write (set all pins high)

static void flash_warning(void) {
    for (int i = 0; i < 3; ++i) {
        io_exp_write(0x09, 0xFF); // all ON
        sleep_ms(120);
        io_exp_write(0x09, 0x00); // all OFF
        sleep_ms(120);
    }
}

//2 bytes, use only the 11 MSBs
float read_temp(void){
    uint8_t reg = TEMP; //Temperature register address
    uint8_t buf[2];     //8-bit
    i2c_write_blocking(i2c_default, TEMP_ADDR, &reg, 1, true); //Write 1 byte (0x00) to tell the LM75B we want to read from its temperature register
    i2c_read_blocking(i2c_default, TEMP_ADDR, buf, 2, false); //Read 2 bytes of data (buf[0] = MSB, buf[1] = LSB)

    int16_t raw = ((int16_t)buf[0] << 8) | buf[1]; //Combine the two bytes into one 16-bit integer (MSB + LSB)
    raw >>= 5;                                     //11-bit valid temperature data. (D15 - D5 = tempurature)
    if(raw & 0x0400) raw |= 0xF800;     //check the sign bit last bit (D10) if it's negative (= 1)
    return raw * 0.125f;                //Tempurature resolution (convert raw integer into Celcius)
}       


void temp_set_thresholds(){
    uint16_t raw = (uint16_t)(TEMP_THRESHOLD / 0.5) << 7; //first 7-bit are unused, 0.5C = 1 bit in register

    uint8_t thyst[3] = {THYST, (raw >> 8) & 0xFF, raw & 0xFF}; //MSB + LSB 
    i2c_write_blocking(i2c_default, TEMP_ADDR, thyst, 3, false);    //0xFF to get 8-bit

    uint8_t tos[3] = {TOS, (raw >> 8) & 0xFF, raw & 0xFF};
    i2c_write_blocking(i2c_default, TEMP_ADDR, tos, 3, false);
}


uint32_t read_light(void){
    uint8_t reg = ALS_DATA_0; 
    uint8_t buf[3];  //0x0D[7:0], 0x0E[15:8], 0x0F[19:16]
    i2c_write_blocking(i2c_default, LIGHT_ADDR, &reg, 1, true); //sets the internal pointer to register 0x0D, true => no stop condition so bus stay open for the next read command
    i2c_read_blocking(i2c_default, LIGHT_ADDR, buf, 3, false); //read and save to buf

    int32_t raw = ((uint32_t)(buf[2] & 0x0F) << 16) | ((uint32_t)buf[1] << 8) | (uint32_t)buf[0]; //0x0F 4bit cuz top 4bit are unused
    return raw; //20-bit
}
//[19:16] | [15:8] | [7:0]
//0b0000LLLL_LLLLLLLL_LLLLLLLL

void amb_light_init_interrupt(void){
    //EN ALS engine
    uint8_t cmd1[2] = {MAIN_CTRL, 0x02};
    i2c_write_blocking(i2c_default, LIGHT_ADDR, cmd1, 2, false);

    // ALS bit width: 20-bit 400ms integration time, 500ms mesurement rate => 00000100 FOR HIGH RESOLUTION
    uint8_t cmd2[2] = {ALS_MEAS_RATE, 0x04};
    i2c_write_blocking(i2c_default, LIGHT_ADDR, cmd2, 2, false);

    // Gain = 3 (0x01)
    uint8_t cmd3[2] = {ALS_GAIN, 0x01};
    i2c_write_blocking(i2c_default, LIGHT_ADDR, cmd3, 2, false);

    // lower and upper thresholds (20-bit values)
    uint32_t low = 100;  // test value
    uint32_t high = 16000; 

    uint8_t low_buf[4] = {ALS_THRES_LOW_0, 
        (uint8_t)(low & 0xFF),  //0xFF 8-bit, LSB
        (uint8_t)((low >> 8) & 0xFF), //intervening byte
        (uint8_t)((low >> 16) & 0x0F)}; //0x0F 4bit, MSB
    i2c_write_blocking(i2c_default, LIGHT_ADDR, low_buf, 4, false);

    uint8_t high_buf[4] = {ALS_THRES_UP_0, 
        (uint8_t)(high & 0xFF),
        (uint8_t)((high >> 8) & 0xFF),
        (uint8_t)((high >> 16) & 0x0F)};
    i2c_write_blocking(i2c_default, LIGHT_ADDR, high_buf, 4, false);

     // interrupt persistence = trigger after 2 consecutive threshold events (00010000)
    uint8_t persist[2] = {INT_PERSIST, 0x10}; 
    i2c_write_blocking(i2c_default, LIGHT_ADDR, persist, 2, false);

    //   Configure interrupt: ALS source, threshold mode, active low
    uint8_t int_cfg[2] = {INT_CFG, 0x14}; //00010100 enable interrupt
    i2c_write_blocking(i2c_default, LIGHT_ADDR, int_cfg, 2, false);

//clear any pending interrupt flag (old interrupts => Makes sure INT pin starts high)  
    uint8_t reg = MAIN_STATUS;  //(read reg to clear the flag) 
    uint8_t st;
    i2c_write_blocking(i2c_default, LIGHT_ADDR, &reg, 1, true); 
    i2c_read_blocking(i2c_default, LIGHT_ADDR, &st, 1, false);

}

void gpio_callback(uint gpio, uint32_t events){
    static uint64_t last_us = 0; //last time the button was pressed
    uint64_t now = time_us_64(); //current time in microseconds since boot (measure how much time has passed since the last button press)
    if(gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)){
        if(now - last_us > 200000){ //200ms debouncing (ignore fast repeated presses)
            mode_switched = !mode_switched;
            last_us = now; //update timestamp
        }// Detect that the button was pressed, prevent button bounce from causing multiple triggers, and
    // Toggle the display mode (switch between temperature and light display).
    } else if (gpio == OS && (events & GPIO_IRQ_EDGE_FALL)){
        hot = true;
    } else if (gpio == INT && (events & GPIO_IRQ_EDGE_FALL)) {
        light_alert = true;
    }
}


int main()
{
    stdio_init_all();
    sleep_ms(1000); // Wait for USB serial

    // SPI init
    spi_init(spi0, 500 * 1000); // 1 MHz
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // I2C setup (SDA=GP4, SCL=GP5)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SDA);
    gpio_pull_up(SCL);

    // CS pins
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    //OS setup
    gpio_init(OS);
    gpio_set_dir(OS, GPIO_IN);
    gpio_pull_up(OS);

    //INT setup
    gpio_init(INT);
    gpio_set_dir(INT, GPIO_IN);
    gpio_pull_up(INT);

    // Set MCP23S17 port A as output -> drives LEDs
    io_exp_write(0x00, 0x00); // IODIRA = 0 (all outputs)

    //Dont need to turn on temp sensor cuz it starts measuring temperature as soon as powered

    //sensors threshold setups
    temp_set_thresholds(); // Both thresholds at 28 degree for immediate re-trigger
    amb_light_init_interrupt();

    // Button config
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_set_irq_enabled(OS, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(INT, GPIO_IRQ_EDGE_FALL, true);

    printf("System ready. Mode: LIGHT (press button to toggle)\n");


    while (true) {
        if (mode_switched) {
            float temp = read_temp();
            if(temp < TEMP_THRESHOLD) hot = false;
            if(hot){
                printf("WARNING!!! HIGH TEMPURATURE.\n");
                flash_warning();
            } else {
            printf("[TEMP]  %.3f °C\n", temp);
            io_exp_write(0x09, (uint8_t)temp); //0x09 => GPIO pin address
            }
        } else {
            uint32_t light = read_light();
            printf("[LIGHT] Raw: %lu\n", (unsigned long)light);
            io_exp_write(0x09, (uint8_t)light); // display LSB on LEDs
        } if (light_alert) {
            light_alert = false; // reset flag
            printf("WARNING!!! Ambient light interrupt triggered!\n");
            flash_warning(); // blink LEDs

            // Clear the interrupt flag by reading MAIN_STATUS (Clear active interrupt =>Acknowledges event and re-arms the sensor)
            uint8_t reg = MAIN_STATUS;
            uint8_t st;
            i2c_write_blocking(i2c_default, LIGHT_ADDR, &reg, 1, true);
            i2c_read_blocking(i2c_default, LIGHT_ADDR, &st, 1, false);
            printf("MAIN_STATUS = 0x%02X (INT cleared)\n", st); //=> getting 0x00 format
        }
        sleep_ms(500);
    }
}






//Bit 10 (since 11 bits total) is the sign bit in two’s complement form
//0xF800 in binary is 1111 1000 0000 0000
//The |= operator means "bitwise OR and assign".
//This operation sets the upper 5 bits (bits 11–15) to 1.
//Before: 0000 0011 1111 1000
//After : 1111 1111 1111 1000
//so that a negative 11-bit number from the LM75B becomes a negative 16-bit number that C understands correctly.
// Without doing this, the value would be interpreted as a large positive number instead of a negative one. 