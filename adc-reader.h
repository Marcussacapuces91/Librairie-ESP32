#pragma once
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <utility>

template<int Gpio, adc_bitwidth_t bitwidth = ADC_BITWIDTH_12, adc_atten_t atten = ADC_ATTEN_DB_12>
class AdcReader {
    static_assert(Gpio >= 0 && Gpio <= 48, "Invalid GPIO number");

    adc_oneshot_unit_handle_t adc_handle = nullptr;
    adc_cali_handle_t cali_handle = nullptr;

    adc_unit_t unit;
    adc_channel_t channel;

public:
    AdcReader() {
        // Déduire automatiquement unité + canal
        ESP_ERROR_CHECK(adc_oneshot_io_to_channel(Gpio, &unit, &channel));

        // Créer l’unité ADC
        adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = unit
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

        // Configurer le canal
        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = atten,
            .bitwidth = bitwidth
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channel, &chan_cfg));

        // Calibration : Arduino utilise "curve fitting", pas "line fitting"
        adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = bitwidth
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle));
    }

    ~AdcReader() {
        cleanup();
    }

    // Non-copyable
    AdcReader(const AdcReader&) = delete;
    AdcReader& operator=(const AdcReader&) = delete;

    // Non-movable
    AdcReader(AdcReader&&) = delete;
    AdcReader& operator=(AdcReader&&) = delete;

    int readRaw() const {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw));
        return raw;
    }

    int readVoltage() const {
        int raw = readRaw();
        int mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &mv));
        return mv;
    }

private:
    void cleanup() {
        if (adc_handle) {
            adc_oneshot_del_unit(adc_handle);
            adc_handle = nullptr;
        }
        if (cali_handle) {
            adc_cali_delete_scheme_curve_fitting(cali_handle);
            cali_handle = nullptr;
        }
    }

};
