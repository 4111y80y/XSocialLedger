#include "WebView2Widget.h"
#include "App/WebView2App.h"
#include "App/WebView2Handler.h"
#include <QCloseEvent>
#include <QDebug>
#include <QResizeEvent>
#include <QTimer>
#include <windows.h>

WebView2Widget::WebView2Widget(QWidget *parent)
    : QWidget(parent), m_handler(nullptr), m_browserCreated(false),
      m_browserCreating(false) {

  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_DontCreateNativeAncestors);

  m_handler = new WebView2Handler(this);

  connect(m_handler, &WebView2Handler::loadStarted, this,
          &WebView2Widget::loadStarted);
  connect(m_handler, &WebView2Handler::loadFinished, this,
          &WebView2Widget::loadFinished);
  connect(m_handler, &WebView2Handler::titleChanged, this,
          &WebView2Widget::titleChanged);
  connect(m_handler, &WebView2Handler::urlChanged, this,
          &WebView2Widget::urlChanged);
  connect(m_handler, &WebView2Handler::jsResultReceived, this,
          &WebView2Widget::jsResultReceived);
  connect(m_handler, &WebView2Handler::popupBlocked, this,
          &WebView2Widget::popupBlocked);

  // XSocialLedger specific signal forwarding
  connect(m_handler, &WebView2Handler::likeFound, this,
          &WebView2Widget::likeFound);
  connect(m_handler, &WebView2Handler::replyFound, this,
          &WebView2Widget::replyFound);
  connect(m_handler, &WebView2Handler::collectProgress, this,
          &WebView2Widget::collectProgress);
  connect(m_handler, &WebView2Handler::selfHandleDetected, this,
          &WebView2Widget::selfHandleDetected);
}

WebView2Widget::~WebView2Widget() { CloseBrowser(); }

void WebView2Widget::CreateBrowser(const QString &url) {
  CreateBrowserInternal(url);
}

void WebView2Widget::CreateBrowserInternal(const QString &url) {
  if (m_browserCreated) {
    LoadUrl(url);
    return;
  }

  if (m_browserCreating) {
    return;
  }

  WebView2App *app = WebView2App::instance();
  if (!app->isInitialized()) {
    m_pendingUrl = url;
    connect(
        app, &WebView2App::initialized, this,
        [this](bool success) {
          if (success) {
            CreateBrowserInternal(m_pendingUrl);
          } else {
            qCritical()
                << "[WebView2Widget] Failed to initialize WebView2 environment";
          }
        },
        Qt::UniqueConnection);
    app->initialize();
    return;
  }

  WId hwnd = winId();
  if (hwnd == 0 || width() <= 0 || height() <= 0 || !isVisible()) {
    m_pendingUrl = url;
    m_browserCreating = true;
    QTimer::singleShot(100, this, [this, url]() {
      m_browserCreating = false;
      CreateBrowserInternal(url);
    });
    return;
  }

  m_browserCreating = true;
  m_pendingUrl = url;

  ICoreWebView2Environment *env = app->environment();
  if (!env) {
    qCritical() << "[WebView2Widget] No WebView2 environment available";
    m_browserCreating = false;
    return;
  }

  env->CreateCoreWebView2Controller(
      (HWND)hwnd,
      Microsoft::WRL::Callback<
          ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
          [this](HRESULT result,
                 ICoreWebView2Controller *controller) -> HRESULT {
            OnControllerCreated(result, controller);
            return S_OK;
          })
          .Get());
}

void WebView2Widget::OnControllerCreated(HRESULT result,
                                         ICoreWebView2Controller *controller) {
  if (FAILED(result) || !controller) {
    qCritical() << "[WebView2Widget] Failed to create controller:" << Qt::hex
                << result;
    m_browserCreating = false;
    return;
  }

  m_controller = controller;

  ICoreWebView2 *webview = nullptr;
  controller->get_CoreWebView2(&webview);

  if (!webview) {
    qCritical() << "[WebView2Widget] Failed to get webview from controller";
    m_browserCreating = false;
    return;
  }

  m_handler->attach(controller, webview);

  ICoreWebView2Settings *settings = nullptr;
  webview->get_Settings(&settings);
  if (settings) {
    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);
    settings->put_AreDevToolsEnabled(TRUE);
    settings->put_AreDefaultContextMenusEnabled(TRUE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_IsBuiltInErrorPageEnabled(TRUE);
    settings->Release();
  }

  if (!m_pendingUrl.isEmpty()) {
    webview->Navigate(m_pendingUrl.toStdWString().c_str());
  }

  webview->Release();

  qDebug() << "[WebView2Widget] Controller created, URL:" << m_pendingUrl;

  m_browserCreated = true;
  m_browserCreating = false;
  ResizeBrowser();
  emit browserCreated();
}

void WebView2Widget::LoadUrl(const QString &url) {
  if (m_handler && m_handler->webview()) {
    m_handler->webview()->Navigate(url.toStdWString().c_str());
  } else {
    m_pendingUrl = url;
  }
}

void WebView2Widget::ExecuteJavaScript(const QString &code) {
  if (m_handler) {
    m_handler->executeJavaScript(code);
  }
}

void WebView2Widget::Reload() {
  if (m_handler && m_handler->webview()) {
    m_handler->webview()->Reload();
  }
}

void WebView2Widget::GoBack() {
  if (m_handler && m_handler->webview()) {
    m_handler->webview()->GoBack();
  }
}

void WebView2Widget::GoForward() {
  if (m_handler && m_handler->webview()) {
    m_handler->webview()->GoForward();
  }
}

void WebView2Widget::CloseBrowser() {
  if (m_controller) {
    m_controller->Close();
    m_controller.Reset();
  }
  if (m_handler) {
    m_handler->detach();
  }
  m_browserCreated = false;
  m_browserCreating = false;
}

void WebView2Widget::DisconnectAll() {
  if (m_handler) {
    m_handler->disconnect(this);
  }
  this->disconnect();
}

void WebView2Widget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  ResizeBrowser();
}

void WebView2Widget::closeEvent(QCloseEvent *event) {
  CloseBrowser();
  QWidget::closeEvent(event);
}

void WebView2Widget::ResizeBrowser() {
  if (!m_controller) {
    return;
  }

  RECT bounds;
  bounds.left = 0;
  bounds.top = 0;
  bounds.right = width();
  bounds.bottom = height();

  m_controller->put_Bounds(bounds);
}
