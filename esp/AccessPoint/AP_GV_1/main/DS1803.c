// esp32c3 esp-idf v 5.5.1
//
// DS1803.c
//
#include "DS1803.h"
#include "config.h"

static const char *TAG = "DS1803";

#define I2C_MASTER_SCL_IO  9               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 10               /*!< gpio number for I2C master data  */

#define I2C_MASTER_FREQ_HZ 100000        /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */

#define WRITE_BIT I2C_MASTER_WRITE              /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ                /*!< I2C master read */
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */

#define DS1803_ADDR                   0x28

static const int i2c_master_port = 0;


/*
static uint8_t  values0[MAX_VALUES] = { 0x00, 0x07, 0x09, 0x10, 0x19, 0x21, 0x2D, 0x3A, 0x48, 0x5C, 0x6A }; // top
static uint8_t  values1[MAX_VALUES] = { 0x00, 0x07, 0x0C, 0x12, 0x18, 0x23, 0x2E, 0x3A, 0x44, 0x5A, 0x65 }; // bottom

                                     0.1   0.2   0.4   0.6   0.8   1.0   1.2   1.4   1.6   1.8 
*/
uint8_t  values0[MAX_VALUES] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // top
uint8_t  values1[MAX_VALUES] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // bottom

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
    if (len >= 2) {
        ESP_LOGI(TAG, "message: 0x%02X, %d", message[0], message[1]);
    } else if (len == 1) {
        ESP_LOGI(TAG, "message: 0x%02X", message[0]);
    }
    if (len > 2) {
        ESP_LOG_BUFFER_HEX(TAG, message + 2, len - 2);
    }
    
    esp_err_t ret; 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS1803_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, message, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void DS1803_set( uint8_t chn, uint8_t idx ) 
{
  const uint8_t  CMD_POT0 = 0xA9;
  const uint8_t  CMD_POT1 = 0xAA;
  const uint8_t  CMD_POTb = 0xAF;
  uint8_t  msg[2];
  
  if        ( 1 == chn) { msg[0] = CMD_POT1; msg[1] = values1[idx]; ESP_LOGI(TAG, "Sending to >CMD_POT1");
  } else if ( 0 == chn) { msg[0] = CMD_POT0; msg[1] = values0[idx]; ESP_LOGI(TAG, "Sending to >CMD_POT0");
  } else {                                   ESP_LOGI(TAG, "Sending Message >CMD_???");
  }
  i2c_master_send( msg, 2);
} // DS1803_set

void DS1803_init( void ) 
{
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_LOGI(TAG, "I2C DS1803 initialized successfully");
	DS1803_set( 0, 0);
	DS1803_set( 1, 0);
  read_values(); // config
} // init_DS1803

