#pragma once

#include <cstdint>

enum wifi_ps_type_t
{
    WIFI_PS_NONE = 0,
    WIFI_PS_MIN_MODEM = 1,
};

enum wifi_second_chan_t
{
    WIFI_SECOND_CHAN_NONE = 0,
};

enum wifi_interface_t
{
    WIFI_IF_STA = 0,
};

int esp_wifi_set_ps(wifi_ps_type_t mode);
int esp_wifi_set_channel(uint8_t channel, wifi_second_chan_t secondChannel);
