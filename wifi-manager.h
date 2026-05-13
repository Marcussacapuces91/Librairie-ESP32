/**
 * @file wifi-manager.h
 * @brief High-level RAII WiFi controller for ESP32 (STA mode + SmartConfig).
 *
 * Provides a modern C++ wrapper around ESP-IDF WiFi APIs with automatic
 * initialization, cleanup, and SmartConfig provisioning.
 *
 * @details
 * - **RAII Pattern**: Automatic initialization in constructor, cleanup in destructor.
 *   No global state, no singletons.
 * - **Event-Driven**: All WiFi events are handled via protected virtual methods
 *   that can be overridden in derived classes.
 * - **SmartConfig Support**: Automatic fallback to ESP-Touch provisioning after
 *   3 failed connection attempts.
 * - **Thread-Safe**: Uses FreeRTOS semaphores for synchronization.
 *
 * @author Marc SIBERT
 * @copyright (c) 2026 by M. SIBERT.
 * @version 1.0
 */

#pragma once

#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"
#include "freertos/semphr.h"

constexpr const char WIFI[] = "WIFI";

/**
 * @class WifiManager
 * @brief High-level RAII WiFi controller for ESP32 (STA mode + SmartConfig).
 *
 * ## RAII Pattern
 * - **Constructor**: Initializes esp_netif, esp_wifi, STA interface, event handlers,
 *   and synchronization semaphore. WiFi is NOT started yet; call connect() to begin.
 * - **Destructor**: Safely unregisters all handlers, stops WiFi, deinits the stack,
 *   and frees the semaphore. Designed to be called without errors even if connect()
 *   was never called.
 *
 * ## SmartConfig Provisioning
 * When the device fails to connect after 3 attempts (or if no SSID is configured):
 *  1. SmartConfig (ESP-Touch) is automatically launched.
 *  2. A smartphone app sends WiFi credentials over a special encoded signal.
 *  3. Once credentials are received, the device applies them and reconnects.
 *  4. SmartConfig stops automatically after successful connection.
 *
 * ## Extensibility & Inheritance
 * All event-related methods (`startSmartConfig`, `onStaStart`, `onStaDisconnected`,
 * `onGotSsidPass`, `onGotIp`) are `protected virtual`, allowing subclasses to:
 *  - Add custom behavior (LED blinking, metrics, logging, persistent storage).
 *  - Modify event handling logic without altering core WiFi state machine.
 *  - Call the parent implementation to maintain standard behavior.
 *
 * **Example:**
 * ```cpp
 * class MyWifiManager : public WifiManager {
 * protected:
 *     void onGotIp(const ip_event_got_ip_t& evt) override {
 *         WifiManager::onGotIp(evt);  // Call parent to maintain behavior
 *         startServices();               // Custom action after IP acquired
 *         ledBlink(GREEN);               // Visual feedback
 *     }
 *
 *     void onStaDisconnected(const wifi_event_sta_disconnected_t& evt) override {
 *         logMetric("disconnection_reason", evt.reason);  // Custom logging
 *         WifiManager::onStaDisconnected(evt);            // Standard retry logic
 *     }
 * };
 * ```
 *
 * ## Thread Safety
 * - Constructor must be called from the task that initializes ESP-IDF event loop.
 * - Event handlers are called from the event loop task (internally handles locking).
 * - connect() may block the calling task waiting for IP acquisition.
 */
class WifiManager {
  public:

/**
 * @brief Construct the WifiManager and initialize WiFi stack.
 *
 * Initializes:
 *  - esp_netif
 *  - esp_wifi with STA mode configuration
 *  - STA network interface
 *  - event handlers for WiFi and IP events
 *  - FreeRTOS binary semaphore for IP acquisition synchronization
 *
 * WiFi is NOT started yet. Call connect() to start it.
 *
 * @throw Asserts on allocation failure (semaphore creation, netif creation).
 */
    WifiManager();

/**
 * @brief Destructor: unregister handlers, stop WiFi, free resources.
 *
 * Ensures safe cleanup in this order:
 *  1. Unregister all event handlers (WIFI_EVENT_STA_START, STA_DISCONNECTED,
 *     SC_EVENT_GOT_SSID_PSWD, IP_EVENT_STA_GOT_IP).
 *  2. Stop WiFi (ignores ESP_ERR_WIFI_NOT_STARTED if not running).
 *  3. Deinit the WiFi module (esp_wifi_deinit).
 *  4. Delete the semaphore (vSemaphoreDelete).
 *
 * Safe to call even if connect() was never called or if WiFi is already stopped.
 */
    ~WifiManager();

/**
 * @brief Start WiFi.
 *
 * Behavior:
 *  - Calls esp_wifi_start() to begin WiFi operations.
 */
    void connect();

/**
 * @brief Start WiFi and block until an IP address is obtained.
 *
 * @return esp_netif_ip_info_t& Reference to the IP information.
 *
 * Behavior:
 *  - Calls esp_wifi_start() to begin WiFi operations.
 *  - Waits on internal semaphore (xSemaphoreTake with portMAX_DELAY)
 *    until IP_EVENT_STA_GOT_IP is signaled.
 *  - Returns the esp_netif_ip_info_t reference to the caller once connected.
 *
 * @note This method blocks the calling task until an IP is obtained.
 *       Use FreeRTOS task awareness when integrating into application logic.
 */
    const esp_netif_ip_info_t& connectAndWaitIP();

/**
 * @brief Erase WiFi credentials stored in NVS (Non-Volatile Storage).
 *
 * Behavior:
 *  - Stops WiFi if currently running (ignores ESP_ERR_WIFI_NOT_STARTED).
 *  - Calls esp_wifi_restore() to erase stored SSID and password from NVS.
 *  - Restarts WiFi in STA mode if it was previously running.
 *  - If WiFi was not running, only erases credentials without restart.
 *
 * Safe to call **before** connect() is ever called, or after the device is
 * fully connected. Useful for factory reset or credential renewal flows.
 *
 * @post WiFi credentials are erased. Device will fall back to SmartConfig
 *       on next connection attempt if no credentials are provided externally.
 */
    void clearCredentials();

/**
 * @brief Get the current IP configuration of the STA interface.
 *
 * @return const esp_netif_ip_info_t& Reference to the current IP information
 *         (IP address, netmask, gateway).
 *
 * Behavior:
 *  - Queries the STA network interface for current IP configuration.
 *  - Returns a static reference to the queried information.
 *
 * @note The returned reference points to a static variable. Subsequent calls
 *       will overwrite the previous result. Copy the data if multiple values
 *       must be retained simultaneously.
 */
    const esp_netif_ip_info_t& getIPInfo() const;

/**
 * @brief Get information about the currently connected access point (AP).
 *
 * @return const wifi_ap_record_t& Reference to the AP record containing SSID,
 *         BSSID, channel, signal strength (RSSI), and authentication mode.
 *
 * Behavior:
 *  - Queries the ESP-IDF WiFi stack for connected AP information.
 *  - Returns a static reference to the queried AP record.
 *
 * @note The returned reference points to a static variable. Subsequent calls
 *       will overwrite the previous result. Copy the data if multiple values
 *       must be retained simultaneously.
 * @warning Should only be called after the device has successfully obtained an IP
 *          (i.e., after onGotIp() has been triggered).
 */
    const wifi_ap_record_t& getAPInfo() const;

  protected:

/**
 * @brief Start SmartConfig (ESP-Touch) provisioning mode.
 *
 * Designed to be overridden in derived classes to add custom behavior:
 *  - LED blinking or color feedback.
 *  - UI notifications or log messages.
 *  - Custom SmartConfig type or configuration.
 *
 * Default implementation:
 *  - Sets SmartConfig type to SC_TYPE_ESPTOUCH.
 *  - Starts SmartConfig with default configuration.
 *
 * Called automatically when:
 *  - No SSID is configured at STA start.
 *  - 3 connection attempts have failed.
 *
 * @details Override this to customize SmartConfig behavior. Always call
 *          the parent implementation to ensure standard SmartConfig setup.
 */
    void startSmartConfig();

/**
 * @brief Handle WIFI_EVENT_STA_START event.
 *
 * Attempts to connect using stored credentials from NVS.
 * If no SSID is configured, automatically starts SmartConfig provisioning.
 *
 * This method is `protected virtual` to allow overriding in derived classes
 * for custom behavior such as:
 *  - Custom logging or metrics collection.
 *  - LED indicators (starting, attempting).
 *  - Pre-connection validation or state setup.
 *
 * Default implementation:
 *  - Calls esp_wifi_connect() to use stored credentials.
 *  - If ESP_ERR_WIFI_SSID is returned, starts SmartConfig.
 */
    void onStaStart();

/**
 * @brief Handle WIFI_EVENT_STA_DISCONNECTED event.
 *
 * @param evt Disconnection event details (includes disconnection reason code).
 *
 * Implements automatic retry logic:
 *  - Retries connection up to 3 times after disconnection.
 *  - Falls back to SmartConfig when retries are exhausted.
 *  - Resets retry counter for next connection cycle.
 *
 * This method is `protected virtual` so subclasses may override it to:
 *  - Implement custom reconnection strategies (backoff, max delay).
 *  - Log disconnection reasons for diagnostics.
 *  - Perform cleanup or state reset on unexpected disconnections.
 *
 * Default implementation:
 *  - Decrements internal retry counter.
 *  - If retries remain, calls esp_wifi_connect() again.
 *  - If retries exhausted, resets counter to 3 and launches SmartConfig.
 *
 * @note The retry counter is independent per disconnect event and resets
 *       after SmartConfig provisioning or successful reconnection.
 */
    void onStaDisconnected(const wifi_event_sta_disconnected_t& evt);

/**
 * @brief Handle SC_EVENT_GOT_SSID_PSWD event (SmartConfig credentials received).
 *
 * @param evt SmartConfig event data containing provisioned SSID and password.
 *
 * Applies new credentials and reconnects:
 *  1. Copies SSID and password from event into wifi_config_t.
 *  2. Stops SmartConfig (esp_smartconfig_stop).
 *  3. Disconnects current connection if active.
 *  4. Sets new WiFi configuration (esp_wifi_set_config).
 *  5. Initiates reconnection with new credentials.
 *
 * Overridable to add custom behavior when new credentials are received:
 *  - Persist credentials to external storage (EEPROM, custom NVS).
 *  - Send provisioning success notification to backend.
 *  - Log provisioning metadata (timestamp, app version, device ID).
 *  - Update UI to show "Connecting with new WiFi" state.
 *
 * @post SmartConfig is stopped. Device reconnects with new credentials.
 *       If connection succeeds, IP_EVENT_STA_GOT_IP will be signaled.
 */
    void onGotSsidPass(const smartconfig_event_got_ssid_pswd_t& evt);

/**
 * @brief Handle IP_EVENT_STA_GOT_IP event (device obtained IP address).
 *
 * @param evt IP event data containing IP address, netmask, gateway, etc.
 *
 * Releases the semaphore to unblock the connect() method call.
 *
 * Overridable to add custom actions when the device gains network connectivity:
 *  - Start background services (OTA, time sync, telemetry).
 *  - Notify application via callback or event system.
 *  - Blink LED or update UI to show "Connected" state.
 *  - Log connection metrics (signal strength, channel, authentication type).
 *
 * Default implementation:
 *  - Logs the acquired IP address.
 *  - Signals the semaphore so connect() returns to caller.
 *
 * @note Always call the parent implementation to maintain semaphore signaling.
 */
    void onGotIp(const ip_event_got_ip_t& evt);

  private:
    /**
     * @brief Pointer to the STA (Station) network interface.
     *
     * Initialized in constructor via esp_netif_create_default_wifi_sta().
     * Returned to caller from connect() after IP is acquired.
     * Deleted in destructor via esp_netif_destroy_default_wifi().
     */
    esp_netif_t* sta_netif;

    /**
     * @brief Binary semaphore for IP acquisition synchronization.
     *
     * Created in constructor: xSemaphoreCreateBinary().
     * Given (signaled) by onGotIp() when IP_EVENT_STA_GOT_IP is received.
     * Taken (waited on) by connect() to block until IP is available.
     * Deleted in destructor: vSemaphoreDelete().
     *
     * Allows connect() to block on the calling task while event loop
     * handles WiFi state transitions asynchronously.
     */
    SemaphoreHandle_t semIP;

    /**
     * @brief Connection retry counter for automatic reconnection.
     *
     * Initialized to 3 in constructor.
     * Decremented in onStaDisconnected() on each disconnection event.
     * Resets to 3 when retries exhausted and SmartConfig is launched.
     * Allows up to 3 reconnection attempts before falling back to provisioning.
     *
     * Permits graceful handling of transient WiFi issues without immediately
     * requiring user intervention via SmartConfig.
     */
    int retry;

/**
 * @brief Static event handler registered with ESP-IDF event loop.
 *
 * @param arg Opaque pointer to WifiManager instance (cast to `this` internally).
 * @param event_base Event base identifier (WIFI_EVENT, IP_EVENT, or SC_EVENT).
 * @param event_id Specific event ID within the event base.
 * @param data Event-specific payload (cast to appropriate struct type).
 *
 * Dispatches ESP-IDF events to the correct WifiManager instance by:
 *  1. Casting `arg` to WifiManager* to recover the instance.
 *  2. Checking event_base and event_id to determine event type.
 *  3. Calling the appropriate protected method (onStaStart, onStaDisconnected, etc.).
 *
 * Registered for these events:
 *  - WIFI_EVENT_STA_START → onStaStart()
 *  - WIFI_EVENT_STA_DISCONNECTED → onStaDisconnected()
 *  - SC_EVENT_GOT_SSID_PSWD → onGotSsidPass()
 *  - IP_EVENT_STA_GOT_IP → onGotIp()
 *
 * @details Static methods cannot be virtual, so this dispatcher pattern
 *          is necessary to enable polymorphic event handling in derived classes.
 *          The instance pointer is passed via the `arg` parameter during
 *          handler registration.
 */
    static void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* data);

};

// ============================================================================
// Implementation
// ============================================================================

WifiManager::WifiManager() :
    semIP( xSemaphoreCreateBinary() ),
    retry( 3 )
{
    assert(semIP);

    ESP_LOGI(WIFI, "Init NetIf");
    ESP_ERROR_CHECK( esp_netif_init() );

    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

    // Register handlers
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, eventHandler, this) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, eventHandler, this) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, SC_EVENT_GOT_SSID_PSWD, eventHandler, this) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler, this) );
}

WifiManager::~WifiManager() {
    // Unregister
    ESP_ERROR_CHECK( esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, eventHandler) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, eventHandler) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(SC_EVENT, SC_EVENT_GOT_SSID_PSWD, eventHandler) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler) );

    const auto err = esp_wifi_stop();
    if (err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_OK) ESP_ERROR_CHECK( err );

    ESP_ERROR_CHECK( esp_wifi_deinit() );

    vSemaphoreDelete(semIP);
}

void WifiManager::connect() {
    ESP_LOGI(WIFI, "Starting WiFi");
    ESP_ERROR_CHECK( esp_wifi_start() );
}

const esp_netif_ip_info_t& WifiManager::connectAndWaitIP() {
    connect();

    ESP_LOGI(WIFI, "Waiting for IP...");
    xSemaphoreTake(semIP, portMAX_DELAY);

    ESP_LOGI(WIFI, "Connected!");
    return getIPInfo();
}

void WifiManager::clearCredentials() {
    ESP_LOGW(WIFI, "Clearing WiFi credentials...");

    const auto err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
      ESP_ERROR_CHECK(esp_wifi_restore());
    } else if (err == ESP_OK) {
      ESP_ERROR_CHECK(esp_wifi_restore());
      ESP_ERROR_CHECK(esp_wifi_start());
    } else ESP_ERROR_CHECK(err);

    ESP_LOGI(WIFI, "WiFi credentials erased.");
}

const esp_netif_ip_info_t& WifiManager::getIPInfo() const {
    static esp_netif_ip_info_t ip_info;
    assert(sta_netif);
    ESP_ERROR_CHECK( esp_netif_get_ip_info(sta_netif, &ip_info) );
    return ip_info;
}

const wifi_ap_record_t& WifiManager::getAPInfo() const {
    static wifi_ap_record_t ap_record;
    ESP_ERROR_CHECK( esp_wifi_sta_get_ap_info(&ap_record) );
    return ap_record;
}

void WifiManager::startSmartConfig() {
    ESP_LOGI(WIFI, "Starting SmartConfig");
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
}

void WifiManager::onStaStart() {
    ESP_LOGI(WIFI, "STA start → connecting using current credentials...");

    // try to connect using stored credentials
    const auto err = esp_wifi_connect();

    // Note: ESP_ERR_WIFI_SSID is almost never returned in practice
    if (err == ESP_ERR_WIFI_SSID) {
      ESP_LOGE(WIFI, "Error connecting: No SSID defined!");
      startSmartConfig();
    } else
      ESP_ERROR_CHECK(err);
}

void WifiManager::onStaDisconnected(const wifi_event_sta_disconnected_t& evt) {
    ESP_LOGW(WIFI, "Disconnected (reason=%d)", evt.reason);

    if (retry--) {
        ESP_LOGI(WIFI, "Retrying (%d left)", retry);
        ESP_ERROR_CHECK( esp_wifi_connect() );
    } else {
        // Retries exhausted, reset counter and start SmartConfig
        retry = 3;
        startSmartConfig();
    }
}

void WifiManager::onGotSsidPass(const smartconfig_event_got_ssid_pswd_t& evt) {
    ESP_LOGI(WIFI, "Received SSID and password");

    wifi_config_t cfg = {};
    memcpy(cfg.sta.ssid, evt.ssid, sizeof(cfg.sta.ssid));
    memcpy(cfg.sta.password, evt.password, sizeof(cfg.sta.password));

    ESP_ERROR_CHECK( esp_smartconfig_stop() );

    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &cfg) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

void WifiManager::onGotIp(const ip_event_got_ip_t& evt) {
    ESP_LOGI(WIFI, "Got IP " IPSTR, IP2STR(&evt.ip_info.ip));
    xSemaphoreGive(semIP);
}

void WifiManager::eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* data) {
    assert(arg);
    auto& instance = *static_cast<WifiManager*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(WIFI, "WIFI Station Start event");
        instance.onStaStart();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(WIFI, "STA disconnected event");
        instance.onStaDisconnected(*static_cast<const wifi_event_sta_disconnected_t*>(data));
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(WIFI, "Got SSID and password event");
        instance.onGotSsidPass(*static_cast<const smartconfig_event_got_ssid_pswd_t*>(data));
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(WIFI, "Got IP event");
        instance.onGotIp(*static_cast<const ip_event_got_ip_t*>(data));
    }
    else {
      ESP_LOGW(WIFI, "Unattended event!");
    }
}
