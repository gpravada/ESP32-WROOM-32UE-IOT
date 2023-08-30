idf_component_register(
    SRCS bmp280.c bmp_app.c i2cdev.c
    INCLUDE_DIRS "include"
    REQUIRES log driver
)
