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
  m_clickMoreTimer->setSingleShot(true); // 每次随机间隔
  connect(m_clickMoreTimer, &QTimer::timeout, this, [this]() {
    injectClickMoreScript();
    // 随机化下次检查间隔: 2-4分钟
    if (m_browsing && m_state != Resting) {
      int nextInterval = randomInRange(120, 240) * 1000;
      m_clickMoreTimer->start(nextInterval);
    }
  });

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
  // 使用三角分布模拟高斯分布（大部分值集中在中间，偶尔极端）
  // 用两个均匀随机数的平均值，结果近似正态分布
  int range = maxVal - minVal + 1;
  int r1 = QRandomGenerator::global()->bounded(range);
  int r2 = QRandomGenerator::global()->bounded(range);
  return minVal + (r1 + r2) / 2;
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

  // 开始自动点击More（随机间隔 2-4分钟）
  m_clickMoreTimer->start(randomInRange(120, 240) * 1000);

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

  // === 优化#4: 偶尔触发长停顿，模拟看到感兴趣的帖子在仔细读 ===
  if (m_scrollCount > 5 && QRandomGenerator::global()->bounded(100) < 15) {
    int pauseSec = 10 + QRandomGenerator::global()->bounded(21); // 10-30秒
    emit statusMessage(
        QString::fromUtf8("👀 停下来看帖子... %1秒").arg(pauseSec));
    // 长停顿期间不滚动，只等待
    QTimer::singleShot(pauseSec * 1000, this, [this]() {
      if (!m_browsing || m_state != Browsing)
        return;
      scheduleNextScroll();
    });
    return;
  }

  // === 优化: 三档滚动距离 + 10%概率向上回滚 ===
  int scrollAmount;
  int roll = QRandomGenerator::global()->bounded(100);
  if (roll < 10) {
    // 10%概率向上回滚一点（真人偶尔会往回看）
    scrollAmount =
        -(100 + QRandomGenerator::global()->bounded(301)); // -100~-400
    emit statusMessage(QString::fromUtf8("⬆️ 往回看了看..."));
  } else if (roll < 75) {
    // 65%正常看帖 (200-600px)
    scrollAmount = 200 + QRandomGenerator::global()->bounded(401);
  } else if (roll < 92) {
    // 17%快速划过 (800-1500px)
    scrollAmount = 800 + QRandomGenerator::global()->bounded(701);
  } else {
    // 8%细看 (80-200px)
    scrollAmount = 80 + QRandomGenerator::global()->bounded(121);
  }

  QString scrollJs = QString(R"JS(
(function() {
    window.scrollBy({ top: %1, behavior: 'smooth' });
})();
)JS")
                         .arg(scrollAmount);

  m_browser->ExecuteJavaScript(scrollJs);

  // === 优化#5: 状态消息每10次更新一次 ===
  if (m_scrollCount % 10 == 0) {
    emit statusMessage(
        QString::fromUtf8("📜 浏览中... 已滚动 %1 次").arg(m_scrollCount));
  }

  // === 优化#2: 只有30%的滚动会触发扫描 ===
  if (QRandomGenerator::global()->bounded(100) < 30) {
    int scanDelay = 1500 + QRandomGenerator::global()->bounded(1500);
    QTimer::singleShot(scanDelay, this, [this]() {
      if (!m_browsing || m_state != Browsing)
        return;
      injectScanScript();
    });
  }

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
            // 优化#6: 不做scrollIntoView（太精准不自然），帖子在正常滚动中已在视口附近
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
  // 优化#3: 模拟鼠标hover效果后再点击
  QString script = QString(R"JS(
(function() {
    const articles = document.querySelectorAll('article[data-testid="tweet"]');
    if (%1 < articles.length) {
        const likeBtn = articles[%1].querySelector('[data-testid="like"]');
        if (likeBtn) {
            // 先触发hover效果
            likeBtn.dispatchEvent(new MouseEvent('mouseenter', {bubbles: true}));
            likeBtn.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));
            // 短暂停顿后点击 (200-500ms)
            const delay = 200 + Math.random() * 300;
            setTimeout(() => {
                likeBtn.click();
                likeBtn.dispatchEvent(new MouseEvent('mouseleave', {bubbles: true}));
                try {
                    window.chrome.webview.postMessage(JSON.stringify({
                        type: 'like_clicked'
                    }));
                } catch(e) {}
            }, delay);
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

  // X.com 使用虚拟滚动，滚到下方后顶部的 "Show N posts" 按钮会从 DOM 中移除
  // 必须确保真正回到顶部(scrollY=0)，按钮才会出现
  emit statusMessage(QString::fromUtf8("⬆️ 滚动到顶部等待新帖子..."));

  QString script = R"JS(
(function() {
    // 第一步：强制立即滚到最顶部
    window.scrollTo(0, 0);
    document.documentElement.scrollTop = 0;
    document.body.scrollTop = 0;

    let attempts = 0;
    const maxAttempts = 10; // 10次 x 2秒 = 最多20秒
    const checkInterval = 2000;

    function ensureAtTop() {
        // 每次检查前都确保在最顶部
        if (window.scrollY > 5) {
            window.scrollTo(0, 0);
            document.documentElement.scrollTop = 0;
            document.body.scrollTop = 0;
            console.log('[XSocialLedger] Force scroll to top, was at: ' + window.scrollY);
        }
    }

    function tryClickMore() {
        attempts++;
        ensureAtTop();

        try {
            const cells = document.querySelectorAll('[data-testid="cellInnerDiv"]');
            for (const cell of cells) {
                const text = cell.textContent || '';
                if (/show.*post|显示.*帖|条新帖|新的帖子|new post/i.test(text)) {
                    const btn = cell.querySelector('[role="button"]') || cell.querySelector('button');
                    if (btn) {
                        btn.click();
                        console.log('[XSocialLedger] Auto-clicked: Show new posts (attempt ' + attempts + ')');
                        try {
                            window.chrome.webview.postMessage(JSON.stringify({
                                type: 'more_clicked', attempts: attempts
                            }));
                        } catch(e) {}
                        return;
                    }
                    cell.click();
                    console.log('[XSocialLedger] Auto-clicked cell: Show new posts (attempt ' + attempts + ')');
                    try {
                        window.chrome.webview.postMessage(JSON.stringify({
                            type: 'more_clicked', attempts: attempts
                        }));
                    } catch(e) {}
                    return;
                }
            }
        } catch(e) {}

        if (attempts < maxAttempts) {
            setTimeout(tryClickMore, checkInterval);
        } else {
            console.log('[XSocialLedger] More button not found after ' + maxAttempts + ' attempts');
            try {
                window.chrome.webview.postMessage(JSON.stringify({
                    type: 'more_timeout'
                }));
            } catch(e) {}
        }
    }

    // 等1秒确保DOM更新后开始检查
    setTimeout(tryClickMore, 1000);
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

  } else if (type == "more_clicked") {
    int attempts = obj.value("attempts").toInt();
    emit statusMessage(
        QString::fromUtf8("✅ 已加载新帖子 (第%1次尝试)").arg(attempts));

  } else if (type == "more_timeout") {
    // 超时未找到More按钮，可能网络异常，重新加载首页
    emit statusMessage(
        QString::fromUtf8("⚠️ 未发现新帖子按钮，重新加载首页..."));
    m_browser->LoadUrl("https://x.com/home");
  }
}
