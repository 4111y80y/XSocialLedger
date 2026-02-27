#include "ReciprocatorEngine.h"
#include "Data/DataStorage.h"
#include "UI/WebView2Widget.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

ReciprocatorEngine::ReciprocatorEngine(WebView2Widget *browser,
                                       DataStorage *storage, QObject *parent)
    : QObject(parent), m_browser(browser), m_storage(storage), m_state(Idle),
      m_browsing(false), m_scrollCount(0), m_countdownRemaining(0),
      m_scrollMinSec(3), m_scrollMaxSec(8), m_likeWaitMinSec(3),
      m_likeWaitMaxSec(8), m_browseMinMin(10), m_browseMaxMin(30),
      m_restMinMin(15), m_restMaxMin(45) {

  m_scrollTimer = new QTimer(this);
  m_scrollTimer->setSingleShot(true);
  connect(m_scrollTimer, &QTimer::timeout, this, &ReciprocatorEngine::doScroll);

  m_sessionTimer = new QTimer(this);
  m_sessionTimer->setSingleShot(true);
  connect(m_sessionTimer, &QTimer::timeout, this, [this]() {
    if (!m_browsing)
      return;
    if (m_state == Browsing || m_state == LikePause) {
      // 浏览时间到，开始休息
      startRestSession();
    } else if (m_state == Resting) {
      // 休息结束，重新开始浏览
      startBrowseSession();
    }
  });

  m_countdownTimer = new QTimer(this);
  m_countdownTimer->setInterval(1000);
  connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
    m_countdownRemaining--;
    if (m_countdownRemaining > 0) {
      emit sessionCountdown(m_countdownRemaining);
    } else {
      m_countdownTimer->stop();
    }
  });

  m_clickMoreTimer = new QTimer(this);
  m_clickMoreTimer->setInterval(8000); // 每8秒检查一次More按钮
  connect(m_clickMoreTimer, &QTimer::timeout, this,
          &ReciprocatorEngine::injectClickMoreScript);

  connect(m_browser, &WebView2Widget::loadFinished, this,
          &ReciprocatorEngine::onPageLoaded);
  connect(m_browser, &WebView2Widget::webMessageReceived, this,
          &ReciprocatorEngine::onWebMessage);
}

ReciprocatorEngine::~ReciprocatorEngine() { stopBrowsing(); }

void ReciprocatorEngine::startBrowsing(
    const QList<QPair<QString, QString>> &targets) {
  if (targets.isEmpty())
    return;

  // 构建目标映射
  m_targetMap.clear();
  m_likedHandles.clear();
  for (const auto &pair : targets) {
    m_targetMap[pair.first.toLower()] = pair.second;
  }

  m_browsing = true;
  m_scrollCount = 0;
  setState(NavigatingHome);

  emit browsingStateChanged("browsing");
  emit statusMessage(QString::fromUtf8("🚀 开始自动回馈浏览，%1 个待回馈用户")
                         .arg(int(m_targetMap.size())));

  // 导航到首页
  m_browser->LoadUrl("https://x.com/home");
}

void ReciprocatorEngine::stopBrowsing() {
  m_browsing = false;
  m_state = Idle;
  m_scrollTimer->stop();
  m_sessionTimer->stop();
  m_countdownTimer->stop();
  m_clickMoreTimer->stop();
  m_targetMap.clear();
  m_likedHandles.clear();
  m_scrollCount = 0;
  m_countdownRemaining = 0;
  emit browsingStateChanged("idle");
  emit batchFinished();
}

void ReciprocatorEngine::setState(State state) {
  m_state = state;
  qDebug() << "[ReciprocatorEngine] State ->" << state;
}

void ReciprocatorEngine::setScrollInterval(int minSec, int maxSec) {
  m_scrollMinSec = minSec;
  m_scrollMaxSec = maxSec;
}

void ReciprocatorEngine::setLikeWaitInterval(int minSec, int maxSec) {
  m_likeWaitMinSec = minSec;
  m_likeWaitMaxSec = maxSec;
}

void ReciprocatorEngine::setBrowseRestCycle(int browseMinMin, int browseMaxMin,
                                            int restMinMin, int restMaxMin) {
  m_browseMinMin = browseMinMin;
  m_browseMaxMin = browseMaxMin;
  m_restMinMin = restMinMin;
  m_restMaxMin = restMaxMin;
}

int ReciprocatorEngine::randomInRange(int minVal, int maxVal) {
  if (minVal >= maxVal)
    return minVal;
  return minVal + QRandomGenerator::global()->bounded(maxVal - minVal + 1);
}

void ReciprocatorEngine::onPageLoaded(bool success) {
  if (!m_browsing)
    return;

  if (m_state == NavigatingHome) {
    if (!success) {
      emit statusMessage(QString::fromUtf8("❌ 首页加载失败"));
      stopBrowsing();
      return;
    }

    emit statusMessage(QString::fromUtf8("✅ 已进入首页，开始浏览..."));

    // 等待页面渲染完成后开始浏览
    int delay = 3000 + QRandomGenerator::global()->bounded(2000);
    QTimer::singleShot(delay, this, [this]() {
      if (!m_browsing)
        return;
      startBrowseSession();
    });
  }
}

void ReciprocatorEngine::startBrowseSession() {
  if (!m_browsing)
    return;

  setState(Browsing);
  m_scrollCount = 0;
  emit browsingStateChanged("browsing");
  emit statusMessage(QString::fromUtf8("👀 浏览中..."));

  // 设置浏览时长定时器
  int browseSeconds = randomInRange(m_browseMinMin * 60, m_browseMaxMin * 60);
  m_sessionTimer->start(browseSeconds * 1000);
  m_countdownRemaining = browseSeconds;
  m_countdownTimer->start();

  // 开始自动点击More
  m_clickMoreTimer->start();

  // 先扫描一次
  injectScanScript();

  // 开始滚动
  scheduleNextScroll();
}

void ReciprocatorEngine::startRestSession() {
  if (!m_browsing)
    return;

  setState(Resting);
  m_scrollTimer->stop();
  m_clickMoreTimer->stop();
  emit browsingStateChanged("resting");

  int restSeconds = randomInRange(m_restMinMin * 60, m_restMaxMin * 60);
  m_sessionTimer->start(restSeconds * 1000);
  m_countdownRemaining = restSeconds;
  m_countdownTimer->start();

  emit statusMessage(
      QString::fromUtf8("😴 休息中... %1 分钟后继续").arg(restSeconds / 60));
}

void ReciprocatorEngine::scheduleNextScroll() {
  if (!m_browsing || m_state == Resting || m_state == LikePause)
    return;

  int delay = randomInRange(m_scrollMinSec, m_scrollMaxSec);
  m_scrollTimer->start(delay * 1000);
}

void ReciprocatorEngine::doScroll() {
  if (!m_browsing || m_state != Browsing)
    return;

  m_scrollCount++;

  // 模拟人类滚动 - 随机滚动距离
  int scrollAmount = 300 + QRandomGenerator::global()->bounded(500);
  QString scrollJs = QString(R"JS(
(function() {
    window.scrollBy({ top: %1, behavior: 'smooth' });
})();
)JS")
                         .arg(scrollAmount);

  m_browser->ExecuteJavaScript(scrollJs);

  emit statusMessage(
      QString::fromUtf8("📜 浏览中... 已滚动 %1 次").arg(m_scrollCount));

  // 滚动后等一会再扫描
  int scanDelay = 1500 + QRandomGenerator::global()->bounded(1500);
  QTimer::singleShot(scanDelay, this, [this]() {
    if (!m_browsing || m_state != Browsing)
      return;
    injectScanScript();
  });

  // 安排下一次滚动
  scheduleNextScroll();
}

QString ReciprocatorEngine::buildTargetHandlesJs() {
  QStringList handles;
  for (auto it = m_targetMap.constBegin(); it != m_targetMap.constEnd(); ++it) {
    if (!m_likedHandles.contains(it.key())) {
      handles << "'" + it.key() + "'";
    }
  }
  return "[" + handles.join(",") + "]";
}

void ReciprocatorEngine::injectScanScript() {
  if (!m_browsing || m_state == Resting)
    return;

  // 检查是否所有目标都已回馈
  if (m_likedHandles.size() >= m_targetMap.size()) {
    emit statusMessage(QString::fromUtf8("✅ 所有 %1 个用户已全部回馈！")
                           .arg(int(m_likedHandles.size())));
    stopBrowsing();
    return;
  }

  QString handlesJs = buildTargetHandlesJs();

  QString script = QString(R"JS(
(function() {
    const targets = new Set(%1);
    if (targets.size === 0) return;

    const skipPaths = ['home','explore','search','notifications','messages',
        'settings','i','compose','login','signup','tos','privacy','help',
        'about','jobs','premium','lists'];

    const articles = document.querySelectorAll('article[data-testid="tweet"]');
    for (let idx = 0; idx < articles.length; idx++) {
        const article = articles[idx];
        const likeBtn = article.querySelector('[data-testid="like"]');
        if (!likeBtn) continue;

        const links = article.querySelectorAll('a[href]');
        let author = '';
        for (const link of links) {
            const href = link.getAttribute('href');
            if (!href) continue;
            const match = href.match(/^\/([a-zA-Z0-9_]+)$/);
            if (!match) continue;
            const username = match[1].toLowerCase();
            if (skipPaths.includes(username)) continue;
            author = username;
            break;
        }

        if (author && targets.has(author)) {
            article.scrollIntoView({ behavior: 'smooth', block: 'center' });
            try {
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'reciprocate_target',
                    handle: author,
                    index: idx
                }));
            } catch(e) {}
            return;
        }
    }
})();
)JS")
                       .arg(handlesJs);

  m_browser->ExecuteJavaScript(script);
}

void ReciprocatorEngine::injectLikeScript(int articleIndex) {
  QString script = QString(R"JS(
(function() {
    const articles = document.querySelectorAll('article[data-testid="tweet"]');
    if (%1 < articles.length) {
        const likeBtn = articles[%1].querySelector('[data-testid="like"]');
        if (likeBtn) {
            likeBtn.click();
            try {
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'like_clicked'
                }));
            } catch(e) {}
        }
    }
})();
)JS")
                       .arg(articleIndex);

  m_browser->ExecuteJavaScript(script);
}

void ReciprocatorEngine::injectClickMoreScript() {
  if (!m_browsing || m_state == Resting)
    return;

  // 参考 SpotlightX 的 "Show N posts" 自动点击逻辑
  QString script = R"JS(
(function() {
    try {
        const cells = document.querySelectorAll('[data-testid="cellInnerDiv"]');
        for (const cell of cells) {
            const text = cell.textContent || '';
            if (/show.*post|显示.*帖|条新帖|新的帖子|new post/i.test(text)) {
                const btn = cell.querySelector('[role="button"]') || cell.querySelector('button');
                if (btn) {
                    btn.click();
                    console.log('[XSocialLedger] Auto-clicked: Show new posts');
                    break;
                }
                cell.click();
                console.log('[XSocialLedger] Auto-clicked cell: Show new posts');
                break;
            }
        }
    } catch(e) {}
})();
)JS";

  m_browser->ExecuteJavaScript(script);
}

void ReciprocatorEngine::onWebMessage(const QString &message) {
  if (!m_browsing)
    return;

  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
  if (!doc.isObject())
    return;

  QJsonObject obj = doc.object();
  QString type = obj.value("type").toString();

  if (type == "reciprocate_target") {
    // 找到了目标用户的帖子
    QString handle = obj.value("handle").toString().toLower();
    int index = obj.value("index").toInt();

    if (m_likedHandles.contains(handle))
      return;

    emit statusMessage(
        QString::fromUtf8("🎯 发现 @%1 的帖子，准备点赞...").arg(handle));

    // 暂停滚动
    m_scrollTimer->stop();
    setState(LikePause);

    // 模拟阅读帖子 (1-3秒，真人看到想点赞的帖子会快速反应)
    int readDelay = 1000 + QRandomGenerator::global()->bounded(2000);
    QTimer::singleShot(readDelay, this, [this, handle, index]() {
      if (!m_browsing)
        return;

      injectLikeScript(index);

      // 记录回馈
      m_likedHandles.insert(handle);
      if (m_targetMap.contains(handle)) {
        QString actionId = m_targetMap.value(handle);
        m_storage->markReciprocated(actionId, true);
        emit likedUser(handle, actionId);
      }

      emit statusMessage(QString::fromUtf8("✅ 已为 @%1 点赞 (%2/%3)")
                             .arg(handle)
                             .arg(int(m_likedHandles.size()))
                             .arg(int(m_targetMap.size())));

      // 检查是否全部完成
      if (m_likedHandles.size() >= m_targetMap.size()) {
        emit statusMessage(QString::fromUtf8("🎉 所有 %1 个用户已全部回馈！")
                               .arg(int(m_likedHandles.size())));
        stopBrowsing();
        return;
      }

      // 点赞后短暂继续浏览（真人点赞后会继续滚动，不会停顿很久）
      int likeWait = randomInRange(m_likeWaitMinSec, m_likeWaitMaxSec);

      QTimer::singleShot(likeWait * 1000, this, [this]() {
        if (!m_browsing)
          return;
        // 恢复浏览状态，继续正常滚动
        if (m_state == LikePause) {
          setState(Browsing);
          scheduleNextScroll();
        }
      });
    });

  } else if (type == "like_clicked") {
    qDebug() << "[ReciprocatorEngine] Like button clicked via JS";
  }
}
