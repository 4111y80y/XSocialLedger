#include "WebView2App.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <windows.h>


WebView2App *WebView2App::s_instance = nullptr;

WebView2App *WebView2App::instance() {
  if (!s_instance) {
    s_instance = new WebView2App();
  }
  return s_instance;
}

void WebView2App::cleanup() {
  if (s_instance) {
    delete s_instance;
    s_instance = nullptr;
  }
}

WebView2App::WebView2App(QObject *parent)
    : QObject(parent), m_initialized(false), m_initializing(false),
      m_userDataFolder(QCoreApplication::applicationDirPath() + "/userdata") {

  // Ensure user data directory exists
  QDir dir(m_userDataFolder);
  if (!dir.exists()) {
    dir.mkpath(".");
  }
}

WebView2App::~WebView2App() { m_environment.Reset(); }

bool WebView2App::isRuntimeInstalled() {
  LPWSTR version = nullptr;
  HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);

  if (SUCCEEDED(hr) && version) {
    qDebug() << "[WebView2] Runtime version:"
             << QString::fromWCharArray(version);
    CoTaskMemFree(version);
    return true;
  }

  qWarning() << "[WebView2] Runtime not installed";
  return false;
}

void WebView2App::initialize(std::function<void(bool)> callback) {
  if (m_initialized) {
    if (callback)
      callback(true);
    return;
  }

  if (m_initializing) {
    qWarning() << "[WebView2] Already initializing";
    return;
  }

  m_initializing = true;
  m_initCallback = callback;

  if (!isRuntimeInstalled()) {
    m_initializing = false;
    QString error = "WebView2 Runtime is not installed.";
    qCritical() << error;
    emit initializeFailed(error);
    if (callback)
      callback(false);
    return;
  }

  std::wstring userDataWide = m_userDataFolder.toStdWString();

  HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, userDataWide.c_str(), nullptr,
      Microsoft::WRL::Callback<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
            m_initializing = false;

            if (FAILED(result) || !env) {
              QString error =
                  QString("Failed to create WebView2 environment: 0x%1")
                      .arg(result, 8, 16, QChar('0'));
              qCritical() << error;
              emit initializeFailed(error);
              if (m_initCallback)
                m_initCallback(false);
              return S_OK;
            }

            m_environment = env;
            m_initialized = true;

            qDebug() << "[WebView2] Environment created successfully";
            emit initialized(true);
            if (m_initCallback)
              m_initCallback(true);

            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    m_initializing = false;
    QString error =
        QString("CreateCoreWebView2EnvironmentWithOptions failed: 0x%1")
            .arg(hr, 8, 16, QChar('0'));
    qCritical() << error;
    emit initializeFailed(error);
    if (callback)
      callback(false);
  }
}
