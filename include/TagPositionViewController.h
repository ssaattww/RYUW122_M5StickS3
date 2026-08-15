#pragma once

#include <cstdint>

#include "RunMode.h"

/**
 * @brief TAG画面で選択中の表示ページを表します。
 */
enum class EnTagPositionDisplayPage : uint8_t
{
    RangingResults,
    PositionGraph,
};

/**
 * @brief BtnA入力と動作モードからTAG表示ページを管理します。
 */
class TagPositionViewController
{
public:
    /**
     * @brief 現在の動作モードとBtnA押下を表示ページへ反映します。
     *
     * @param mode 現在の動作モード
     * @param buttonAPressed BtnAが新たに押された場合はtrue
     * @return 表示ページが変化した場合はtrue
     */
    bool Update(EnRunMode mode, bool buttonAPressed);

    /**
     * @brief 現在選択中の表示ページを取得します。
     *
     * @return 現在の表示ページ
     */
    EnTagPositionDisplayPage GetPage() const;

private:
    EnTagPositionDisplayPage m_page =
        EnTagPositionDisplayPage::RangingResults;
};
