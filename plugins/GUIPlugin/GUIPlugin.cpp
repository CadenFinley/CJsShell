#include "GUIPlugin.h"
#include <iostream>
#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QWidget>
#include <QLabel>
#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>

GUIPlugin::GUIPlugin() : 
    isRunning(false),
    app(nullptr),
    mainWindow(nullptr) {
    // Initialize settings with defaults
    settings = getDefaultSettings();
}

GUIPlugin::~GUIPlugin() {
    shutdown();
}

std::string GUIPlugin::getName() const {
    return "GUIPlugin";
}

std::string GUIPlugin::getVersion() const {
    return "1.0.0";
}

std::string GUIPlugin::getDescription() const {
    return "Provides a graphical user interface for DevToolsTerminal";
}

std::string GUIPlugin::getAuthor() const {
    return "Caden Finley";
}

bool GUIPlugin::initialize() {
    std::cout << "GUIPlugin: Initializing..." << std::endl;
    
    startGUI();
    return true;
}

void GUIPlugin::shutdown() {
    std::cout << "GUIPlugin: Shutting down..." << std::endl;
    stopGUI();
}

bool GUIPlugin::handleCommand(std::queue<std::string>& args) {
    if (args.empty()) return false;
    
    std::string cmd = args.front();
    args.pop();
    
    if (cmd == "show") {
        if (!isRunning) {
            startGUI();
            return true;
        }
        return false;
    }
    else if (cmd == "hide") {
        if (isRunning) {
            stopGUI();
            return true;
        }
        return false;
    }
    else if (cmd == "toggle") {
        if (isRunning) {
            stopGUI();
        } else {
            startGUI();
        }
        return true;
    }
    
    return false;
}

std::vector<std::string> GUIPlugin::getCommands() const {
    return {"gui"};
}

std::map<std::string, std::string> GUIPlugin::getDefaultSettings() const {
    return {
        {"theme", "light"},
        {"fontFamily", "Arial"},
        {"fontSize", "12"},
        {"startWithGUI", "true"},
        {"windowWidth", "800"},
        {"windowHeight", "600"}
    };
}

void GUIPlugin::updateSetting(const std::string& key, const std::string& value) {
    settings[key] = value;
    
    // Apply settings in real-time if GUI is running
    if (isRunning && mainWindow) {
        // Handle different settings
        if (key == "theme") {
            // Apply theme changes
            if (value == "dark") {
                // Apply dark theme styling
                mainWindow->setStyleSheet("background-color: #2D2D30; color: #FFFFFF;");
            } else {
                // Apply light theme styling
                mainWindow->setStyleSheet("");
            }
        }
        // Other settings can be handled similarly
    }
}

void GUIPlugin::startGUI() {
    std::lock_guard<std::mutex> lock(guiMutex);
    
    if (isRunning) return;
    
    isRunning = true;
    
    guiThread = std::thread([this]() {
        int argc = 0;
        app = new QApplication(argc, nullptr);
        
        mainWindow = new QMainWindow();
        mainWindow->setWindowTitle("DevTools Terminal GUI");
        
        int width = std::stoi(settings["windowWidth"]);
        int height = std::stoi(settings["windowHeight"]);
        mainWindow->resize(width, height);
        
        // Setup central widget with tabs
        QTabWidget* tabWidget = new QTabWidget();
        
        // Terminal Tab
        QWidget* terminalTab = new QWidget();
        QVBoxLayout* terminalLayout = new QVBoxLayout(terminalTab);
        
        QTextEdit* terminalOutput = new QTextEdit();
        terminalOutput->setReadOnly(true);
        terminalOutput->setFont(QFont(QString::fromStdString(settings["fontFamily"]), std::stoi(settings["fontSize"])));
        
        QLineEdit* terminalInput = new QLineEdit();
        
        terminalLayout->addWidget(terminalOutput);
        terminalLayout->addWidget(terminalInput);
        
        // Connect terminal input
        QObject::connect(terminalInput, &QLineEdit::returnPressed, [terminalInput, terminalOutput]() {
            QString command = terminalInput->text();
            terminalOutput->append("> " + command);
            // Here you would process the command through the PluginManager
            terminalInput->clear();
        });
        
        // AI Chat Tab
        QWidget* aiChatTab = new QWidget();
        QVBoxLayout* aiChatLayout = new QVBoxLayout(aiChatTab);
        
        QTextEdit* aiChatOutput = new QTextEdit();
        aiChatOutput->setReadOnly(true);
        
        QLineEdit* aiChatInput = new QLineEdit();
        
        aiChatLayout->addWidget(aiChatOutput);
        aiChatLayout->addWidget(aiChatInput);
        
        // Connect AI chat input
        QObject::connect(aiChatInput, &QLineEdit::returnPressed, [aiChatInput, aiChatOutput]() {
            QString message = aiChatInput->text();
            aiChatOutput->append("You: " + message);
            // Here you would send the message to the OpenAI API
            aiChatInput->clear();
        });
        
        // Settings Tab
        QWidget* settingsTab = new QWidget();
        QVBoxLayout* settingsLayout = new QVBoxLayout(settingsTab);
        
        QLabel* themeLabel = new QLabel("Theme:");
        QHBoxLayout* themeLayout = new QHBoxLayout();
        QPushButton* lightThemeBtn = new QPushButton("Light");
        QPushButton* darkThemeBtn = new QPushButton("Dark");
        
        themeLayout->addWidget(lightThemeBtn);
        themeLayout->addWidget(darkThemeBtn);
        
        // Connect theme buttons
        QObject::connect(lightThemeBtn, &QPushButton::clicked, [this]() {
            updateSetting("theme", "light");
        });
        
        QObject::connect(darkThemeBtn, &QPushButton::clicked, [this]() {
            updateSetting("theme", "dark");
        });
        
        settingsLayout->addWidget(themeLabel);
        settingsLayout->addLayout(themeLayout);
        settingsLayout->addStretch();
        
        // Add tabs to the tab widget
        tabWidget->addTab(terminalTab, "Terminal");
        tabWidget->addTab(aiChatTab, "AI Chat");
        tabWidget->addTab(settingsTab, "Settings");
        
        mainWindow->setCentralWidget(tabWidget);
        
        // Apply initial theme
        if (settings["theme"] == "dark") {
            mainWindow->setStyleSheet("background-color: #2D2D30; color: #FFFFFF;");
        }
        
        mainWindow->show();
        
        // Connect window close event to shutdown
        mainWindow->connect(mainWindow, &QMainWindow::destroyed, [this]() {
            isRunning = false;
        });
        
        app->exec();
        
        delete mainWindow;
        delete app;
        
        mainWindow = nullptr;
        app = nullptr;
        
        isRunning = false;
    });
}

void GUIPlugin::stopGUI() {
    std::lock_guard<std::mutex> lock(guiMutex);
    
    if (!isRunning) return;
    
    if (app && mainWindow) {
        mainWindow->close();
        
        if (guiThread.joinable()) {
            guiThread.join();
        }
    }
    
    isRunning = false;
}

// Plugin API implementation
IMPLEMENT_PLUGIN(GUIPlugin)
