set(srcs "main.c"
         "config.c"
         "DS1803.c"
         "webserver.c"
         "GV.c")
idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
