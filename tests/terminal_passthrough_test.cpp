#include <gtest/gtest.h>
#include "terminalpassthrough.h"

// This is a basic test suite for TerminalPassthrough
TEST(TerminalPassthroughTest, Initialization) {
    // Arrange
    TerminalPassthrough terminal;
    
    // Act & Assert - just verify the constructor doesn't throw
    SUCCEED();
}

TEST(TerminalPassthroughTest, CommandSplitting) {
    // Arrange
    TerminalPassthrough terminal;
    
    // Act
    std::queue<std::string> args = terminal.splitCommand("echo hello world");
    
    // Assert
    ASSERT_EQ(args.size(), 3);
    EXPECT_EQ(args.front(), "echo");
    args.pop();
    EXPECT_EQ(args.front(), "hello");
    args.pop();
    EXPECT_EQ(args.front(), "world");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
