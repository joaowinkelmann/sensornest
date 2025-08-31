idf_component_register(SRCS "tempmesh.c"
                            "rest_server.c"
                            "rest_client.c"
                            "captive_dns.c"
                            "certs.c"
                            "sensor.c"
                            "batch_store.c"
                            "role_manager.c"
                            "worker_role.c"
                            "controller_role.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_timer
                    REQUIRES esp_wifi esp_event nvs_flash esp_netif esp_http_client esp_http_server json dht littlefs)

if(CONFIG_EXAMPLE_DEPLOY_WEB_PAGES)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front")
    set(WEB_DIST_DIR "${WEB_SRC_DIR}/dist")

    execute_process(
        COMMAND bun --version
        WORKING_DIRECTORY ${WEB_SRC_DIR}
        RESULT_VARIABLE BUN_FOUND
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(BUN_FOUND EQUAL 0)
        set(FRONT_BUILD_CMD bun run build)
    else()
        set(FRONT_BUILD_CMD npm run build)
    endif()

    add_custom_target(build_frontend ALL
        COMMAND ${FRONT_BUILD_CMD}
        WORKING_DIRECTORY ${WEB_SRC_DIR}
        COMMENT "Building frontend assets..."
        VERBATIM
    )

    littlefs_create_partition_image(www ${WEB_DIST_DIR} FLASH_IN_PROJECT DEPENDS build_frontend)
endif()