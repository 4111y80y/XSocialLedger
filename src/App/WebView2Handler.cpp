#include "WebView2Handler.h"
#include <QDateTime>
#include <QDebug>

WebView2Handler::WebView2Handler(QObject *parent) : QObject(parent) {}

WebView2Handler::~WebView2Handler() { detach(); }

void WebView2Handler::attach(ICoreWebView2Controller *controller,
                             ICoreWebView2 *webview) {
  if (!controller || !webview) {
    qWarning() << "[WebView2Handler] Invalid controller or webview";
    return;
  }

  detach();

  m_controller = controller;
  m_webview = webview;

  setupEventHandlers();

  qDebug() << "[WebView2Handler] Attached to webview";
}

void WebView2Handler::detach() {
  if (m_webview) {
    removeEventHandlers();
  }
  m_controller.Reset();
  m_webview.Reset();
}

void WebView2Handler::setupEventHandlers() {
  if (!m_webview)
    return;

  // Navigation starting
  m_webview->add_NavigationStarting(
      Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
          [this](ICoreWebView2 *sender,
                 ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
            emit loadStarted();
            return S_OK;
          })
          .Get(),
      &m_navigationStartingToken);

  // Navigation completed
  m_webview->add_NavigationCompleted(
      Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
          [this](ICoreWebView2 *sender,
                 ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
            BOOL success;
            args->get_IsSuccess(&success);
            emit loadFinished(success != FALSE);
            return S_OK;
          })
          .Get(),
      &m_navigationCompletedToken);

  // Source changed (URL changed)
  m_webview->add_SourceChanged(
      Microsoft::WRL::Callback<ICoreWebView2SourceChangedEventHandler>(
          [this](ICoreWebView2 *sender,
                 ICoreWebView2SourceChangedEventArgs *args) -> HRESULT {
            LPWSTR uri;
            sender->get_Source(&uri);
            if (uri) {
              emit urlChanged(QString::fromWCharArray(uri));
              CoTaskMemFree(uri);
            }
            return S_OK;
          })
          .Get(),
      &m_sourceChangedToken);

  // Document title changed
  m_webview->add_DocumentTitleChanged(
      Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
          [this](ICoreWebView2 *sender, IUnknown *args) -> HRESULT {
            LPWSTR title;
            sender->get_DocumentTitle(&title);
            if (title) {
              emit titleChanged(QString::fromWCharArray(title));
              CoTaskMemFree(title);
            }
            return S_OK;
          })
          .Get(),
      &m_documentTitleChangedToken);

  // New window requested - block popups, navigate in same window
  m_webview->add_NewWindowRequested(
      Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
          [this](ICoreWebView2 *sender,
                 ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
            LPWSTR uri;
            args->get_Uri(&uri);
            QString url = uri ? QString::fromWCharArray(uri) : QString();
            if (uri)
              CoTaskMemFree(uri);

            // Block popup, navigate in current window instead
            args->put_Handled(TRUE);
            if (!url.isEmpty() && m_webview) {
              m_webview->Navigate(url.toStdWString().c_str());
            }
            emit popupBlocked(url);
            return S_OK;
          })
          .Get(),
      &m_newWindowRequestedToken);

  // Web message received (from JavaScript)
  m_webview->add_WebMessageReceived(
      Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [this](ICoreWebView2 *sender,
                 ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
            LPWSTR message;
            args->TryGetWebMessageAsString(&message);
            if (message) {
              processConsoleMessage(QString::fromWCharArray(message));
              CoTaskMemFree(message);
            }
            return S_OK;
          })
          .Get(),
      &m_webMessageReceivedToken);

  // Accelerator key pressed
  if (m_controller) {
    m_controller->add_AcceleratorKeyPressed(
        Microsoft::WRL::Callback<
            ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [this](
                ICoreWebView2Controller *sender,
                ICoreWebView2AcceleratorKeyPressedEventArgs *args) -> HRESULT {
              COREWEBVIEW2_KEY_EVENT_KIND kind;
              args->get_KeyEventKind(&kind);

              if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN ||
                  kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
                UINT key;
                args->get_VirtualKey(&key);

                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

                // Block F5 refresh
                if (key == VK_F5) {
                  args->put_Handled(TRUE);
                  return S_OK;
                }

                // Block Ctrl+key shortcuts that interfere
                if (ctrl && !alt) {
                  switch (key) {
                  case 'R':
                  case 'P':
                  case 'S':
                  case 'U':
                  case 'O':
                  case 'T':
                  case 'N':
                  case 'W':
                  case 'H':
                  case 'J':
                  case 'D':
                    args->put_Handled(TRUE);
                    return S_OK;
                  }
                }
              }

              return S_OK;
            })
            .Get(),
        &m_acceleratorKeyPressedToken);
  }

  // Inject console message bridge script
  m_webview->AddScriptToExecuteOnDocumentCreated(
      L"(function() {"
      L"  const originalLog = console.log;"
      L"  console.log = function(...args) {"
      L"    const msg = args.map(a => typeof a === 'object' ? "
      L"JSON.stringify(a) : String(a)).join(' ');"
      L"    try {"
      L"      if (window.chrome && window.chrome.webview) {"
      L"        window.chrome.webview.postMessage(msg);"
      L"      }"
      L"    } catch(e) {}"
      L"    originalLog.apply(console, args);"
      L"  };"
      L"})();",
      nullptr);
}

void WebView2Handler::removeEventHandlers() {
  if (!m_webview)
    return;

  m_webview->remove_NavigationStarting(m_navigationStartingToken);
  m_webview->remove_NavigationCompleted(m_navigationCompletedToken);
  m_webview->remove_SourceChanged(m_sourceChangedToken);
  m_webview->remove_DocumentTitleChanged(m_documentTitleChangedToken);
  m_webview->remove_NewWindowRequested(m_newWindowRequestedToken);
  m_webview->remove_WebMessageReceived(m_webMessageReceivedToken);

  if (m_controller) {
    m_controller->remove_AcceleratorKeyPressed(m_acceleratorKeyPressedToken);
  }
}

void WebView2Handler::executeJavaScript(const QString &code) {
  if (!m_webview) {
    qWarning() << "[WebView2Handler] Cannot execute JS: webview is null";
    return;
  }

  m_webview->ExecuteScript(
      code.toStdWString().c_str(),
      Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
          [this](HRESULT error, LPCWSTR result) -> HRESULT { return S_OK; })
          .Get());
}

void WebView2Handler::processConsoleMessage(const QString &msg) {
  // XSocialLedger message protocol
  if (msg.startsWith("[LIKE_FOUND]")) {
    QString jsonData = msg.mid(12);
    emit likeFound(jsonData);
    return;
  }

  if (msg.startsWith("[REPLY_FOUND]")) {
    QString jsonData = msg.mid(13);
    emit replyFound(jsonData);
    return;
  }

  if (msg.startsWith("[COLLECT_PROGRESS]")) {
    QString jsonData = msg.mid(18);
    emit collectProgress(jsonData);
    return;
  }

  if (msg.startsWith("[SELF_HANDLE]")) {
    QString handle = msg.mid(13).trimmed();
    if (!handle.isEmpty()) {
      qDebug() << "[WebView2Handler] Self handle detected:" << handle;
      emit selfHandleDetected(handle);
    }
    return;
  }

  // Check for JS result
  if (msg.startsWith("[JSRESULT]")) {
    QString result = msg.mid(10).trimmed();
    emit jsResultReceived(result);
    return;
  }

  // Debug logging
  if (msg.startsWith("[DEBUG]") || msg.startsWith("[COLLECTOR]")) {
    qDebug() << msg;
  }
}
