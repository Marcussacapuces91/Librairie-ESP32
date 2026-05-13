/**
 *
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
 * [...]
 *
 * ## Inheritance & Extension
 * The class is designed to be extensible:
 *  - All event-related methods are `protected` so they can be overridden.
 *  - A derived class may customize behavior for WiFi events (start, disconnect,
 *    SmartConfig, IP acquisition) without modifying the core logic.
 *
 * Example:
 * ```cpp
 * class MyWifi : public WifiManager {
 * protected:
 *     void onGotIp(const ip_event_got_ip_t& evt) override {
 *         WifiManager::onGotIp(evt);   // keep base behavior
 *         printf("Custom action: IP acquired!\\n");
 *     }
 * };
 * ```
 */
class WifiManager {
  public:
/**
 * @brief Construct the WifiManager and initialize WiFi stack.
 *
 * Initializes:
 *  - esp_netif
 *  - esp_wifi
 *  - STA interface
 *  - event handlers
 *  - internal semaphore
 *
 * WiFi is NOT started yet. Call connect() to start it.
 */
    WifiManager();

/**
 * @brief Destructor: unregister handlers, stop WiFi, free resources.
 *
 * Ensures:
 *  - WiFi is stopped (if started)
 *  - esp_wifi is deinitialized
 *  - event handlers are unregistered
 *  - semaphore is deleted
 */
    ~WifiManager();

/**
 * @brief Start WiFi and block until an IP address is obtained.
 *
 * @return esp_netif_t* The STA network interface.
 *
 * Behavior:
 *  - Calls esp_wifi_start()
 *  - Waits on semaphore until IP_EVENT_STA_GOT_IP
 *  - Returns the netif once connected
 */
    esp_netif_t* connect();

/**
 * @brief Erase WiFi credentials stored in NVS.
 *
 * Behavior:
 *  - Stops WiFi if running (ignores ESP_ERR_WIFI_NOT_STARTED)
 *  - Calls esp_wifi_restore() to erase NVS credentials
 *  - Restarts WiFi if it was previously running
 *
 * Safe to call BEFORE connect().
 */
    void clearCredentials();

  protected:
/**
 * @brief Start SmartConfig (ESP-Touch).
 *
 * Designed to be overridden in derived classes to add custom behavior
 * (LED blinking, UI feedback, logging, etc.).
 */
    void startSmartConfig();

/**
 * @brief Handle WIFI_EVENT_STA_START.
 *
 * Attempts to connect using stored credentials.
 * If no SSID is available → SmartConfig is started.
 *
 * This method is `protected` to allow overriding in derived classes
 * (e.g., custom logging, metrics, LED indicators).
 */
    void onStaStart();

/**
 * @brief Handle WIFI_EVENT_STA_DISCONNECTED.
 *
 * @param evt Disconnection event details.
 *
 * Retries connection up to `retry` times.
 * Falls back to SmartConfig when retries are exhausted.
 *
 * This method is `protected` so subclasses may override it to implement
 * custom reconnection strategies or additional diagnostics.
 */
    void onStaDisconnected(const wifi_event_sta_disconnected_t& evt);

/**
 * @brief Handle SC_EVENT_GOT_SSID_PSWD.
 *
 * @param evt SmartConfig credentials.
 *
 * Applies new credentials, reconnects, and stops SmartConfig.
 *
 * Overridable to add custom behavior when new credentials are received
 * (e.g., persisting metadata, UI feedback, logging).
 */
    void onGotSsidPass(const smartconfig_event_got_ssid_pswd_t& evt);

/**
 * @brief Handle IP_EVENT_STA_GOT_IP.
 *
 * @param evt IP information.
 *
 * Releases the semaphore to unblock connect().
 *
 * Overridable to add custom actions when the device obtains an IP
 * (e.g., start services, notify user, blink LED).
 */
    void onGotIp(const ip_event_got_ip_t& evt);

  private:
    // Internal state
    esp_netif_t* sta_netif;
    SemaphoreHandle_t semIP;
    int retry;

/**
 * @brief Static event handler registered with ESP-IDF.
 *
 * Dispatches events to the correct WifiManager instance.
 *
 * @param arg Pointer to WifiManager instance (this)
 * @param event_base WIFI_EVENT / IP_EVENT / SC_EVENT
 * @param event_id Event ID
 * @param data Event-specific data
 */
    static void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* data);

};

// ============================================================================
// Implementation
// ============================================================================

WifiManager::WifiManager() :
    sta_netif( nullptr ),
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

esp_netif_t* WifiManager::connect() {
    ESP_LOGI(WIFI, "Starting WiFi");
    ESP_ERROR_CHECK( esp_wifi_start() );

    ESP_LOGI(WIFI, "Waiting for IP...");
    xSemaphoreTake(semIP, portMAX_DELAY);

    ESP_LOGI(WIFI, "Connected!");
    return sta_netif;
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
