#ifndef WEBVIEW2HANDLER_H
#define WEBVIEW2HANDLER_H

#include <QObject>
#include <QString>
#include <WebView2.h>
#include <objbase.h>
#include <windows.h>
#include <wrl.h>

// WebView2 event handler - converts WebView2 events to Qt signals
class WebView2Handler : public QObject {
  Q_OBJECT

public:
  explicit WebView2Handler(QObject *parent = nullptr);
  ~WebView2Handler();

  // Attach to a WebView2 controller/webview
  void attach(ICoreWebView2Controller *controller, ICoreWebView2 *webview);

  // Detach from current webview
  void detach();

  // Get WebView2 interfaces
  ICoreWebView2 *webview() const { return m_webview.Get(); }
  ICoreWebView2Controller *controller() const { return m_controller.Get(); }

  // Execute JavaScript
  void executeJavaScript(const QString &code);

  // Check if webview is valid
  bool isValid() const { return m_webview != nullptr; }

signals:
  void browserCreated();
  void browserClosed();
  void loadStarted();
  void loadFinished(bool success);
  void titleChanged(const QString &title);
  void urlChanged(const QString &url);
  void jsResultReceived(const QString &result);
  void popupBlocked(const QString &url);

  // XSocialLedger specific signals
  void likeFound(const QString &jsonData);
  void replyFound(const QString &jsonData);
  void collectProgress(const QString &jsonData);
  void selfHandleDetected(const QString &handle);
  void webMessageReceived(const QString &message);

private:
  void setupEventHandlers();
  void removeEventHandlers();
  void processConsoleMessage(const QString &message);

  Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
  Microsoft::WRL::ComPtr<ICoreWebView2> m_webview;

  // Event registration tokens
  EventRegistrationToken m_navigationStartingToken = {};
  EventRegistrationToken m_navigationCompletedToken = {};
  EventRegistrationToken m_sourceChangedToken = {};
  EventRegistrationToken m_documentTitleChangedToken = {};
  EventRegistrationToken m_newWindowRequestedToken = {};
  EventRegistrationToken m_webMessageReceivedToken = {};
  EventRegistrationToken m_acceleratorKeyPressedToken = {};
};

#endif // WEBVIEW2HANDLER_H
