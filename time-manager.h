/**
 * @file time-manager.h
 * @brief High-level RAII SNTP Time controller for ESP32.
 *
 * Provides a modern C++ wrapper around ESP-IDF SNTP APIs with automatic
 * synchronization and blocking wait mechanisms.
 *
 * @details
 * - **RAII Pattern**: Initialization of SNTP and semaphores in constructor, 
 *   cleanup of resources in destructor.
 * - **Blocking Synchronization**: The `syncAndWait()` method allows the application 
 *   to ensure the system clock is accurate before starting time-sensitive services.
 * - **Thread-Safe**: Uses atomic variables for synchronization status and 
 *   FreeRTOS semaphores for task signaling.
 * - **Debug Friendly**: Tracks the number of NTP synchronizations to monitor 
 *   network stability.
 *
 * @author Marc SIBERT
 * @copyright (c) 2026 by M. SIBERT.
 * @version 1.0
 */

#pragma once

#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <string>
#include <ctime>
#include <atomic>

constexpr const char TIME_TAG[] = "TIME";

/**
 * @class TimeManager
 * @brief RAII controller for SNTP time synchronization.
 *
 * ## Synchronization Flow
 * 1. **Instantiation**: Initializes the SNTP service with a provided NTP pool.
 * 2. **Waiting**: The application calls `syncAndWait()`, which suspends the current 
 *    task using a binary semaphore.
 * 3. **Event**: When the SNTP service receives a valid time update from the server, 
 *    the `sntp_time_cb` callback is triggered, which signals the semaphore.
 * 4. **Resumption**: The task wakes up and the system clock is now synchronized.
 *
 * ## Fail-Fast Behavior
 * If the clock cannot be synchronized within the specified timeout, the class 
 * calls `abort()`. This is based on the principle that a system without 
 * accurate time is in an invalid state for production (e.g., for TLS certificates 
 * or scheduled events).
 *
 * ## Example Usage:
 * ```cpp
 * // In app_main or a dedicated boot task
 * void boot_sequence() {
 *     // 1. Establish Network
 *     WifiManager wifi;
 *     wifi.connectAndWaitIP(); 
 * 
 *     // 2. Synchronize Time
 *     TimeManager timeMgr("pool.ntp.org");
 *     timeMgr.syncAndWait(30000); // Wait up to 30s or abort()
 * 
 *     // 3. Use time in application
 *     ESP_LOGI("APP", "System started at: %s", timeMgr.getISO8601().c_str());
 * }
 * ```
 */
class TimeManager {
public:
/**
 * @brief Construct and initialize the SNTP service.
 * 
 * Initializes the internal binary semaphore and configures the ESP-IDF 
 * SNTP stack to start automatically.
 * 
 * @param pool NTP server pool address (default: "pool.ntp.org").
 * @throw Calls abort() if the internal semaphore cannot be created.
 */
    TimeManager(const char* pool = "pool.ntp.org");

/**
 * @brief Destructor: ensures safe cleanup of the semaphore.
 */
    virtual ~TimeManager();

/**
 * @brief Block the calling task until the system clock is synchronized.
 * 
 * @param timeout Maximum time to wait for synchronization in milliseconds. 
 *                Defaults to 30,000 ms.
 * 
 * @details
 * The method uses a FreeRTOS binary semaphore to suspend the task. 
 * This is the most CPU-efficient way to wait for an asynchronous event.
 * 
 * @throw Calls abort() if the timeout is reached without synchronization.
 */
    void syncAndWait(const unsigned timeout = 30000);

/**
 * @brief Get current system time formatted as an ISO8601 UTC string.
 * 
 * @return std::string containing the time in format "YYYY-MM-DDTHH:MM:SSZ".
 * @note Returns the current system time regardless of whether it has 
 *       been synchronized.
 */
    std::string getISO8601() const;

/**
 * @brief Get the total number of successful NTP synchronizations.
 * @return Number of times the sntp_time_cb has been triggered with valid data.
 */
    uint32_t getSyncCount() const;

/**
 * @brief Check if the system clock has been synchronized at least once.
 * @return true if synchronized, false otherwise.
 */
    bool isSynced() const;

private:
/**
 * @brief Static callback required by ESP-IDF SNTP API.
 * 
 * This method is called by the SNTP service whenever the system time is updated.
 * It updates the instance state and signals the semaphore to wake up 
 * the waiting task.
 */
    static void sntp_time_callback(struct timeval *tv);

/**
 * @brief Pointer to the current instance.
 * Necessary to bridge the C-style callback to the C++ class instance.
 */
    inline static TimeManager* instance = nullptr;

/**
 * @brief Binary semaphore used to synchronize the task awaiting time acquisition.
 * 
 * Given by `sntp_time_callback` upon successful time retrieval and taken by 
 * `syncAndWait()` to unblock the application flow.
 */
    SemaphoreHandle_t semTime;

/**
 * @brief Atomic flag indicating if at least one successful synchronization has occurred.
 * 
 * Used to provide a thread-safe status check via `isSynced()`.
 */
    std::atomic<bool> is_synced;

/**
 * @brief Atomic counter tracking the total number of successful NTP updates.
 * 
 * Incremented inside the callback on every valid time update. Useful for 
 * debugging and monitoring the stability of the network connection.
 */
    std::atomic<uint32_t> sync_count;

};

// ============================================================================
// Implementation
// ============================================================================

TimeManager::TimeManager(const char* pool) :
    semTime( xSemaphoreCreateBinary() ),
    is_synced( false ),
    sync_count( 0 )
{
    instance = this;

    if (semTime == nullptr) {
        ESP_LOGE(TIME_TAG, "Failed to create semaphore!");
        abort();
    }

    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(pool);
    sntp_config.smooth_sync = true;
    sntp_config.start = true;
    sntp_config.sync_cb = sntp_time_callback;

    ESP_LOGI(TIME_TAG, "Initializing SNTP with pool: %s", pool);
    ESP_ERROR_CHECK( esp_netif_sntp_init(&sntp_config) );
//    ESP_ERROR_CHECK( sntp_set_sync_delay(600) );
}

TimeManager::~TimeManager() {
    instance = nullptr;
    if (semTime != nullptr) {
        vSemaphoreDelete(semTime);
    }
}

void TimeManager::syncAndWait(const unsigned timeout) {
    ESP_LOGI(TIME_TAG, "Waiting for SNTP sync (timeout=%u ms)...", timeout);

    if (xSemaphoreTake(semTime, pdMS_TO_TICKS(timeout)) == pdTRUE) {
        if (is_synced) ESP_LOGI(TIME_TAG, "Time synchronization successful!");
        else ESP_LOGW(TIME_TAG, "Time synchronization successful, but not synced!");
    } else {
        ESP_LOGE(TIME_TAG, "CRITICAL ERROR: SNTP timeout reached. System aborting...");
        abort();
    }
}

std::string TimeManager::getISO8601() const {
    time_t now = time(NULL);
    struct tm utc;
    gmtime_r(&now, &utc);

    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);

    return std::string(buffer);
}

uint32_t TimeManager::getSyncCount() const {
    return sync_count; 
}

bool TimeManager::isSynced() const {
    return is_synced;
}

void TimeManager::sntp_time_callback(struct timeval *tv) {
    if (tv == nullptr) return;

    if (tv->tv_sec) {
        assert(instance);
        if (instance) {
            instance->is_synced = true;
            instance->sync_count++;

            ESP_LOGI(TIME_TAG, "NTP Update received! Sync #%u | Unix: %ld",
                     instance->sync_count.load(), tv->tv_sec);

            xSemaphoreGive(instance->semTime);
        }
    }
}
