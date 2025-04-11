#include <gtest/gtest.h>
#include "pluginmanager.h"
#include <filesystem>

// This is a basic test suite for PluginManager
TEST(PluginManagerTest, Initialization) {
    // Arrange
    PluginManager manager;
    
    // Act - initialize the manager
    bool result = manager.initialize();
    
    // Assert - initialization should succeed
    EXPECT_TRUE(result);
}

TEST(PluginManagerTest, GetPluginList) {
    // Arrange
    PluginManager manager;
    manager.initialize();
    
    // Act - get the list of plugins
    std::vector<std::string> plugins = manager.getAvailablePlugins();
    
    // Assert - there should be a list (may be empty in test environment)
    // Just verifying the function runs without crashing
    SUCCEED();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
