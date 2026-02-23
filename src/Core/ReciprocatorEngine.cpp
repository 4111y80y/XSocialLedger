#include "ReciprocatorEngine.h"
#include "App/WebView2Handler.h"
#include "Data/DataStorage.h"
#include "UI/WebView2Widget.h"
#include <QDebug>
#include <QRandomGenerator>

ReciprocatorEngine::ReciprocatorEngine(WebView2Widget *browser,
                                       DataStorage *storage, QObject *parent)
    : QObject(parent), m_browser(browser), m_storage(storage), m_state(Idle),
      m_busy(false), m_scrollAttempts(0), m_maxScrollAttempts(15),
      m_batchDone(0), m_batchTotal(0), m_batchInterval(150),
      m_batchCountdownRemaining(0), m_batchMode(false) {

  m_stepTimer = new QTimer(this);
  m_stepTimer->setSingleShot(true);
  connect(m_stepTimer, &QTimer::timeout, this,
          &ReciprocatorEngine::onStepTimer);
  connect(m_browser, &WebView2Widget::loadFinished, this,
          &ReciprocatorEngine::onPageLoaded);

  m_batchTimer = new QTimer(this);
  m_batchTimer->setSingleShot(true);
  connect(m_batchTimer, &QTimer::timeout, this, [this]() {
    m_batchCountdownTimer->stop();
    if (!m_batchMode || m_batchQueue.isEmpty()) {
      emit batchFinished();
      m_batchMode = false;
      return;
    }
    auto next = m_batchQueue.takeFirst();
    startLikeReciprocate(next.first, next.second);
  });

  m_batchCountdownTimer = new QTimer(this);
  m_batchCountdownTimer->setInterval(1000);
  connect(m_batchCountdownTimer, &QTimer::timeout, this, [this]() {
    m_batchCountdownRemaining--;
    if (m_batchCountdownRemaining > 0) {
      emit statusMessage(
          QString::fromUtf8("\xe2\x8f\xb3 "
                            "\xe5\x9b\x9e\xe9\xa6\x88\xe5\x80\x92\xe8\xae\xa1"
                            "\xe6\x97\xb6: %1s (%2/%3)")
              .arg(m_batchCountdownRemaining)
              .arg(m_batchDone + 1)
              .arg(m_batchTotal));
    }
  });
}

ReciprocatorEngine::~ReciprocatorEngine() { stop(); }

void ReciprocatorEngine::startLikeReciprocate(const QString &userHandle,
                                              const QString &actionId) {
  if (m_busy) {
    emit statusMessage("回馈引擎忙碌中，请稍后...");
    return;
  }

  m_currentHandle = userHandle;
  m_currentActionId = actionId;
  m_scrollAttempts = 0;
  m_busy = true;

  emit reciprocateStarted(userHandle);
  emit statusMessage(QString("🔄 开始回馈 @%1 ...").arg(userHandle));

  // 导航到用户主页
  QString url = "https://x.com/" + userHandle;
  setState(NavigatingToProfile);
  m_browser->LoadUrl(url);
}

void ReciprocatorEngine::stop() {
  m_stepTimer->stop();
  m_busy = false;
  m_state = Idle;
  m_currentHandle.clear();
  m_currentActionId.clear();
}

void ReciprocatorEngine::setState(State state) { m_state = state; }

void ReciprocatorEngine::onPageLoaded(bool success) {
  if (!m_busy)
    return;

  if (m_state == NavigatingToProfile) {
    if (!success) {
      emit reciprocateFailed(m_currentHandle, "页面加载失败");
      stop();
      return;
    }

    emit statusMessage(
        QString("已进入 @%1 主页，等待帖子加载...").arg(m_currentHandle));
    setState(WaitingForTimeline);
    // 等待页面内容加载（3-5秒随机延迟，模拟人工操作）
    int delay = 3000 + QRandomGenerator::global()->bounded(2000);
    m_stepTimer->start(delay);
  }
}

void ReciprocatorEngine::onStepTimer() {
  if (!m_busy)
    return;

  switch (m_state) {
  case WaitingForTimeline:
    // 页面加载完成，开始扫描帖子
    setState(ScanningPosts);
    injectScanScript();
    break;

  case ScanningPosts:
    // 扫描后需要滚动找更多
    setState(ScrollingDown);
    injectScrollScript();
    break;

  case ClickingLike:
    injectClickScript();
    break;

  case WaitingAfterLike:
    m_storage->markReciprocated(m_currentActionId, true);
    emit reciprocateSuccess(m_currentHandle, m_currentActionId);
    stop();
    if (m_batchMode) {
      m_batchDone++;
      emit batchProgress(m_batchDone, m_batchTotal);
      if (m_batchQueue.isEmpty()) {
        emit batchFinished();
        m_batchMode = false;
      } else {
        m_batchTimer->start(m_batchInterval * 1000);
        m_batchCountdownRemaining = m_batchInterval;
        m_batchCountdownTimer->start();
      }
    }
    break;

  case ScrollingDown:
    injectScrollScript();
    break;

  default:
    break;
  }
}

void ReciprocatorEngine::injectScanScript() {
  // 扫描页面上的帖子，找到第一个未点赞的，滚动到它
  QString script = R"JS(
(function() {
    const articles = document.querySelectorAll('article[data-testid="tweet"]');
    for (let i = 0; i < articles.length; i++) {
        const likeBtn = articles[i].querySelector('[data-testid="like"]');
        if (likeBtn) {
            articles[i].scrollIntoView({ behavior: 'smooth', block: 'center' });
            console.log('[RECIPROCATE_FOUND]' + i);
            return;
        }
    }
    console.log('[RECIPROCATE_NOTFOUND]' + articles.length);
})();
)JS";

  m_browser->ExecuteJavaScript(script);

  // 等待扫描完成后进入下一步
  int delay = 2000 + QRandomGenerator::global()->bounded(1000);
  QTimer::singleShot(delay, this, [this]() {
    if (!m_busy)
      return;
    // 尝试点赞
    setState(ClickingLike);
    m_stepTimer->start(500);
  });
}

void ReciprocatorEngine::injectClickScript() {
  // 点击第一个未点赞的按钮
  QString script = R"JS(
(function() {
    const articles = document.querySelectorAll('article[data-testid="tweet"]');
    for (const article of articles) {
        const likeBtn = article.querySelector('[data-testid="like"]');
        if (likeBtn) {
            likeBtn.click();
            console.log('[RECIPROCATE_LIKED]');
            return;
        }
    }
    // 没找到可点赞的帖子，可能需要继续滚动
    console.log('[RECIPROCATE_NOCLICK]');
})();
)JS";

  m_browser->ExecuteJavaScript(script);

  // 等待点赞生效，加随机延迟模拟人工
  int waitAfterLike = 2500 + QRandomGenerator::global()->bounded(2000);
  setState(WaitingAfterLike);
  m_stepTimer->start(waitAfterLike);
}

void ReciprocatorEngine::injectScrollScript() {
  m_scrollAttempts++;
  if (m_scrollAttempts > m_maxScrollAttempts) {
    emit reciprocateFailed(m_currentHandle, "滚动次数超限，未找到可点赞帖子");
    emit statusMessage(QString("❌ @%1 滚动%2次仍未找到可点赞的帖子")
                           .arg(m_currentHandle)
                           .arg(m_maxScrollAttempts));
    stop();
    return;
  }

  emit statusMessage(QString("📜 向下滚动查找 @%1 的帖子 (%2/%3)...")
                         .arg(m_currentHandle)
                         .arg(m_scrollAttempts)
                         .arg(m_maxScrollAttempts));

  // 向下滚动并寻找未点赞帖子
  QString script = R"JS(
(function() {
    window.scrollBy(0, 600);
})();
)JS";

  m_browser->ExecuteJavaScript(script);

  // 等待新内容加载，然后重新扫描
  int delay = 2500 + QRandomGenerator::global()->bounded(2000);
  QTimer::singleShot(delay, this, [this]() {
    if (!m_busy)
      return;
    setState(ScanningPosts);
    injectScanScript();
  });
}

void ReciprocatorEngine::startBatchReciprocate(
    const QList<QPair<QString, QString>> &queue) {
  if (queue.isEmpty())
    return;
  m_batchQueue = queue;
  m_batchDone = 0;
  m_batchTotal = queue.size();
  m_batchMode = true;
  emit batchProgress(0, m_batchTotal);
  auto first = m_batchQueue.takeFirst();
  startLikeReciprocate(first.first, first.second);
}

void ReciprocatorEngine::stopBatch() {
  m_batchMode = false;
  m_batchQueue.clear();
  m_batchTimer->stop();
  m_batchCountdownTimer->stop();
  stop();
  emit batchFinished();
}
