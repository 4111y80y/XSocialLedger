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
  connect(m_browser, &WebView2Widget::collectProgress, this,
          &NotificationCollector::onCollectProgress);
  connect(
      m_browser, &WebView2Widget::selfHandleDetected, this,
      [this](const QString &handle) {
        m_storage->setSelfHandle(handle);
        int removed = m_storage->removeByHandle(handle);
        if (removed > 0) {
          emit selfRecordsCleaned(removed);
          emit statusMessage(
              QString("已清理 %1 条自己的记录 (@%2)").arg(removed).arg(handle));
        }
      });
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

    // 自动检测当前登录用户的 handle（排除自己）— 多种方式兜底
    let myHandle = '';

    function detectMyHandle() {
        if (myHandle) return myHandle;

        // 方法1: data-testid
        const profileLink = document.querySelector('a[data-testid="AppTabBar_Profile_Link"]');
        if (profileLink) {
            const href = profileLink.getAttribute('href') || '';
            if (href.match(/^\/[a-zA-Z0-9_]+$/)) {
                myHandle = href.replace('/', '').toLowerCase();
                console.log('[SELF_HANDLE]' + myHandle);
                return myHandle;
            }
        }

        // 方法2: nav 里的个人主页链接
        const navLinks = document.querySelectorAll('nav a[role="link"]');
        const sysRoutes = ['/i/', '/home', '/explore', '/search', '/notifications', '/messages', '/settings', '/compose', '/premium'];
        for (const link of navLinks) {
            const href = link.getAttribute('href') || '';
            if (href.match(/^\/[a-zA-Z0-9_]+$/) && !sysRoutes.some(r => href.startsWith(r))) {
                myHandle = href.replace('/', '').toLowerCase();
                console.log('[SELF_HANDLE]' + myHandle);
                return myHandle;
            }
        }

        // 方法3: 从 "Replying to @XXX" 文本提取（最可靠！因为通知页必有此文本）
        const allLinks = document.querySelectorAll('a[role="link"]');
        for (const link of allLinks) {
            const href = link.getAttribute('href') || '';
            if (!href.match(/^\/[a-zA-Z0-9_]+$/)) continue;
            // 检查此链接前面是否有 "Replying to" 文本
            const parent = link.closest('div');
            if (parent && parent.textContent && parent.textContent.includes('Replying to')) {
                // 这个通知页的 "Replying to @XXX" 中的 XXX 就是自己
                const handle = href.replace('/', '').toLowerCase();
                // 统计此 handle 出现在 "Replying to" 中的次数
                const replyingTexts = document.querySelectorAll('div[dir="ltr"]');
                let count = 0;
                for (const rt of replyingTexts) {
                    if (rt.textContent && rt.textContent.includes('Replying to') && rt.textContent.includes('@' + handle)) {
                        count++;
                    }
                }
                // 如果多次出现在 "Replying to" 中，很可能是自己
                if (count >= 2) {
                    myHandle = handle;
                    console.log('[SELF_HANDLE]' + myHandle);
                    return myHandle;
                }
            }
        }

        // 方法4: 从 cookie 中提取 screen_name (twid)
        try {
            const cookies = document.cookie;
            const twidMatch = cookies.match(/twid=u%3D(\d+)/);
            if (twidMatch) {
                // 有 twid 但无法直接获取 screen_name
                // 尝试从页面 __NEXT_DATA__ 或 meta 标签获取
                const metaTag = document.querySelector('meta[property="al:android:url"]');
                if (metaTag) {
                    const content = metaTag.getAttribute('content') || '';
                    const match = content.match(/screen_name=([^&]+)/);
                    if (match) {
                        myHandle = match[1].toLowerCase();
                        console.log('[SELF_HANDLE]' + myHandle);
                        return myHandle;
                    }
                }
            }
        } catch(e) {}

        return myHandle;
    }

    // 初始检测
    detectMyHandle();
    // 延迟再次检测（DOM 可能未完全加载）
    setTimeout(detectMyHandle, 3000);
    setTimeout(detectMyHandle, 8000);

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

void NotificationCollector::onCollectProgress(const QString &jsonData) {
  QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
  if (!doc.isObject())
    return;
  QJsonObject obj = doc.object();
  int found = obj["found"].toInt();
  int total = obj["total"].toInt();
  emit statusMessage(
      QString("采集中... 本次新增 %1 条，累计 %2 条").arg(found).arg(total));
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
