#pragma once

#include <string_view>

namespace EngineTests
{
    bool hasAnyTestFlag(LPWSTR cmdLine);
    int runFromCommandLine(LPWSTR cmdLine);
}
