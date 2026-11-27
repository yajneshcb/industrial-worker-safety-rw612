/* Copyright 2026. HTTPS telemetry transport for the FRDM-RW612. */
#include "dashboard_transport.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "ca_bundle.h"
#include "fsl_debug_console.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#define HTTP_REQUEST_BUFFER_SIZE  1024U
#define HTTP_RESPONSE_BUFFER_SIZE 256U
#define HTTPS_IO_TIMEOUT_MS       5000U

static volatile uint32_t s_epochAtSync = 0U;
static volatile TickType_t s_tickAtSync = 0U;

void DASHBOARD_TimeSyncSet(uint32_t epoch_seconds)
{
    s_epochAtSync = epoch_seconds;
    s_tickAtSync = xTaskGetTickCount();
}

bool DASHBOARD_IsTimeReady(void)
{
    return time(NULL) >= (time_t)1704067200U;
}

/* lwIP SNTP supplies the UTC epoch; advance it from FreeRTOS ticks for TLS checks. */
time_t time(time_t *result)
{
    uint32_t epoch = s_epochAtSync;
    time_t current;

    if (epoch == 0U)
    {
        current = (time_t)0;
    }
    else
    {
        TickType_t elapsed = xTaskGetTickCount() - s_tickAtSync;
        current = (time_t)(epoch + (elapsed / configTICK_RATE_HZ));
    }
    if (result != NULL)
    {
        *result = current;
    }
    return current;
}

static int TcpSend(void *context, const unsigned char *buffer, size_t length)
{
    int fd = *(int *)context;
    return (int)send(fd, buffer, length, 0);
}

static int TcpReceive(void *context, unsigned char *buffer, size_t length)
{
    int fd = *(int *)context;
    return (int)recv(fd, buffer, length, 0);
}

int DASHBOARD_PostJson(const char *host, const char *path, unsigned short port,
                       const char *secret, const char *json)
{
    int result = -1;
    int fd = -1;
    int tls_ready = 0;
    int config_ready = 0;
    int random_ready = 0;
    int entropy_ready = 0;
    int ca_ready = 0;
    struct addrinfo hints = {0};
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    struct timeval timeout = { HTTPS_IO_TIMEOUT_MS / 1000U,
                               (HTTPS_IO_TIMEOUT_MS % 1000U) * 1000U };
    char port_text[6];
    char request[HTTP_REQUEST_BUFFER_SIZE];
    char response[HTTP_RESPONSE_BUFFER_SIZE];
    size_t request_length;
    size_t sent_length = 0U;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_x509_crt ca;

    if ((host == NULL) || (path == NULL) || (json == NULL))
    {
        return -1;
    }

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&random);
    mbedtls_x509_crt_init(&ca);
    entropy_ready = 1;
    random_ready = 1;
    config_ready = 1;
    tls_ready = 1;
    ca_ready = 1;

    result = mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, NULL, 0U);
    if (result != 0)
    {
        goto cleanup;
    }
    result = mbedtls_x509_crt_parse(&ca, (const unsigned char *)g_app_ca_bundle, g_app_ca_bundle_len);
    if (result != 0)
    {
        goto cleanup;
    }

    (void)snprintf(port_text, sizeof(port_text), "%u", (unsigned int)port);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    result = getaddrinfo(host, port_text, &hints, &addresses);
    if ((result != 0) || (addresses == NULL))
    {
        result = -2;
        goto cleanup;
    }

    for (address = addresses; address != NULL; address = address->ai_next)
    {
        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0)
        {
            continue;
        }
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(fd, address->ai_addr, address->ai_addrlen) == 0)
        {
            break;
        }
        (void)close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    addresses = NULL;
    if (fd < 0)
    {
        result = -3;
        goto cleanup;
    }

    result = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0)
    {
        goto cleanup;
    }
    mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_ca_chain(&config, &ca, NULL);
    mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
    result = mbedtls_ssl_setup(&ssl, &config);
    if (result != 0)
    {
        goto cleanup;
    }
    result = mbedtls_ssl_set_hostname(&ssl, host);
    if (result != 0)
    {
        goto cleanup;
    }
    mbedtls_ssl_set_bio(&ssl, &fd, TcpSend, TcpReceive, NULL);
    do
    {
        result = mbedtls_ssl_handshake(&ssl);
    } while ((result == MBEDTLS_ERR_SSL_WANT_READ) || (result == MBEDTLS_ERR_SSL_WANT_WRITE));
    if (result != 0)
    {
        goto cleanup;
    }

    if ((secret != NULL) && (secret[0] != '\0'))
    {
        result = snprintf(request, sizeof(request),
                          "POST %s HTTP/1.1\r\nHost: %s\r\n"
                          "Content-Type: application/json\r\nConnection: close\r\n"
                          "X-Telemetry-Secret: %s\r\nContent-Length: %u\r\n\r\n%s",
                          path, host, secret, (unsigned int)strlen(json), json);
    }
    else
    {
        result = snprintf(request, sizeof(request),
                          "POST %s HTTP/1.1\r\nHost: %s\r\n"
                          "Content-Type: application/json\r\nConnection: close\r\n"
                          "Content-Length: %u\r\n\r\n%s",
                          path, host, (unsigned int)strlen(json), json);
    }
    if ((result < 0) || ((size_t)result >= sizeof(request)))
    {
        result = -4;
        goto cleanup;
    }
    request_length = (size_t)result;
    while (sent_length < request_length)
    {
        result = mbedtls_ssl_write(&ssl, (const unsigned char *)&request[sent_length],
                                   request_length - sent_length);
        if (result <= 0)
        {
            goto cleanup;
        }
        sent_length += (size_t)result;
    }

    result = mbedtls_ssl_read(&ssl, (unsigned char *)response, sizeof(response) - 1U);
    if (result > 0)
    {
        response[result] = '\0';
        if (sscanf(response, "HTTP/%*u.%*u %d", &result) != 1)
        {
            result = -5;
        }
    }

cleanup:
    if (addresses != NULL)
    {
        freeaddrinfo(addresses);
    }
    if (fd >= 0)
    {
        (void)close(fd);
    }
    if (tls_ready)
    {
        mbedtls_ssl_free(&ssl);
    }
    if (config_ready)
    {
        mbedtls_ssl_config_free(&config);
    }
    if (ca_ready)
    {
        mbedtls_x509_crt_free(&ca);
    }
    if (random_ready)
    {
        mbedtls_ctr_drbg_free(&random);
    }
    if (entropy_ready)
    {
        mbedtls_entropy_free(&entropy);
    }
    return result;
}
