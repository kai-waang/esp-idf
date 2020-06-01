/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "soc/i2s_periph.h"
#include "soc/rtc_periph.h"
#include "soc/soc_caps.h"
#include "hal/i2s_hal.h"
#include "hal/i2s_types.h"
#include "driver/periph_ctrl.h"
#include "esp_intr_alloc.h"

#if SOC_I2S_SUPPORTS_ADC_DAC
#include "driver/adc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define I2S_PIN_NO_CHANGE (-1) /*!< Use in i2s_pin_config_t for pins which should not be changed */


/**
 * @brief I2S driver configuration parameters
 *
 */
typedef struct {
    union {
        // Compatible with previous versions. For ESP32-S3, ESP32-C3 and the later chip, you should use `param_cfg` fields to initialize I2S.
        struct {
            i2s_mode_t              mode;                   /*!< I2S work mode*/
            uint32_t                sample_rate;            /*!< I2S sample rate*/
            uint32_t                bits_per_sample;        /*!< I2S bits per sample*/
            i2s_channel_fmt_t       channel_format;         /*!< I2S channel format */
            i2s_comm_format_t       communication_format;   /*!< I2S communication format */
        };
        i2s_config_param_t param_cfg;                       /*!< I2S config paramater */
    };
    int  intr_alloc_flags;       /*!< Flags used to allocate the interrupt. One or multiple (ORred) ESP_INTR_FLAG_* values. See esp_intr_alloc.h for more info */
    int  dma_buf_count;          /*!< I2S DMA Buffer Count */
    int  dma_buf_len;            /*!< I2S DMA Buffer Length */
    bool use_apll;              /*!< I2S using APLL as main I2S clock, enable it to get accurate clock */
    bool tx_desc_auto_clear;     /*!< I2S auto clear tx descriptor if there is underflow condition (helps in avoiding noise in case of data unavailability) */
    int  fixed_mclk;             /*!< I2S using fixed MCLK output. If use_apll = true and fixed_mclk > 0, then the clock output for i2s is fixed and equal to the fixed_mclk value.*/
} i2s_driver_config_t;

typedef i2s_driver_config_t i2s_config_t;
typedef intr_handle_t i2s_isr_handle_t;

/**
 * @brief I2S event types
 *
 */
typedef enum {
    I2S_EVENT_DMA_ERROR,
    I2S_EVENT_TX_DONE,     /*!< I2S DMA finish sent 1 buffer*/
    I2S_EVENT_RX_DONE,     /*!< I2S DMA finish received 1 buffer*/
    I2S_EVENT_MAX,         /*!< I2S event max index*/
} i2s_event_type_t;
/**
 * @brief Event structure used in I2S event queue
 *
 */
typedef struct {
    i2s_event_type_t    type;   /*!< I2S event type */
    size_t              size;   /*!< I2S data size for I2S_DATA event*/
} i2s_event_t;

/**
 * @brief Set I2S pin number
 *
 * @note
 * The I2S peripheral output signals can be connected to multiple GPIO pads.
 * However, the I2S peripheral input signal can only be connected to one GPIO pad.
 *
 * @param   i2s_num     I2S_NUM_0 or I2S_NUM_1
 *
 * @param   pin         I2S Pin structure, or NULL to set 2-channel 8-bit internal DAC pin configuration (GPIO25 & GPIO26)
 *
 * Inside the pin configuration structure, set I2S_PIN_NO_CHANGE for any pin where
 * the current configuration should not be changed.
 *
 * @note if *pin is set as NULL, this function will initialize both of the built-in DAC channels by default.
 *       if you don't want this to happen and you want to initialize only one of the DAC channels, you can call i2s_set_dac_mode instead.
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL            IO error
 */
esp_err_t i2s_set_pin(i2s_port_t i2s_num, const i2s_pin_config_t *pin);

#if SOC_I2S_SUPPORTS_PDM_RX
/**
 * @brief Set PDM mode down-sample rate
 *        In PDM RX mode, there would be 2 rounds of downsample process in hardware.
 *        In the first downsample process, the sampling number can be 16 or 8.
 *        In the second downsample process, the sampling number is fixed as 8.
 *        So the clock frequency in PDM RX mode would be (fpcm * 64) or (fpcm * 128) accordingly.
 * @param i2s_num I2S_NUM_0, I2S_NUM_1
 * @param dsr i2s RX down sample rate for PDM mode.
 *
 * @note After calling this function, it would call i2s_set_clk inside to update the clock frequency.
 *       Please call this function after I2S driver has been initialized.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM      Out of memory
 */
esp_err_t i2s_set_pdm_rx_down_sample(i2s_port_t i2s_num, i2s_pdm_dsr_t dsr);
#endif

#if SOC_I2S_SUPPORTS_PDM_TX
/**
 * @brief Set TX PDM mode up-sample rate
 *        TX PDM have two type upsampling rate configurations:
 *        1: fp = 960, fs = sample_rate / 100, in this case, Fpdm = 128*48000
 *        2: fp = 960, fs = 480, in this case, Fpdm = 128*Fpcm = 128*sample_rate
 *        If the pdm receiver do not care the pdm serial clock, it's recommended set Fpdm = 128*48000
 *
 * @param i2s_num I2S_NUM_0
 * @param sample_rate The sample rate to be set
 * @param fp PDM TX upsampling configuration paramater
 * @param fs PDM TX upsampling configuration paramater
 *
 * @note After calling this function, it would call i2s_set_clk inside to update the clock frequency.
 *       Please call this function after I2S driver has been initialized.
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM      Out of memory
 */
esp_err_t i2s_set_pdm_tx_up_sample(i2s_port_t i2s_num, int sample_rate, int fp, int fs);
#endif

/**
 * @brief Install and start I2S driver.
 *
 * @param i2s_num         I2S_NUM_0, I2S_NUM_1
 *
 * @param i2s_config      I2S configurations - see i2s_config_t struct
 *
 * @param queue_size      I2S event queue size/depth.
 *
 * @param i2s_queue       I2S event queue handle, if set NULL, driver will not use an event queue.
 *
 * This function must be called before any I2S driver read/write operations.
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM      Out of memory
 */
esp_err_t i2s_driver_install(i2s_port_t i2s_num, const i2s_config_t *i2s_config, int queue_size, void* i2s_queue);

/**
 * @brief Uninstall I2S driver.
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_driver_uninstall(i2s_port_t i2s_num);

/**
 * @brief Write data to I2S DMA transmit buffer.
 *
 * @param i2s_num             I2S_NUM_0, I2S_NUM_1
 *
 * @param src                 Source address to write from
 *
 * @param size                Size of data in bytes
 *
 * @param[out] bytes_written  Number of bytes written, if timeout, the result will be less than the size passed in.
 *
 * @param ticks_to_wait       TX buffer wait timeout in RTOS ticks. If this
 * many ticks pass without space becoming available in the DMA
 * transmit buffer, then the function will return (note that if the
 * data is written to the DMA buffer in pieces, the overall operation
 * may still take longer than this timeout.) Pass portMAX_DELAY for no
 * timeout.
 *
 * @return
 *     - ESP_OK               Success
 *     - ESP_ERR_INVALID_ARG  Parameter error
 */
esp_err_t i2s_write(i2s_port_t i2s_num, const void *src, size_t size, size_t *bytes_written, TickType_t ticks_to_wait);

/**
 * @brief Write data to I2S DMA transmit buffer while expanding the number of bits per sample. For example, expanding 16-bit PCM to 32-bit PCM.
 *
 * @param i2s_num             I2S_NUM_0, I2S_NUM_1
 *
 * @param src                 Source address to write from
 *
 * @param size                Size of data in bytes
 *
 * @param src_bits            Source audio bit
 *
 * @param aim_bits            Bit wanted, no more than 32, and must be greater than src_bits
 *
 * @param[out] bytes_written  Number of bytes written, if timeout, the result will be less than the size passed in.
 *
 * @param ticks_to_wait       TX buffer wait timeout in RTOS ticks. If this
 * many ticks pass without space becoming available in the DMA
 * transmit buffer, then the function will return (note that if the
 * data is written to the DMA buffer in pieces, the overall operation
 * may still take longer than this timeout.) Pass portMAX_DELAY for no
 * timeout.
 *
 * Format of the data in source buffer is determined by the I2S
 * configuration (see i2s_config_t).
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_write_expand(i2s_port_t i2s_num, const void *src, size_t size, size_t src_bits, size_t aim_bits, size_t *bytes_written, TickType_t ticks_to_wait);

/**
 * @brief Read data from I2S DMA receive buffer
 *
 * @param i2s_num         I2S_NUM_0, I2S_NUM_1
 *
 * @param dest            Destination address to read into
 *
 * @param size            Size of data in bytes
 *
 * @param[out] bytes_read Number of bytes read, if timeout, bytes read will be less than the size passed in.
 *
 * @param ticks_to_wait   RX buffer wait timeout in RTOS ticks. If this many ticks pass without bytes becoming available in the DMA receive buffer, then the function will return (note that if data is read from the DMA buffer in pieces, the overall operation may still take longer than this timeout.) Pass portMAX_DELAY for no timeout.
 *
 * @note If the built-in ADC mode is enabled, we should call i2s_adc_enable and i2s_adc_disable around the whole reading process,
 *       to prevent the data getting corrupted.
 *
 * @return
 *     - ESP_OK               Success
 *     - ESP_ERR_INVALID_ARG  Parameter error
 */
esp_err_t i2s_read(i2s_port_t i2s_num, void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait);

/**
 * @brief Set sample rate used for I2S RX and TX.
 *
 * The bit clock rate is determined by the sample rate and i2s_config_t configuration parameters (number of channels, bits_per_sample).
 *
 * `bit_clock = rate * (number of channels) * bits_per_sample`
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
 * @param rate I2S sample rate (ex: 8000, 44100...)
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM      Out of memory
 */
esp_err_t i2s_set_sample_rates(i2s_port_t i2s_num, uint32_t rate);

/**
 * @brief Stop I2S driver
 *
 * There is no need to call i2s_stop() before calling i2s_driver_uninstall().
 *
 * Disables I2S TX/RX, until i2s_start() is called.
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_stop(i2s_port_t i2s_num);

/**
 * @brief Start I2S driver
 *
 * It is not necessary to call this function after i2s_driver_install() (it is started automatically), however it is necessary to call it after i2s_stop().
 *
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
* @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_start(i2s_port_t i2s_num);

/**
 * @brief Zero the contents of the TX DMA buffer.
 *
 * Pushes zero-byte samples into the TX DMA buffer, until it is full.
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_zero_dma_buffer(i2s_port_t i2s_num);

#if SOC_I2S_SUPPORTS_PCM
/**
 * @brief Configure I2S a/u-law decompress or compress
 *
 * @param i2s_num  I2S_NUM_0
 *
 * @param mode  I2S mode. I2S_MODE_TX, I2S_MODE_RX
 *
 * @param pcm_cfg  a/u-law decompress or compress configuration paramater
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_pcm_config(i2s_port_t i2s_num, int mode, i2s_pcm_cfg_t pcm_cfg);
#endif

/**
 * @brief Set clock & bit width used for I2S RX and TX.
 *
 * Similar to i2s_set_sample_rates(), but also sets bit width.
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
 * @param rate I2S sample rate (ex: 8000, 44100...)
 *
 * @param slot_bits i2s slot bit configuration
 *
 * @param sloct_ch I2S slot number configuration
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM      Out of memory
 */
esp_err_t i2s_set_clk(i2s_port_t i2s_num, uint32_t rate, i2s_slot_bits_cfg_t slot_bits, i2s_slot_channel_cfg_t sloct_ch);

/**
 * @brief get clock set on particular port number.
 *
 * @param i2s_num  I2S_NUM_0, I2S_NUM_1
 *
 * @return
 *     - actual clock set by i2s driver
 */
uint32_t i2s_get_clk(i2s_port_t i2s_num);

#if SOC_I2S_SUPPORTS_ADC_DAC
/**
 * @brief Set built-in ADC mode for I2S DMA, this function will initialize ADC pad,
 *        and set ADC parameters.
 * @note  In this mode, the ADC maximum sampling rate is 150KHz. Set the sampling rate through ``i2s_config_t``.
 * @param adc_unit    SAR ADC unit index
 * @param adc_channel ADC channel index
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t i2s_set_adc_mode(adc_unit_t adc_unit, adc1_channel_t adc_channel);

/**
 * @brief Start to use I2S built-in ADC mode
 * @note This function would acquire the lock of ADC to prevent the data getting corrupted
 *       during the I2S peripheral is being used to do fully continuous ADC sampling.
 *
 * @param i2s_num i2s port index
 * @return
 *     - ESP_OK                Success
 *     - ESP_ERR_INVALID_ARG   Parameter error
 *     - ESP_ERR_INVALID_STATE Driver state error
 */
esp_err_t i2s_adc_enable(i2s_port_t i2s_num);

/**
 * @brief Stop to use I2S built-in ADC mode
 * @param i2s_num i2s port index
 * @note This function would release the lock of ADC so that other tasks can use ADC.
 * @return
 *     - ESP_OK                 Success
 *     - ESP_ERR_INVALID_ARG    Parameter error
 *     - ESP_ERR_INVALID_STATE  Driver state error
 */
esp_err_t i2s_adc_disable(i2s_port_t i2s_num);

/**
 * @brief Set I2S dac mode, I2S built-in DAC is disabled by default
 *
 * @param dac_mode DAC mode configurations - see i2s_dac_mode_t
 *
 * @note Built-in DAC functions are only supported on I2S0 for current ESP32 chip.
 *       If either of the built-in DAC channel are enabled, the other one can not
 *       be used as RTC DAC function at the same time.
 *
 * @return
 *     - ESP_OK               Success
 *     - ESP_ERR_INVALID_ARG  Parameter error
 */
esp_err_t i2s_set_dac_mode(i2s_dac_mode_t dac_mode);
#endif //SOC_I2S_SUPPORTS_ADC_DAC


#ifdef __cplusplus
}
#endif
