#include <gtest/gtest.h>
#include "thememanager.h"

// This is a basic test suite for ThemeManager
TEST(ThemeManagerTest, Initialization) {
    // Arrange
    ThemeManager manager;
    
    // Act - initialize the manager
    bool result = manager.initialize();
    
    // Assert - initialization should succeed
    EXPECT_TRUE(result);
}

TEST(ThemeManagerTest, DefaultTheme) {
    // Arrange
    ThemeManager manager;
    manager.initialize();
    
    // Act - get the default theme
    std::string theme = manager.getCurrentTheme();
    
    // Assert - there should be a default theme
    EXPECT_FALSE(theme.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
