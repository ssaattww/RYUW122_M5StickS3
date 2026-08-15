#pragma once

#include <cstdint>

#include "RunMode.h"

/**
 * @brief production task controller用の表示ページstubです。
 */
enum class EnTagPositionDisplayPage : uint8_t
{
    RangingResults,
    PositionGraph,
};

/**
 * @brief production task controller用のページ管理stubです。
 */
class TagPositionViewController
{
public:
    /**
     * @brief testではBtnA入力なしのため表示ページを維持します。
     *
     * @param mode 現在モード
     * @param buttonAPressed BtnA押下状態
     * @return ページ変更時はtrue
     */
    bool Update(EnRunMode mode, bool buttonAPressed)
    {
        static_cast<void>(buttonAPressed);
        if (mode != EnRunMode::Tag &&
            m_page != EnTagPositionDisplayPage::RangingResults)
        {
            m_page = EnTagPositionDisplayPage::RangingResults;
            return true;
        }
        return false;
    }

    /**
     * @brief 現在ページを取得します。
     *
     * @return 現在ページ
     */
    EnTagPositionDisplayPage GetPage() const
    {
        return m_page;
    }

private:
    EnTagPositionDisplayPage m_page =
        EnTagPositionDisplayPage::RangingResults;
};
