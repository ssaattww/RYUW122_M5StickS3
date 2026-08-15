#include "TagPositionViewController.h"

/**
 * @brief 現在の動作モードとBtnA押下を表示ページへ反映します。
 *
 * @param mode 現在の動作モード
 * @param buttonAPressed BtnAが新たに押された場合はtrue
 * @return 表示ページが変化した場合はtrue
 */
bool TagPositionViewController::Update(
    EnRunMode mode,
    bool buttonAPressed)
{
    if (mode != EnRunMode::Tag)
    {
        if (m_page == EnTagPositionDisplayPage::RangingResults)
        {
            return false;
        }
        m_page = EnTagPositionDisplayPage::RangingResults;
        return true;
    }
    if (!buttonAPressed)
    {
        return false;
    }

    m_page = m_page == EnTagPositionDisplayPage::RangingResults
        ? EnTagPositionDisplayPage::PositionGraph
        : EnTagPositionDisplayPage::RangingResults;
    return true;
}

/**
 * @brief 現在選択中の表示ページを取得します。
 *
 * @return 現在の表示ページ
 */
EnTagPositionDisplayPage TagPositionViewController::GetPage() const
{
    return m_page;
}
