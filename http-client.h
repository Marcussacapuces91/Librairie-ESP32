#pragma once

#include <esp_http_client.h>
#include <esp_crt_bundle.h>

class HttpClient {

    static constexpr auto TAG = "HTTP";

    const esp_http_client_config_t config;
    const esp_http_client_handle_t client{nullptr};

public:

/**
 * @brief Construct the client with fixed url.
 *
 * @param url: URL to be connected to.
 */
    HttpClient(std::string_view url);

    // Copie interdite
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

/**
 * @brief Destructor that release the ressources.
 */
    ~HttpClient();

/**
 * @brief set Authorisation header.
 *
 * @param s: content for authorization header.
 */
    void setAuthorization(std::string_view s);

/**
 * @brief Ask for a GET request, using the fixed URL, and returning the result.
 *
 * @return The response as a std::string.
 */
    std::string get() const;
};

HttpClient::HttpClient(std::string_view url) :
    config({
      .url = url.data(),
      .is_async = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
    }),
    client(esp_http_client_init(&config))
{}

HttpClient::~HttpClient() {
    ESP_ERROR_CHECK( esp_http_client_cleanup(client) );
}

void HttpClient::setAuthorization(std::string_view s) {
    ESP_ERROR_CHECK( esp_http_client_set_header(client, "Authorization", s.data()) );
}

std::string HttpClient::get() const {
    std::string resp;
    ESP_ERROR_CHECK( esp_http_client_set_method(client, HTTP_METHOD_GET) );
    ESP_ERROR_CHECK( esp_http_client_open(client, 0) );

    [[maybe_unused]] const auto content_length = esp_http_client_fetch_headers(client);  // need to be read
    
    char buffer[DEFAULT_HTTP_BUF_SIZE];
    while (true) {
        const auto read_len = esp_http_client_read_response(client, buffer, DEFAULT_HTTP_BUF_SIZE);
        if (!read_len) break;
        resp.append(buffer, read_len); 
    }

    // ESP_LOGI(TAG, "Status = %d, Length = %lld",
    //          esp_http_client_get_status_code(client),
    //          esp_http_client_get_content_length(client)
    // );

    ESP_ERROR_CHECK( esp_http_client_close(client) );
    return resp;
}
