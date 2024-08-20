#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#include <esp_system.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"
static const char *TAG = "camera_capture";

// Configuration for the camera
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1 // Software reset will be performed
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22




void app_main(void) {

     // Initialize the camera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sscb_sda = CAM_PIN_SIOD;
    config.pin_sscb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_96X96;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_gain_ctrl(s, 0);                       // auto gain off
    s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
    s->set_exposure_ctrl(s, 0);                   // auto exposure off
    s->set_brightness(s, 2);                     // (-2 to 2) - set brightness
    s->set_agc_gain(s, 10);          // set gain manually (0 - 30)
    s->set_aec_value(s, 500);     // set exposure manually  (0-1200)




    // Initialize NVS
    esp_err_t err_nvs = nvs_flash_init();
    if (err_nvs != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) initializing NVS!", esp_err_to_name(err_nvs));
        return;
    }

    // Open NVS handle
    nvs_handle_t my_handle;
    err_nvs = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err_nvs != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS handle!", esp_err_to_name(err_nvs));
        return;
    }

    // Read the counter value, initialize if not found
    int file_index = 0;
    err_nvs = nvs_get_i32(my_handle, "file_index", &file_index);
    if (err_nvs == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI("NVS", "The value is not initialized yet!");
    } else if (err_nvs != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) reading!", esp_err_to_name(err_nvs));
        nvs_close(my_handle);
        return;
    }

    // Increment and save the counter
    file_index++;
    err_nvs = nvs_set_i32(my_handle, "file_index", file_index);
    if (err_nvs != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) writing!", esp_err_to_name(err_nvs));
        nvs_close(my_handle);
        return;
    }

    // Commit the value to NVS
    err_nvs = nvs_commit(my_handle);
    nvs_close(my_handle);
    if (err_nvs != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) committing!", esp_err_to_name(err_nvs));
        return;
    }
    

    // Mount SD card
    const char mount_point[] = "/sdcard";
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t* card;

    err_nvs = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (err_nvs != ESP_OK) {
        ESP_LOGE("SD Card", "Failed to mount filesystem (%s).", esp_err_to_name(err_nvs));
        return;
    }



    camera_fb_t *fb = esp_camera_fb_get();
    if(!fb){ 
        ESP_LOGE(TAG, "camera capture failed");
        esp_vfs_fat_sdcard_unmount(mount_point, card);
        return;
    }

    // Create unique file name
    char filepath[64];
    sprintf(filepath, "%s/file_%d.txt", mount_point, file_index);

    // Open file for writing
    FILE* f = fopen(filepath, "w");
    for (size_t i = 0; i < fb->len; i++){ 
        fprintf(f, "%02x ", fb->buf[i]);
        if((i+1 ) % 16 == 0){ 
            fprintf(f, "\n");
        }
    }
    fclose(f);
    // if (f != NULL) {
    //     fwrite(fb->buf,1 , fb->len, f);
    //     fclose(f);
    //     ESP_LOGI(TAG, "File saved to %s ", filepath);
        
    // }else{ 
    //     ESP_LOGE(TAG, "Failed to open file for writing");
    // }

    esp_camera_fb_return(fb);

    // Write to file
    // fprintf(f, "This is the log number %d.\n", file_index);
    // fclose(f);
    ESP_LOGI("File", "File %s has been created and written successfully", filepath);

    // Unmount the SD card
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI("SD Card", "Card unmounted");
}
