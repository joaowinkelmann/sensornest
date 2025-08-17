idf_component_register(SRCS "tempmesh.c"
                            "rest_server.c"
                            "captive_dns.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_timer
                    REQUIRES esp_wifi esp_event nvs_flash esp_netif esp_http_client esp_http_server json dht littlefs)

if(CONFIG_EXAMPLE_DEPLOY_WEB_PAGES)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front")
    if(EXISTS ${WEB_SRC_DIR}/dist)
        littlefs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "'${WEB_SRC_DIR}/dist' doesn't exist. Please run 'pnpm build' under '${WEB_SRC_DIR}'")
    endif()
endif()