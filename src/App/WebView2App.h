#ifndef WEBVIEW2APP_H
#define WEBVIEW2APP_H

#include <QObject>
#include <QString>
#include <WebView2.h>
#include <functional>
#include <objbase.h>
#include <windows.h>
#include <wrl.h>


// WebView2 environment singleton manager
class WebView2App : public QObject {
  Q_OBJECT

public:
  static WebView2App *instance();
  static void cleanup();

  // Initialize WebView2 environment (async)
  void initialize(std::function<void(bool)> callback = nullptr);

  // Check if initialized
  bool isInitialized() const { return m_initialized; }

  // Get environment
  ICoreWebView2Environment *environment() const { return m_environment.Get(); }

  // User data folder root
  QString userDataFolder() const { return m_userDataFolder; }

  // Check if Edge WebView2 Runtime is installed
  static bool isRuntimeInstalled();

signals:
  void initialized(bool success);
  void initializeFailed(const QString &error);

private:
  explicit WebView2App(QObject *parent = nullptr);
  ~WebView2App();

  static WebView2App *s_instance;

  Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_environment;
  bool m_initialized;
  bool m_initializing;
  QString m_userDataFolder;
  std::function<void(bool)> m_initCallback;
};

#endif // WEBVIEW2APP_H
