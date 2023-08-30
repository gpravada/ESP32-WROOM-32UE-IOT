idf_component_register(
    SRCS bmp280.c i2cdev.c
    INCLUDE_DIRS .
    REQUIRES log driver
)
