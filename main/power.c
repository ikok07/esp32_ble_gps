//
// Created by Kok on 2/11/26.
//

#include "power.h"

static esp_pm_lock_handle_t gHlock;

/**
 * @brief Configures the MCU's power management
 */
esp_err_t POWER_Config() {
    esp_pm_config_t POWER_Config = {
        .max_freq_mhz = CPU_MAX_MHZ,
        .min_freq_mhz = CPU_MIN_MHZ,
        .light_sleep_enable = false
    };
    return esp_pm_configure(&POWER_Config);
}

/**
 * @brief Acquires the ESP_PM_CPU_FREQ_MAX lock in order to run the CPU at max frequency
 */
esp_err_t POWER_RunMax() {
    esp_err_t err;
    if ((err = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "max-power", &gHlock)) != ESP_OK) {
        return err;
    }
    return esp_pm_lock_acquire(gHlock);
}

/**
 * @brief Acquires the ESP_PM_NO_LIGHT_SLEEP lock in order to prevent CPU from entering light sleep
 */
esp_err_t POWER_DisableLightSleep() {
    esp_err_t err;
    if ((err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "no-sleep", &gHlock)) != ESP_OK) {
        return err;
    }
    return esp_pm_lock_acquire(gHlock);
}

/**
 * @brief Releases all locks in order to return to normal power state
 */
esp_err_t POWER_RunNormal() {
    esp_err_t err;
    if ((err = esp_pm_lock_release(gHlock)) != ESP_OK) {
        return err;
    }
    return esp_pm_lock_delete(gHlock);
}
