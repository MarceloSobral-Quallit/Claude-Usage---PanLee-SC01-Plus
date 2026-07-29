# wifi_cred.txt (raiz do repo) e OPCIONAL: se presente, so pre-semeia uma
# rede de conveniencia para desenvolvimento (ver wifi_dev_seed.h.in e
# wifi_manager_begin()). O onboarding por toque nunca depende deste arquivo
# e o build funciona normalmente sem ele.
set(WIFI_CREDENTIALS_FILE "${CMAKE_CURRENT_LIST_DIR}/../../../wifi_cred.txt")
set(WIFI_SEED_SSID "")
set(WIFI_SEED_PASSWORD "")
if(EXISTS "${WIFI_CREDENTIALS_FILE}")
    file(STRINGS "${WIFI_CREDENTIALS_FILE}" WIFI_CREDENTIAL_LINES REGEX ".+")
    foreach(WIFI_CREDENTIAL_LINE IN LISTS WIFI_CREDENTIAL_LINES)
        if(WIFI_CREDENTIAL_LINE MATCHES "^CONFIG_WIFI_SSID=\"(.*)\"$")
            set(WIFI_SEED_SSID "${CMAKE_MATCH_1}")
        elseif(WIFI_CREDENTIAL_LINE MATCHES "^CONFIG_WIFI_PASSWORD=\"(.*)\"$")
            set(WIFI_SEED_PASSWORD "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()
string(REPLACE "\\" "\\\\" WIFI_SEED_SSID "${WIFI_SEED_SSID}")
string(REPLACE "\"" "\\\"" WIFI_SEED_SSID "${WIFI_SEED_SSID}")
string(REPLACE "\\" "\\\\" WIFI_SEED_PASSWORD "${WIFI_SEED_PASSWORD}")
string(REPLACE "\"" "\\\"" WIFI_SEED_PASSWORD "${WIFI_SEED_PASSWORD}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/wifi_dev_seed.h.in" "${CMAKE_CURRENT_BINARY_DIR}/wifi_dev_seed.h" @ONLY)

# O componente gerenciado lvgl/lvgl so adiciona "managed_components/" (o pai
# da propria pasta) ao seu INCLUDE_DIRS — nao "main/". Isso faz com que os
# arquivos internos do LVGL (ex. src/core/lv_group.c) nao encontrem
# main/lv_conf.h via #include "lv_conf.h" (LV_CONF_INCLUDE_SIMPLE), mesmo com
# CONFIG_LV_CONF_SKIP desabilitado. A correcao documentada pelo proprio
# lv_conf_internal.h e definir LV_CONF_PATH globalmente (com o caminho
# absoluto), o que tem prioridade sobre LV_CONF_INCLUDE_SIMPLE e nao depende
# de include path nenhum. idf_build_set_property com COMPILE_DEFINITIONS
# aplica a TODOS os componentes do build, nao so a este — mas o comando so
# existe na passada "cheia" do CMake, nao na passada leve de deteccao de
# REQUIRES (CMAKE_BUILD_EARLY_EXPANSION=TRUE), entao precisa do guard abaixo.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    idf_build_set_property(COMPILE_DEFINITIONS "LV_CONF_PATH=\"${CMAKE_CURRENT_LIST_DIR}/lv_conf.h\"" APPEND)
endif()

idf_component_register(
    SRCS
        "app_main.c"
        "app_state.c"
        "bsp_display.c"
        "bsp_sdcard.c"
        "bsp_debug_log.c"
        "crypto.c"
        "api.c"
        "status.c"
        "wifi_manager.c"
        "storage.c"
        "time_sync.c"
        "onboarding_server.c"
        "ui_common.c"
        "ui_onboarding.c"
        "ui_dashboard.c"
        "ui_settings.c"
        "ui_moments.c"
    INCLUDE_DIRS
        "."
        "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES
        esp_lcd
        driver
        esp_lcd_st7796
        nvs_flash
        lvgl
        fatfs
        sdmmc
        esp_driver_sdspi
        esp_driver_spi
        esp_timer
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        esp_http_server
        esp-tls
        mbedtls
        mdns
        json
        lwip
)
