#pragma once

#include "../../src/plugininterface.h"
#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QThread>
#include <QLabel>
#include <QTabWidget>
#include <atomic>
#include <thread>
#include <mutex>

class GUIPlugin : public PluginInterface {
public:
    GUIPlugin();
    ~GUIPlugin() override;
    
    // PluginInterface implementation
    std::string getName() const override;
    std::string getVersion() const override;
    std::string getDescription() const override;
    std::string getAuthor() const override;
    
    bool initialize() override;
    void shutdown() override;
    
    bool handleCommand(std::queue<std::string>& args) override;
    std::vector<std::string> getCommands() const override;
    
    std::map<std::string, std::string> getDefaultSettings() const override;
    void updateSetting(const std::string& key, const std::string& value) override;

private:
    void startGUI();
    void stopGUI();
    
    std::thread guiThread;
    std::atomic<bool> isRunning;
    std::mutex guiMutex;
    
    QApplication* app;
    QMainWindow* mainWindow;
    
    // Settings
    std::map<std::string, std::string> settings;
};
