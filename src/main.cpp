#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include "App/WebView2App.h"
#include "UI/MainWindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    // Set console output to UTF-8
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    // Initialize COM for WebView2
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    // Create Qt application
    QApplication app(argc, argv);
    app.setApplicationName("XSocialLedger");
    app.setOrganizationName("5118Python");
    app.setApplicationVersion("1.0.0");

    qDebug() << "[INFO] XSocialLedger 启动中...";

    // Check WebView2 Runtime
    if (!WebView2App::isRuntimeInstalled()) {
        QMessageBox::critical(nullptr, "WebView2 Runtime Not Found",
            "Microsoft Edge WebView2 Runtime is required.\n\n"
            "Please install Microsoft Edge or download the WebView2 Runtime from:\n"
            "https://developer.microsoft.com/microsoft-edge/webview2/");
        return 1;
    }

    // Initialize WebView2 environment
    WebView2App::instance()->initialize([](bool success) {
        if (!success) {
            qCritical() << "[Startup] Failed to initialize WebView2 environment";
        }
    });

    // Create main window
    MainWindow* window = new MainWindow();
    window->show();

    qDebug() << "[INFO] 主窗口已显示";

    // Run Qt event loop
    int result = app.exec();

    delete window;

    // Cleanup WebView2
    WebView2App::cleanup();

#ifdef _WIN32
    CoUninitialize();
#endif

    return result;
}
