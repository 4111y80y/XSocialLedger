#include "NotificationCollector.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
#include "UI/WebView2Widget.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

NotificationCollector::NotificationCollector(WebView2Widget *browser,
                                             DataStorage *storage,
                                             QObject *parent)
    : QObject(parent), m_browser(browser), m_storage(storage),
      m_collecting(false), m_scriptInjected(false), m_scrollCount(0) {

  m_pollTimer = new QTimer(this);
  m_pollTimer->setInterval(15000); // 15秒轮询
  connect(m_pollTimer, &QTimer::timeout, this,
          &NotificationCollector::onPollTimer);

  // 连接浏览器信号
  connect(m_browser, &WebView2Widget::loadFinished, this,
          &NotificationCollector::onPageLoaded);
  connect(m_browser, &WebView2Widget::likeFound, this,
          &NotificationCollector::onLikeFound);
  connect(m_browser, &WebView2Widget::replyFound, this,
          &NotificationCollector::onReplyFound);
}

NotificationCollector::~NotificationCollector() { stopCollecting(); }

void NotificationCollector::startCollecting() {
  if (m_collecting)
    return;

  m_collecting = true;
  m_scrollCount = 0;
  emit collectingStateChanged(true);
  emit statusMessage("采集已开始...");

  // 如果页面已加载，直接注入脚本
  injectCollectorScript();

  // 启动轮询
  m_pollTimer->start();
}

void NotificationCollector::stopCollecting() {
  if (!m_collecting)
    return;

  m_collecting = false;
  m_pollTimer->stop();
  m_scriptInjected = false;
  emit collectingStateChanged(false);
  emit statusMessage("采集已停止");
}

void NotificationCollector::onPageLoaded(bool success) {
  if (!success || !m_collecting)
    return;

  qDebug() << "[Collector] Page loaded, injecting script...";
  m_scriptInjected = false;

  // 延迟注入，等页面渲染完成
  QTimer::singleShot(2000, this, [this]() {
    if (m_collecting) {
      injectCollectorScript();
    }
  });
}

void NotificationCollector::injectCollectorScript() {
  if (m_scriptInjected)
    return;

  QString script = R"JS(
(function() {
    if (window.__xsl_injected) return;
    window.__xsl_injected = true;

    const seen = new Set();

    // 自动检测当前登录用户的 handle（排除自己）
    let myHandle = '';
    const profileLink = document.querySelector('a[data-testid="AppTabBar_Profile_Link"]');
    if (profileLink) {
        const href = profileLink.getAttribute('href') || '';
        myHandle = href.replace('/', '').toLowerCase();
    }
    if (!myHandle) {
        // 备用方案：从 URL 或页面中查找
        const navLinks = document.querySelectorAll('nav a[role="link"]');
        for (const link of navLinks) {
            const href = link.getAttribute('href') || '';
            if (href.match(/^\/[a-zA-Z0-9_]+$/) && !href.startsWith('/i/') && 
                !href.startsWith('/home') && !href.startsWith('/explore') && 
                !href.startsWith('/search') && !href.startsWith('/notifications') &&
                !href.startsWith('/messages')) {
                myHandle = href.replace('/', '').toLowerCase();
                break;
            }
        }
    }
    console.log('[DEBUG] Detected my handle: @' + myHandle);

    function collectNotifications() {
        const articles = document.querySelectorAll('article[role="article"]');
        let newCount = 0;

        articles.forEach(el => {
            const text = el.innerText || '';
            const timeEl = el.querySelector('time');
            const timestamp = timeEl ? timeEl.getAttribute('datetime') : '';
            if (!timestamp) return;

            // 获取所有用户链接
            const links = Array.from(el.querySelectorAll('a[role="link"]'))
                .filter(a => {
                    const href = a.getAttribute('href') || '';
                    return href.match(/^\/[^/]+$/) && !href.startsWith('/i/') && !href.startsWith('/search');
                });

            if (links.length === 0) return;

            // 判断类型
            let type = '';
            if (text.includes('liked') || text.includes('赞了') || text.includes('いいね')) {
                type = 'like';
            } else if (text.includes('replied') || text.includes('回复') || text.includes('Replying to') || text.includes('返信')) {
                type = 'reply';
            } else if (text.includes('mentioned') || text.includes('提到') || text.includes('メンション')) {
                type = 'reply';
            } else {
                return;  // 跳过其他类型
            }

            // 获取帖子链接
            const statusEl = el.querySelector('a[href*="/status/"]');
            const statusLink = statusEl ? statusEl.href : '';

            // 获取帖子片段
            const snippet = text.substring(0, 120).replace(/\n/g, ' ');

            links.forEach(link => {
                const href = link.getAttribute('href') || '';
                const handle = href.replace('/', '');
                const name = link.innerText || handle;

                if (!handle || handle.length === 0) return;

                // ★ 排除自己的账号
                if (myHandle && handle.toLowerCase() === myHandle) return;

                const id = handle + '_' + type + '_' + timestamp;
                if (seen.has(id)) return;
                seen.add(id);

                const data = {
                    handle: handle,
                    name: name,
                    type: type,
                    timestamp: timestamp,
                    statusLink: statusLink,
                    snippet: snippet
                };

                const tag = type === 'like' ? '[LIKE_FOUND]' : '[REPLY_FOUND]';
                console.log(tag + JSON.stringify(data));
                newCount++;
            });
        });

        if (newCount > 0) {
            console.log('[COLLECT_PROGRESS]{"found":' + newCount + ',"total":' + seen.size + '}');
        }
    }

    // 初始采集
    setTimeout(collectNotifications, 1000);

    // 滚动监听
    let scrollTimer = null;
    window.addEventListener('scroll', () => {
        clearTimeout(scrollTimer);
        scrollTimer = setTimeout(collectNotifications, 800);
    });

    // 定时采集 (DOM 可能动态更新)
    setInterval(collectNotifications, 10000);

    // MutationObserver 监听新内容
    const observer = new MutationObserver((mutations) => {
        clearTimeout(scrollTimer);
        scrollTimer = setTimeout(collectNotifications, 500);
    });

    const container = document.querySelector('[aria-label]') || document.body;
    observer.observe(container, { childList: true, subtree: true });

    console.log('[DEBUG] XSocialLedger collector script injected (excluding @' + myHandle + ')');
})();
)JS";

  m_browser->ExecuteJavaScript(script);
  m_scriptInjected = true;
  qDebug() << "[Collector] Script injected";
  emit statusMessage("采集脚本已注入");
}

void NotificationCollector::onLikeFound(const QString &jsonData) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
  if (!doc.isObject())
    return;

  QJsonObject obj = doc.object();
  SocialAction action;
  action.userHandle = obj["handle"].toString();
  action.userName = obj["name"].toString();
  action.type = "like";
  action.timestamp = obj["timestamp"].toString();
  action.postSnippet = obj["snippet"].toString();
  action.statusLink = obj["statusLink"].toString();
  action.reciprocated = false;
  action.id =
      SocialAction::makeId(action.userHandle, action.type, action.timestamp);

  if (m_storage->addAction(action)) {
    qDebug() << "[Collector] New like from" << action.userHandle << "at"
             << action.timestamp;
    emit newLikeCollected(action.userName.isEmpty() ? action.userHandle
                                                    : action.userName,
                          action.timestamp);
  }
}

void NotificationCollector::onReplyFound(const QString &jsonData) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
  if (!doc.isObject())
    return;

  QJsonObject obj = doc.object();
  SocialAction action;
  action.userHandle = obj["handle"].toString();
  action.userName = obj["name"].toString();
  action.type = "reply";
  action.timestamp = obj["timestamp"].toString();
  action.postSnippet = obj["snippet"].toString();
  action.statusLink = obj["statusLink"].toString();
  action.reciprocated = false;
  action.id =
      SocialAction::makeId(action.userHandle, action.type, action.timestamp);

  if (m_storage->addAction(action)) {
    qDebug() << "[Collector] New reply from" << action.userHandle << "at"
             << action.timestamp;
    emit newReplyCollected(action.userName.isEmpty() ? action.userHandle
                                                     : action.userName,
                           action.timestamp);
  }
}

void NotificationCollector::onPollTimer() {
  if (!m_collecting)
    return;

  // 触发滚动以加载更多通知
  triggerScroll();
}

void NotificationCollector::triggerScroll() {
  // 每次轮询时触发一次向下滚动
  m_scrollCount++;

  QString scrollScript = R"JS(
    (function() {
        window.scrollBy(0, window.innerHeight * 0.8);
        // 采集当前可见的通知
        if (typeof collectNotifications === 'function') {
            setTimeout(collectNotifications, 1000);
        }
    })();
    )JS";

  m_browser->ExecuteJavaScript(scrollScript);
}
