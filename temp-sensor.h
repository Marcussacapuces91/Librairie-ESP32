#pragma once

#include <driver/temperature_sensor.h>

class TempSensor {
    temperature_sensor_config_t temp_sensor;
    temperature_sensor_handle_t handle = nullptr;

public:

    TempSensor();

// Non-copyable!
    TempSensor(const TempSensor&) = delete;
    TempSensor& operator=(const TempSensor&) = delete;

    ~TempSensor();

    float read() const;

};

TempSensor::TempSensor() :
    temp_sensor( TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100) )
{
    ESP_ERROR_CHECK( temperature_sensor_install(&temp_sensor, &handle) );
    ESP_ERROR_CHECK( temperature_sensor_enable(handle) );
};

TempSensor::~TempSensor() {
    if (handle) {
        ESP_ERROR_CHECK( temperature_sensor_disable(handle) );
        ESP_ERROR_CHECK( temperature_sensor_uninstall(handle) );
    }
};

float TempSensor::read() const {
    float temp = 0;
    ESP_ERROR_CHECK( temperature_sensor_get_celsius(handle, &temp) );
    return temp;
}
