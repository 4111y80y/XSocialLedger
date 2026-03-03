#include "ListMonitorEngine.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
#include "UI/WebView2Widget.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

ListMonitorEngine::ListMonitorEngine(WebView2Widget *browser,
                                     DataStorage *storage, QObject *parent)
    : QObject(parent), m_browser(browser), m_storage(storage), m_state(Idle),
      m_running(false), m_currentListIndex(0), m_scrollCount(0),
      m_sessionLikeCount(0),
      // 风控默认值
      m_likeIntervalMinSec(15), m_likeIntervalMaxSec(45), m_scrollMinSec(3),
      m_scrollMaxSec(8), m_listStayMinMin(3), m_listStayMaxMin(8),
      m_switchWaitMinSec(10), m_switchWaitMaxSec(30), m_maxLikesPerSession(50) {

  m_scrollTimer = new QTimer(this);
  m_scrollTimer->setSingleShot(true);
  connect(m_scrollTimer, &QTimer::timeout, this, &ListMonitorEngine::doScroll);

  m_listStayTimer = new QTimer(this);
  m_listStayTimer->setSingleShot(true);
  connect(m_listStayTimer, &QTimer::timeout, this,
          &ListMonitorEngine::switchToNextList);

  // WebView2 信号
  connect(m_browser, &WebView2Widget::loadFinished, this,
          &ListMonitorEngine::onPageLoaded);
  connect(m_browser, &WebView2Widget::webMessageReceived, this,
          &ListMonitorEngine::onWebMessage);
}

ListMonitorEngine::~ListMonitorEngine() { stop(); }

void ListMonitorEngine::start(const QStringList &listUrls) {
  if (listUrls.isEmpty()) {
    emit statusMessage(QString::fromUtf8("⚠️ 没有设置List URL，无法启动监控"));
    return;
  }

  m_listUrls = listUrls;
  m_currentListIndex = 0;
  m_sessionLikeCount = 0;
  m_likedTweetIds.clear();
  m_running = true;

  emit runningChanged(true);
  emit statusMessage(QString::fromUtf8("🔍 LIST监控启动，共 %1 个列表")
                         .arg(m_listUrls.size()));

  navigateToCurrentList();
}

void ListMonitorEngine::stop() {
  m_running = false;
  m_scrollTimer->stop();
  m_listStayTimer->stop();
  setState(Idle);
  emit runningChanged(false);
}

void ListMonitorEngine::setState(State state) { m_state = state; }

void ListMonitorEngine::setLikeInterval(int minSec, int maxSec) {
  m_likeIntervalMinSec = minSec;
  m_likeIntervalMaxSec = maxSec;
}

void ListMonitorEngine::setScrollInterval(int minSec, int maxSec) {
  m_scrollMinSec = minSec;
  m_scrollMaxSec = maxSec;
}

void ListMonitorEngine::setListStayDuration(int minMin, int maxMin) {
  m_listStayMinMin = minMin;
  m_listStayMaxMin = maxMin;
}

void ListMonitorEngine::setSwitchWait(int minSec, int maxSec) {
  m_switchWaitMinSec = minSec;
  m_switchWaitMaxSec = maxSec;
}

void ListMonitorEngine::setMaxLikesPerSession(int maxLikes) {
  m_maxLikesPerSession = maxLikes;
}

int ListMonitorEngine::randomInRange(int minVal, int maxVal) {
  if (minVal >= maxVal)
    return minVal;
  // 三角分布（高斯近似）
  int r1 = QRandomGenerator::global()->bounded(maxVal - minVal + 1);
  int r2 = QRandomGenerator::global()->bounded(maxVal - minVal + 1);
  return minVal + (r1 + r2) / 2;
}

void ListMonitorEngine::navigateToCurrentList() {
  if (!m_running || m_listUrls.isEmpty())
    return;

  QString url = m_listUrls[m_currentListIndex];
  setState(NavigatingList);
  m_scrollCount = 0;

  emit statusMessage(QString::fromUtf8("📋 导航到列表 %1/%2: %3")
                         .arg(m_currentListIndex + 1)
                         .arg(m_listUrls.size())
                         .arg(url));

  m_browser->LoadUrl(url);
}

void ListMonitorEngine::onPageLoaded(bool success) {
  if (!m_running)
    return;

  if (m_state == NavigatingList) {
    if (!success) {
      emit statusMessage(QString::fromUtf8("❌ 列表页面加载失败，5秒后重试"));
      QTimer::singleShot(5000, this, [this]() {
        if (m_running)
          navigateToCurrentList();
      });
      return;
    }

    setState(Scanning);
    emit statusMessage(QString::fromUtf8("✅ 列表加载完成，开始监控..."));

    // 启动List停留计时器
    int stayMin = randomInRange(m_listStayMinMin, m_listStayMaxMin);
    m_listStayTimer->start(stayMin * 60 * 1000);

    emit statusMessage(QString::fromUtf8("⏱️ 本列表停留 %1 分钟").arg(stayMin));

    // 等2秒让页面渲染完后开始滚动扫描
    QTimer::singleShot(2000, this, [this]() {
      if (m_running && m_state == Scanning) {
        scheduleNextScroll();
      }
    });
  }
}

void ListMonitorEngine::scheduleNextScroll() {
  if (!m_running || m_state != Scanning)
    return;

  int delay = randomInRange(m_scrollMinSec, m_scrollMaxSec);
  m_scrollTimer->start(delay * 1000);
}

void ListMonitorEngine::doScroll() {
  if (!m_running || m_state != Scanning)
    return;

  m_scrollCount++;

  // 检查是否超过最大点赞数
  if (m_sessionLikeCount >= m_maxLikesPerSession) {
    emit statusMessage(QString::fromUtf8("🛑 已达到单次最大点赞数 %1，停止监控")
                           .arg(m_maxLikesPerSession));
    stop();
    return;
  }

  // 滚动距离
  int scrollAmount = 200 + QRandomGenerator::global()->bounded(401); // 200-600

  QString scrollJs = QString(R"JS(
(function() {
    window.scrollBy({ top: %1, behavior: 'smooth' });
})();
)JS")
                         .arg(scrollAmount);

  m_browser->ExecuteJavaScript(scrollJs);

  // 滚动后扫描未点赞的帖子
  QTimer::singleShot(800, this, [this]() {
    if (m_running && m_state == Scanning) {
      injectScanScript();
    }
  });

  // 每10次滚动汇报一下
  if (m_scrollCount % 10 == 0) {
    emit statusMessage(QString::fromUtf8("📜 已滚动 %1 次 | 本次已点赞 %2/%3")
                           .arg(m_scrollCount)
                           .arg(m_sessionLikeCount)
                           .arg(m_maxLikesPerSession));
  }

  scheduleNextScroll();
}

void ListMonitorEngine::injectScanScript() {
  if (!m_running || m_state != Scanning)
    return;

  // 扫描当前可见区域的帖子，查找未点赞的
  // 排除已知的 tweetId 避免重复
  QString script = R"JS(
(function() {
    try {
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        for (let i = 0; i < articles.length; i++) {
            const article = articles[i];
            const rect = article.getBoundingClientRect();
            // 只处理可见区域的帖子
            if (rect.top < -100 || rect.bottom > window.innerHeight + 100) continue;

            // 获取帖子ID
            const timeLink = article.querySelector('a[href*="/status/"]');
            let tweetId = '';
            if (timeLink) {
                const match = timeLink.href.match(/\/status\/(\d+)/);
                if (match) tweetId = match[1];
            }
            if (!tweetId) continue;

            // 获取作者handle
            const userLinks = article.querySelectorAll('a[role="link"][href^="/"]');
            let authorHandle = '';
            for (const link of userLinks) {
                const href = link.getAttribute('href');
                if (href && /^\/[a-zA-Z0-9_]+$/.test(href)) {
                    authorHandle = href.substring(1).toLowerCase();
                    break;
                }
            }

            // 查找未点赞按钮
            const likeBtn = article.querySelector('[data-testid="like"]');
            if (!likeBtn) continue; // 已经点赞了(data-testid="unlike")或无按钮

            // 发送到C++
            try {
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'list_unliked_post',
                    index: i,
                    tweetId: tweetId,
                    handle: authorHandle
                }));
            } catch(e) {}
            return; // 一次只处理一个
        }
    } catch(e) {}
})();
)JS";

  m_browser->ExecuteJavaScript(script);
}

void ListMonitorEngine::injectLikeScript(int articleIndex) {
  if (!m_running)
    return;

  // 模拟hover + click 点赞
  QString script = QString(R"JS(
(function() {
    try {
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        if (%1 >= articles.length) return;
        const article = articles[%1];
        const likeBtn = article.querySelector('[data-testid="like"]');
        if (!likeBtn) return;

        // 模拟鼠标进入
        likeBtn.dispatchEvent(new MouseEvent('mouseenter', {bubbles: true}));
        likeBtn.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));

        setTimeout(function() {
            likeBtn.click();
            likeBtn.dispatchEvent(new MouseEvent('mouseleave', {bubbles: true}));

            try {
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'list_like_clicked'
                }));
            } catch(e) {}
        }, 200 + Math.random() * 500);
    } catch(e) {}
})();
)JS")
                       .arg(articleIndex);

  m_browser->ExecuteJavaScript(script);
}

void ListMonitorEngine::switchToNextList() {
  if (!m_running)
    return;

  m_scrollTimer->stop();
  setState(SwitchingList);

  m_currentListIndex = (m_currentListIndex + 1) % m_listUrls.size();

  int waitSec = randomInRange(m_switchWaitMinSec, m_switchWaitMaxSec);

  emit statusMessage(
      QString::fromUtf8("🔄 %1秒后切换到下一个列表...").arg(waitSec));

  QTimer::singleShot(waitSec * 1000, this, [this]() {
    if (m_running) {
      navigateToCurrentList();
    }
  });
}

void ListMonitorEngine::onWebMessage(const QString &message) {
  if (!m_running)
    return;

  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
  if (!doc.isObject())
    return;

  QJsonObject obj = doc.object();
  QString type = obj.value("type").toString();

  if (type == "list_unliked_post") {
    QString tweetId = obj.value("tweetId").toString();
    QString handle = obj.value("handle").toString();
    int index = obj.value("index").toInt();

    // 检查是否已处理
    if (m_likedTweetIds.contains(tweetId))
      return;

    // 风控：检查点赞数上限
    if (m_sessionLikeCount >= m_maxLikesPerSession) {
      emit statusMessage(QString::fromUtf8("🛑 已达最大点赞数，停止"));
      stop();
      return;
    }

    emit statusMessage(
        QString::fromUtf8("❤️ 发现 @%1 未点赞帖子，准备点赞...").arg(handle));

    // 暂停滚动
    m_scrollTimer->stop();
    setState(LikePause);

    // 随机短暂等待后点赞
    int readDelay = 500 + QRandomGenerator::global()->bounded(1500);
    QTimer::singleShot(readDelay, this, [this, handle, tweetId, index]() {
      if (!m_running)
        return;

      injectLikeScript(index);

      // 记录
      m_likedTweetIds.insert(tweetId);
      m_sessionLikeCount++;

      // 存入DataStorage - 标记为已回馈
      SocialAction action;
      action.userHandle = handle;
      action.type = "list_like";
      action.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
      action.postSnippet = "List auto-like";
      action.statusLink =
          QString("https://x.com/%1/status/%2").arg(handle, tweetId);
      action.id = SocialAction::makeId(handle, "list_like", action.timestamp);
      action.reciprocated = true; // 直接标记为已回馈
      m_storage->addAction(action);

      emit statusMessage(QString::fromUtf8("✅ 已点赞 @%1 (%2/%3)")
                             .arg(handle)
                             .arg(m_sessionLikeCount)
                             .arg(m_maxLikesPerSession));
      emit likedPost(handle, action.statusLink);

      // 点赞间隔等待后恢复扫描
      int waitSec = randomInRange(m_likeIntervalMinSec, m_likeIntervalMaxSec);

      emit statusMessage(
          QString::fromUtf8("⏳ 等待 %1 秒再继续...").arg(waitSec));

      QTimer::singleShot(waitSec * 1000, this, [this]() {
        if (!m_running)
          return;
        if (m_state == LikePause) {
          setState(Scanning);
          scheduleNextScroll();
        }
      });
    });

  } else if (type == "list_like_clicked") {
    qDebug() << "[ListMonitorEngine] Like clicked via JS";
  }
}
