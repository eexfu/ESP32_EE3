idf_component_register(
    INCLUDE_DIRS include
    SRCS src/rc522.c
    REQUIRES esp_event driver
    REQUIRES esp_event esp_timer
)
