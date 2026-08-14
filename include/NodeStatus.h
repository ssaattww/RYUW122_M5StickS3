#pragma once

#include <Arduino.h>

#include "ConfigPreference.h"

#pragma pack(push, 1)
struct NodeStatus
{
    uint16_t anchorPositionX;
    uint16_t anchorPositionY;
    uint8_t macAddress[6];
    uint8_t nodeID;
    EnRunMode mode;
};
#pragma pack(pop)

static_assert(sizeof(NodeStatus) == sizeof(uint16_t) * 2 + sizeof(uint8_t) * 7 + sizeof(EnRunMode), "NodeStatus size mismatch");