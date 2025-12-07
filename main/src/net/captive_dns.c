/*
 * SPDX-FileCopyrightText: 2010-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <errno.h>
#include "esp_timer.h"

static const char *TAG = "CAPTIVE_DNS";

// DNS packet structure definitions
#define DNS_PORT 53
#define DNS_MAX_PACKET_SIZE 512
#define DNS_HEADER_SIZE 12
#define DNS_QUERY_FLAG_RESPONSE 0x8000
#define DNS_QUERY_FLAG_AUTHORITATIVE 0x0400
#define DNS_QUERY_FLAG_RECURSION_DESIRED 0x0100

// DNS header structure
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority_rrs;
    uint16_t additional_rrs;
} __attribute__((packed)) dns_header_t;

// DNS answer structure for A record
typedef struct {
    uint16_t name_ptr;      // Pointer to name (0xC00C for compression)
    uint16_t type;          // A record = 1
    uint16_t class;         // IN = 1
    uint32_t ttl;           // Time to live
    uint16_t data_length;   // Length of IP address (4 bytes)
    uint32_t ip_address;    // IP address in network byte order
} __attribute__((packed)) dns_answer_t;

static int dns_socket = -1;

// Function to get the AP IP address
static uint32_t get_ap_ip_address(void) {
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to get AP netif handle");
        return inet_addr("192.168.4.1"); // fallback
    }
    
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(ap_netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP IP info: %s", esp_err_to_name(err));
        return inet_addr("192.168.4.1"); // fallback
    }

    return ip_info.ip.addr;
}

// Parse domain name from DNS query and return query type
static int parse_dns_query(const uint8_t *packet, size_t packet_len, char *domain, size_t domain_size) {
    if (packet_len < DNS_HEADER_SIZE + 5) { // minimum query size
        return -1;
    }
    
    const dns_header_t *header = (const dns_header_t *)packet;
    if (ntohs(header->questions) != 1) {
        ESP_LOGD(TAG, "Unexpected question count: %d", ntohs(header->questions));
        return -1;
    }
    
    // Skip DNS header to get to question section
    const uint8_t *question_start = packet + DNS_HEADER_SIZE;
    const uint8_t *ptr = question_start;
    const uint8_t *packet_end = packet + packet_len;
    
    // Parse domain name from question
    size_t domain_pos = 0;
    bool first_label = true;
    
    while (ptr < packet_end && *ptr != 0) {
        uint8_t label_len = *ptr++;
        
        if (label_len == 0) break;
        
        // Check for compression (not expected in questions, but handle it)
        if ((label_len & 0xC0) == 0xC0) {
            ESP_LOGD(TAG, "Unexpected compression in DNS question");
            break;
        }
        
        // Check bounds
        if (ptr + label_len > packet_end) {
            ESP_LOGE(TAG, "DNS question extends beyond packet");
            return -1;
        }
        
        // Add dot separator (except for first label)
        if (!first_label) {
            if (domain_pos < domain_size - 1) {
                domain[domain_pos++] = '.';
            }
        }
        first_label = false;
        
        // Copy label to domain string
        for (int i = 0; i < label_len && domain_pos < domain_size - 1; i++) {
            domain[domain_pos++] = (char)ptr[i];
        }
        
        ptr += label_len;
    }
    
    if (ptr >= packet_end) {
        ESP_LOGE(TAG, "DNS question malformed - no null terminator");
        return -1;
    }
    
    domain[domain_pos] = '\0';
    ptr++; // Skip null terminator
    
    // Get query type and class
    if (ptr + 4 > packet_end) {
        ESP_LOGE(TAG, "DNS question too short for type/class");
        return -1;
    }
    
    uint16_t qtype = ntohs(*(uint16_t *)ptr);
    uint16_t qclass = ntohs(*(uint16_t *)(ptr + 2));
    
    ESP_LOGD(TAG, "DNS query: domain='%s', type=%d, class=%d", domain, qtype, qclass);
    
    return qtype;
}

// Create DNS response packet
static size_t create_dns_response(const uint8_t *query_packet, size_t query_len, 
                                  uint8_t *response_packet, size_t response_max_len,
                                  uint32_t redirect_ip) {
    if (query_len < DNS_HEADER_SIZE || response_max_len < query_len + sizeof(dns_answer_t)) {
        ESP_LOGE(TAG, "Invalid packet sizes for DNS response");
        return 0;
    }
    
    // Copy original query packet as base
    memcpy(response_packet, query_packet, query_len);
    
    // Modify header to make it a response
    dns_header_t *header = (dns_header_t *)response_packet;
    header->flags = htons(DNS_QUERY_FLAG_RESPONSE | DNS_QUERY_FLAG_AUTHORITATIVE | DNS_QUERY_FLAG_RECURSION_DESIRED);
    header->answers = htons(1);
    header->authority_rrs = 0;
    header->additional_rrs = 0;
    
    // Add answer section at the end of the original query
    dns_answer_t *answer = (dns_answer_t *)(response_packet + query_len);
    answer->name_ptr = htons(0xC00C);  // Pointer to name in question (compression)
    answer->type = htons(1);           // A record
    answer->class = htons(1);          // IN class
    answer->ttl = htonl(60);           // 60 seconds TTL
    answer->data_length = htons(4);    // IPv4 address length
    answer->ip_address = redirect_ip;  // Already in network byte order
    
    return query_len + sizeof(dns_answer_t);
}

// Define quantos requests DNS pode responder por segundo
#define DNS_MAX_RESP_PER_SEC 40

static uint32_t rl_window_start_ms = 0;
static uint16_t rl_count_in_window = 0;
static TaskHandle_t dns_task_handle = NULL;

// Helper to decide if we should answer (drops excess to reduce load)
static bool dns_rate_allowed(void) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000); // current time in ms
    if (rl_window_start_ms == 0 || (now - rl_window_start_ms) >= 1000) {
        rl_window_start_ms = now;
        rl_count_in_window = 0;
    }
    if (rl_count_in_window >= DNS_MAX_RESP_PER_SEC) return false;
    rl_count_in_window++;
    return true;
}

// Main DNS server function
static void dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    uint8_t packet_buffer[DNS_MAX_PACKET_SIZE];
    uint8_t response_buffer[DNS_MAX_PACKET_SIZE];
    char domain_name[256];

    // Get the AP IP address to redirect to
    uint32_t ap_ip = get_ap_ip_address();
    
    // Create UDP socket
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_socket < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket: errno=%d", errno);
        goto cleanup;
    }
    
    // Set socket options for reuse
    int opt = 1;
    if (setsockopt(dns_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: errno=%d", errno);
    }
    
    // Bind socket to DNS port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DNS_PORT);
    
    if (bind(dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket: errno=%d", errno);
        goto cleanup;
    }
    
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(dns_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    ESP_LOGI(TAG, "Servidor DNS iniciado na porta %d, redirecionando todas as consultas para " IPSTR,
             DNS_PORT, IP2STR((esp_ip4_addr_t*)&ap_ip));
    
    // Main DNS server loop
    while (1) {
        client_addr_len = sizeof(client_addr);
        
        // Receive DNS query
        ssize_t packet_len = recvfrom(dns_socket, packet_buffer, sizeof(packet_buffer), 0,
                                      (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (packet_len < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, try again
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout (SO_RCVTIMEO). Not a fatal error; just loop again.
                // Could add housekeeping here if needed.
                continue;
            }
            ESP_LOGE(TAG, "DNS recvfrom fatal error: errno=%d", errno);
            break;
        }
        
        if (packet_len < DNS_HEADER_SIZE) {
            ESP_LOGD(TAG, "DNS packet too short: %d bytes", (int)packet_len);
            continue;
        }
        
        // Parse the DNS query
        domain_name[0] = '\0';
        int query_type = parse_dns_query(packet_buffer, packet_len, domain_name, sizeof(domain_name));
        
        if (query_type < 0) {
            ESP_LOGD(TAG, "Failed to parse DNS query from %s", inet_ntoa(client_addr.sin_addr));
            continue;
        }
        
        ESP_LOGD(TAG, "DNS query from %s: '%s' (type %d)",
                 inet_ntoa(client_addr.sin_addr), domain_name, query_type);
        
        // Only respond to A record queries (type 1)
        if (query_type != 1) {
            ESP_LOGD(TAG, "Ignoring non-A record query for '%s' (type %d)", domain_name, query_type);
            continue;
        }
        
        if (!dns_rate_allowed()) {
            ESP_LOGD(TAG, "DNS rate limit drop for '%s'", domain_name);
            continue;
        }
        
        // Create response packet
        size_t response_len = create_dns_response(packet_buffer, packet_len, 
                                                  response_buffer, sizeof(response_buffer), ap_ip);
        
        if (response_len == 0) {
            ESP_LOGE(TAG, "Failed to create DNS response for '%s'", domain_name);
            continue;
        }
        
        // Send DNS response
        ssize_t sent_bytes = sendto(dns_socket, response_buffer, response_len, 0,
                                    (struct sockaddr *)&client_addr, client_addr_len);
        
        if (sent_bytes != response_len) {
            ESP_LOGW(TAG, "DNS response send error: sent %d of %d bytes (errno=%d)",
                     (int)sent_bytes, (int)response_len, errno);
        } else {
            ESP_LOGD(TAG, "DNS response sent: '%s' -> " IPSTR " (%d bytes)",
                     domain_name, IP2STR((esp_ip4_addr_t*)&ap_ip), (int)response_len);
        }
        
        // Brief delay to prevent overwhelming the log
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
cleanup:
    if (dns_socket >= 0) {
        close(dns_socket);
        dns_socket = -1;
    }
    dns_task_handle = NULL;   // <--- mark free
    ESP_LOGI(TAG, "DNS server task ended");
    vTaskDelete(NULL);
}

// Public function to start the captive DNS server (called from main app)
void captive_dns_task(void *pv) {
    if (dns_task_handle) {
        ESP_LOGW(TAG, "Captive DNS already running");
        return;
    }
    // Launch actual server in this same task context (original behavior) OR create new task.
    // Keep original behavior (blocking) but mark handle:
    dns_task_handle = xTaskGetCurrentTaskHandle();
    vTaskDelay(pdMS_TO_TICKS(2000));
    dns_server_task(pv);
}

// Optional: Function to stop DNS server (if needed for cleanup)
void captive_dns_stop(void) {
    if (dns_socket >= 0) {
        ESP_LOGI(TAG, "Stopping captive DNS server...");
        shutdown(dns_socket, SHUT_RDWR);
        close(dns_socket);
        dns_socket = -1;
    }
}