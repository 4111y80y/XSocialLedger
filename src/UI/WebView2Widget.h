#ifndef WEBVIEW2WIDGET_H
#define WEBVIEW2WIDGET_H

#include <QString>
#include <QWidget>
#include <WebView2.h>
#include <objbase.h>
#include <windows.h>
#include <wrl.h>


class WebView2Handler;

// Qt wrapper for WebView2
class WebView2Widget : public QWidget {
  Q_OBJECT

public:
  explicit WebView2Widget(QWidget *parent = nullptr);
  ~WebView2Widget();

  // Create browser with URL
  void CreateBrowser(const QString &url);

  // Navigate to URL
  void LoadUrl(const QString &url);

  // Execute JavaScript
  void ExecuteJavaScript(const QString &code);

  // Get handler
  WebView2Handler *GetHandler() const { return m_handler; }
  WebView2Handler *handler() const { return m_handler; }

  // Navigation
  void Reload();
  void GoBack();
  void GoForward();

  // Close browser
  void CloseBrowser();

  // Disconnect all signals for safe deletion
  void DisconnectAll();

signals:
  void browserCreated();
  void loadStarted();
  void loadFinished(bool success);
  void titleChanged(const QString &title);
  void urlChanged(const QString &url);
  void jsResultReceived(const QString &result);
  void popupBlocked(const QString &url);

  // XSocialLedger specific signals forwarded from handler
  void likeFound(const QString &jsonData);
  void replyFound(const QString &jsonData);
  void collectProgress(const QString &jsonData);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private:
  void CreateBrowserInternal(const QString &url);
  void ResizeBrowser();
  void OnControllerCreated(HRESULT result, ICoreWebView2Controller *controller);

  WebView2Handler *m_handler;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
  bool m_browserCreated;
  bool m_browserCreating;
  QString m_pendingUrl;
};

#endif // WEBVIEW2WIDGET_H
