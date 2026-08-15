#pragma once

#ifndef NT_SHELL_ENABLED
#define NT_SHELL_ENABLED 1
#endif

#ifndef RANGING_DIAGNOSTICS_ENABLED
#define RANGING_DIAGNOSTICS_ENABLED 0
#endif

static_assert(
    !(NT_SHELL_ENABLED && RANGING_DIAGNOSTICS_ENABLED),
    "Ranging diagnostics and NT-Shell must not share Serial");
