set(srcs "main.c"
	     "connect_wifi.c"
	     "DS1803.c")
idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
