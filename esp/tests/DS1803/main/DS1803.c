#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
//#include "driver/i2c_master.h"
// https://esp32tutorials.com/

static const char *TAG = "DS1803";


#define I2C_MASTER_SCL_IO  9               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 10               /*!< gpio number for I2C master data  */
#define I2C_MASTER_FREQ_HZ 100000          /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0        /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0        /*!< I2C master doesn't need buffer */
#define SLAVE_ADDRESS 0x28

#define WRITE_BIT I2C_MASTER_WRITE              /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ                /*!< I2C master read */
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */


// #define DS1803_ADDR                   0x28
// #define uint8_t CMD_POT0               0xA9
// #define CMD_POT1                      0xAA
// #define CMD_POT01                     0xAF



int i2c_master_port = 0;
static esp_err_t i2c_master_init(void)
{
  
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static esp_err_t i2c_master_send(uint8_t message[], int len)
{
    ESP_LOGI(TAG, "Sending Message = %s", message);   
    
    esp_err_t ret; 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SLAVE_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, message, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void app_main(void)
{
	const uint8_t  CMD_POT1 = 0xAA;
	const uint8_t  valFF    = 0xFF;
	const uint8_t  val20    = 0x20;
	
    uint8_t  on_command[]  = {CMD_POT1, valFF};
    uint8_t  off_command[] = {CMD_POT1, val20};
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    while(1)
    {
        i2c_master_send(on_command, 2);
        vTaskDelay(3000/ portTICK_PERIOD_MS);
        i2c_master_send(off_command, 2);
        vTaskDelay(3000/ portTICK_PERIOD_MS);  
    }
    
}
