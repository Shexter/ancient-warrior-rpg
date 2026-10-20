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

TEST(Terminal, KittyPopIsWrittenOnlyAfterAPush) {
    EXPECT_EQ(leaveSequence(false), kLeaveSequence);
    EXPECT_EQ(leaveSequence(true), std::string(kKittyPop) + std::string(kLeaveSequence));
    EXPECT_EQ(leaveSequence(false).find(kKittyPop), std::string_view::npos);
}

TEST(Terminal, KittyProbeReadsTheReplies) {
    EXPECT_EQ(probeKittyReply(""), KittyProbe::Waiting);
    EXPECT_EQ(probeKittyReply("\x1b[?0u"), KittyProbe::Waiting);
    EXPECT_EQ(probeKittyReply("\x1b[?0u\x1b[?62"), KittyProbe::Waiting); // reply still arriving
    EXPECT_EQ(probeKittyReply("\x1b[?0u\x1b[?62;22c"), KittyProbe::Supported);
    EXPECT_EQ(probeKittyReply("\x1b[?62;22c"), KittyProbe::Unsupported);
    EXPECT_EQ(probeKittyReply("w\x1b[?1u q\x1b[?64;1c"), KittyProbe::Supported); // typed keys
}
