# Toyota_Remote_Controller

SDK provides simple LwM2M client for remote control of automotive sensors and relays.

                                            SUPPORTED OBJECTS

Firmware update:

    Object provides FOTA and can be used as mechanism of firmware update by remote server.
    To use this feature, you must to build anjay-library with current flags:
    WITH_BLOCK_DOWNLOAD "Enable support for CoAP(S) downloads" ON "WITH_DOWNLOADER" ON.
    WITH_HTTP_DOWNLOAD "Enable support for HTTP(S) downloads" ON "WITH_DOWNLOADER" ON.

    Also you need to install the library by next commands:

    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make clean install

Headlights control:

    Object provides remote control of car headlights. Also you can regulate tilt angle of
    headlight chassis. Headlights brightness is about 50 percents by default and it can be
    regulated in range from 0 to 100 percents. Headlights control state is disabled by default.

Humidity control:

    Object provides remote control of car humidity sensor. Default humidity value is 35 percents.
    It can be regulated in range from 0 to 40 percents. Humidity sensor is disable by default.
