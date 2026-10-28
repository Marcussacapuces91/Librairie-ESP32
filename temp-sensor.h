/**
 * @file temp-sensor.h
 * @brief RAII wrapper for the ESP32 internal temperature sensor.
 *
 * This header defines a small C++ interface that encapsulates the ESP-IDF
 * internal temperature sensor driver. The class automatically installs,
 * enables, disables, and uninstalls the sensor, which makes it safe to use as
 * a scoped object without manual lifecycle management.
 *
 * The implementation follows the RAII pattern: the sensor is created and
 * initialized in the constructor, and all associated resources are released in
 * the destructor. The class is intentionally non-copyable to avoid multiple
 * owners sharing the same hardware handle.
 *
 * @author Marc SIBERT
 * @copyright (c) 2026 by M. SIBERT.
 * @version 1.0
 */

#pragma once

#include <driver/temperature_sensor.h>

/**
 * @brief High-level object that manages the ESP32 internal temperature sensor.
 *
 * This class wraps the ESP-IDF temperature sensor driver and provides a simple
 * read-only interface to obtain the current internal temperature in degrees
 * Celsius. The sensor is initialized automatically when an instance is created.
 */
class TempSensor {
    temperature_sensor_config_t temp_sensor;
    temperature_sensor_handle_t handle = nullptr;

public:

    /**
     * @brief Constructs and starts the internal temperature sensor.
     *
     * Installs the driver with the default configuration and immediately enables
     * the sensor so it can start reporting values.
     */
    TempSensor();

    /**
     * @brief Deleted copy constructor.
     *
     * The underlying ESP-IDF handle is owned by a single instance, so copying a
     * TempSensor object would create ambiguous ownership.
     */
    TempSensor(const TempSensor&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     *
     * The sensor handle must remain uniquely owned by one object; assignment is
     * therefore forbidden.
     */
    TempSensor& operator=(const TempSensor&) = delete;

    /**
     * @brief Destroys the sensor and releases the associated resources.
     *
     * If the sensor was successfully installed, it is disabled and uninstalled
     * before the object is destroyed.
     */
    ~TempSensor();

    /**
     * @brief Reads the current temperature in degrees Celsius.
     *
     * @return The current internal temperature measured by the ESP32 sensor, in
     *         degrees Celsius.
     */
    float read() const;

};

/**
 * @brief Initializes the internal temperature sensor and enables measurements.
 *
 * Uses the default ESP-IDF temperature sensor configuration and installs the
 * driver into the system.
 */
TempSensor::TempSensor() :
    temp_sensor( TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100) )
{
    ESP_ERROR_CHECK( temperature_sensor_install(&temp_sensor, &handle) );
    ESP_ERROR_CHECK( temperature_sensor_enable(handle) );
};

/**
 * @brief Disables and releases the temperature sensor handle.
 *
 * This cleanup is automatically executed when the object goes out of scope to
 * prevent resource leaks.
 */
TempSensor::~TempSensor() {
    if (handle) {
        ESP_ERROR_CHECK( temperature_sensor_disable(handle) );
        ESP_ERROR_CHECK( temperature_sensor_uninstall(handle) );
    }
};

/**
 * @brief Returns the current temperature read by the internal sensor.
 *
 * @return The measured temperature in degrees Celsius.
 */
float TempSensor::read() const {
    float temp = 0;
    ESP_ERROR_CHECK( temperature_sensor_get_celsius(handle, &temp) );
    return temp;
}
