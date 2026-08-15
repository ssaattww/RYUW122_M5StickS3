#include "Ryuw122ResponseParser.h"

#include <RYUW122.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>

namespace
{
    constexpr char AnchorPrefix[] = "+ANCHOR_RCV=";
    constexpr size_t AnchorPrefixLength = sizeof(AnchorPrefix) - 1U;
    constexpr char ErrorPrefix[] = "+ERR=";
    constexpr size_t ErrorPrefixLength = sizeof(ErrorPrefix) - 1U;

    /**
     * @brief 区切り位置までの文字列を固定長領域へコピーします。
     *
     * @param begin コピー開始位置
     * @param end コピー終了位置
     * @param destination コピー先
     * @param destinationSize コピー先のbyte数
     * @return 文字列がコピー先へ収まった場合はtrue
     */
    bool CopyField(
        const char* begin,
        const char* end,
        char* destination,
        size_t destinationSize)
    {
        if (begin == nullptr || end == nullptr || end < begin ||
            destination == nullptr || destinationSize == 0U)
        {
            return false;
        }
        const size_t length = static_cast<size_t>(end - begin);
        if (length >= destinationSize)
        {
            return false;
        }
        memcpy(destination, begin, length);
        destination[length] = '\0';
        return true;
    }

    /**
     * @brief 10進整数fieldを範囲検査付きで解析します。
     *
     * @param begin field開始位置
     * @param end field終了位置
     * @param minimum 許容する最小値
     * @param maximum 許容する最大値
     * @param value 解析結果の格納先
     * @return 範囲内の整数を解析できた場合はtrue
     */
    bool ParseIntegerField(
        const char* begin,
        const char* end,
        long minimum,
        long maximum,
        long& value)
    {
        char field[24] = {};
        if (!CopyField(begin, end, field, sizeof(field)) || field[0] == '\0')
        {
            return false;
        }
        errno = 0;
        char* parsedEnd = nullptr;
        const long parsed = strtol(field, &parsedEnd, 10);
        if (errno == ERANGE || parsedEnd == field || *parsedEnd != '\0' ||
            parsed < minimum || parsed > maximum)
        {
            return false;
        }
        value = parsed;
        return true;
    }

    /**
     * @brief 距離fieldを純数値または空白付きcm表記として解析します。
     *
     * @param begin field開始位置
     * @param end field終了位置
     * @param distanceCm 解析したcm距離の格納先
     * @return 許容範囲の距離を解析できた場合はtrue
     */
    bool ParseDistanceField(
        const char* begin,
        const char* end,
        long& distanceCm)
    {
        char field[24] = {};
        if (!CopyField(begin, end, field, sizeof(field)) || field[0] == '\0')
        {
            return false;
        }
        errno = 0;
        char* parsedEnd = nullptr;
        const long parsed = strtol(field, &parsedEnd, 10);
        if (errno == ERANGE || parsedEnd == field || parsed < 0 ||
            parsed > static_cast<long>(UINT32_MAX / 10U))
        {
            return false;
        }
        if (*parsedEnd != '\0')
        {
            if (*parsedEnd != ' ' && *parsedEnd != '\t')
            {
                return false;
            }
            while (*parsedEnd == ' ' || *parsedEnd == '\t')
            {
                ++parsedEnd;
            }
            if (strcmp(parsedEnd, "cm") != 0)
            {
                return false;
            }
        }
        distanceCm = parsed;
        return true;
    }
}

bool Ryuw122ResponseParser::ParseAnchorResponse(
    const char* line,
    Ryuw122PortResponse& response)
{
    if (!IsAnchorResponseLine(line))
    {
        return false;
    }

    const char* fields[5] = {line + AnchorPrefixLength};
    const char* fieldEnds[5] = {};
    size_t fieldCount = 1U;
    const char* cursor = fields[0];
    while (fieldCount < 5U)
    {
        const char* comma = strchr(cursor, ',');
        if (comma == nullptr)
        {
            break;
        }
        fieldEnds[fieldCount - 1U] = comma;
        fields[fieldCount] = comma + 1;
        cursor = comma + 1;
        ++fieldCount;
    }
    if (strchr(cursor, ',') != nullptr || (fieldCount != 4U && fieldCount != 5U))
    {
        return false;
    }
    fieldEnds[fieldCount - 1U] = line + strlen(line);

    char tagAddress[9] = {};
    long payloadLength = 0;
    long distanceCm = 0;
    long uwbRssi = 0;
    if (!CopyField(fields[0], fieldEnds[0], tagAddress, sizeof(tagAddress)) ||
        strlen(tagAddress) != 8U ||
        !ParseIntegerField(
            fields[1], fieldEnds[1], 0, RYUW122_MAX_PAYLOAD_LENGTH,
            payloadLength) ||
        static_cast<size_t>(fieldEnds[2] - fields[2]) !=
            static_cast<size_t>(payloadLength) ||
        !ParseDistanceField(fields[3], fieldEnds[3], distanceCm) ||
        (fieldCount == 5U &&
         !ParseIntegerField(
             fields[4], fieldEnds[4], INT16_MIN, INT16_MAX, uwbRssi)))
    {
        return false;
    }

    response = Ryuw122PortResponse{};
    memcpy(response.tagAddress, tagAddress, sizeof(response.tagAddress));
    response.isSuccess = true;
    response.distanceCm = static_cast<int32_t>(distanceCm);
    response.uwbRssi = static_cast<int16_t>(uwbRssi);
    response.reason = EnRyuw122RangingReason::Success;
    return true;
}

bool Ryuw122ResponseParser::ParseErrorResponse(
    const char* line,
    Ryuw122PortResponse& response)
{
    if (line == nullptr || strncmp(line, ErrorPrefix, ErrorPrefixLength) != 0)
    {
        return false;
    }
    long code = 0;
    const char* value = line + ErrorPrefixLength;
    if (!ParseIntegerField(value, line + strlen(line), INT_MIN, INT_MAX, code))
    {
        return false;
    }
    response = Ryuw122PortResponse{};
    response.reason = EnRyuw122RangingReason::ErrorResponse;
    response.diagnosticCode = static_cast<int32_t>(code);
    return true;
}

bool Ryuw122ResponseParser::IsAnchorResponseLine(const char* line)
{
    return line != nullptr &&
        strncmp(line, AnchorPrefix, AnchorPrefixLength) == 0;
}
