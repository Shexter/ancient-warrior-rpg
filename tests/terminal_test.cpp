#include "platform/terminal.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace heroes::platform;

// Every mode the game turns on is turned off again when it leaves (the restore paths all write
// the same sequence).
TEST(Terminal, LeavingUndoesEveryModeEntered) {
    for (const char* mode : {"1049", "25", "1000", "1006"}) {
        const std::string number = std::string("\x1b[?") + mode;
        const bool hides = std::string(mode) == "25"; // the cursor is hidden on entry, shown on exit
        EXPECT_NE(kEnterSequence.find(number + (hides ? "l" : "h")), std::string::npos) << mode;
        EXPECT_NE(kLeaveSequence.find(number + (hides ? "h" : "l")), std::string::npos) << mode;
    }
}
