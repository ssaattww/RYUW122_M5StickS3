#pragma once

enum wifi_mode_t
{
    WIFI_STA = 1,
};

class WiFiClass
{
public:
    /**
     * @brief native testではWi-Fi mode設定を常に成功させます。
     *
     * @param mode 設定するWi-Fi mode
     * @return 常にtrue
     */
    bool mode(wifi_mode_t mode);
};

extern WiFiClass WiFi;
