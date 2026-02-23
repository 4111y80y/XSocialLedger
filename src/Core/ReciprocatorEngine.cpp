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
      m_batchDone(0), m_batchTotal(0), m_batchMinInterval(120),
      m_batchMaxInterval(180), m_batchCountdownRemaining(0),
      m_batchMode(false) {

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
      emit batchCountdownTick(m_batchCountdownRemaining);
    }
  });
}

ReciprocatorEngine::~ReciprocatorEngine() { stop(); }

void ReciprocatorEngine::startLikeReciprocate(const QString &userHandle,
                                              const QString &actionId) {
  if (m_busy) {
    emit statusMessage(QString::fromUtf8(
        "\xe5\x9b\x9e\xe9\xa6\x88\xe5\xbc\x95\xe6\x93\x8e\xe5\xbf\x99\xe7\xa2"
        "\x8c\xe4\xb8\xad\xef\xbc\x8c\xe8\xaf\xb7\xe7\xa8\x8d\xe5\x90\x8e..."));
    return;
  }

  m_currentHandle = userHandle;
  m_currentActionId = actionId;
  m_scrollAttempts = 0;
  m_busy = true;

  emit reciprocateStarted(userHandle);
  emit statusMessage(
      QString::fromUtf8(
          "\xf0\x9f\x94\x84 \xe5\xbc\x80\xe5\xa7\x8b\xe5\x9b\x9e\xe9\xa6\x88 "
          "@%1 ...")
          .arg(userHandle));

  // Navigate to user profile
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
      emit reciprocateFailed(
          m_currentHandle,
          QString::fromUtf8("\xe9\xa1\xb5\xe9\x9d\xa2\xe5\x8a\xa0\xe8\xbd\xbd"
                            "\xe5\xa4\xb1\xe8\xb4\xa5"));
      stop();
      return;
    }

    emit statusMessage(
        QString::fromUtf8(
            "\xe5\xb7\xb2\xe8\xbf\x9b\xe5\x85\xa5 @%1 "
            "\xe4\xb8\xbb\xe9\xa1\xb5\xef\xbc\x8c\xe7\xad\x89\xe5\xbe\x85\xe5"
            "\xb8\x96\xe5\xad\x90\xe5\x8a\xa0\xe8\xbd\xbd...")
            .arg(m_currentHandle));
    setState(WaitingForTimeline);
    int delay = 3000 + QRandomGenerator::global()->bounded(2000);
    m_stepTimer->start(delay);
  }
}

void ReciprocatorEngine::onStepTimer() {
  if (!m_busy)
    return;

  switch (m_state) {
  case WaitingForTimeline:
    setState(ScanningPosts);
    injectScanScript();
    break;

  case ScanningPosts:
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
        // Random interval for next batch item
        int interval = m_batchMinInterval +
                       QRandomGenerator::global()->bounded(
                           m_batchMaxInterval - m_batchMinInterval + 1);
        m_batchTimer->start(interval * 1000);
        m_batchCountdownRemaining = interval;
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

  int delay = 2000 + QRandomGenerator::global()->bounded(1000);
  QTimer::singleShot(delay, this, [this]() {
    if (!m_busy)
      return;
    setState(ClickingLike);
    m_stepTimer->start(500);
  });
}

void ReciprocatorEngine::injectClickScript() {
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
    console.log('[RECIPROCATE_NOCLICK]');
})();
)JS";

  m_browser->ExecuteJavaScript(script);

  int waitAfterLike = 2500 + QRandomGenerator::global()->bounded(2000);
  setState(WaitingAfterLike);
  m_stepTimer->start(waitAfterLike);
}

void ReciprocatorEngine::injectScrollScript() {
  m_scrollAttempts++;
  if (m_scrollAttempts > m_maxScrollAttempts) {
    emit reciprocateFailed(
        m_currentHandle,
        QString::fromUtf8("\xe6\xbb\x9a\xe5\x8a\xa8\xe6\xac\xa1\xe6\x95\xb0\xe8"
                          "\xb6\x85\xe9\x99\x90"));
    emit statusMessage(
        QString::fromUtf8("\xe2\x9d\x8c @%1 "
                          "\xe6\xbb\x9a\xe5\x8a\xa8%"
                          "2\xe6\xac\xa1\xe4\xbb\x8d\xe6\x9c\xaa\xe6\x89\xbe"
                          "\xe5\x88\xb0\xe5\x8f\xaf\xe7\x82\xb9\xe8\xb5\x9e\xe7"
                          "\x9a\x84\xe5\xb8\x96\xe5\xad\x90")
            .arg(m_currentHandle)
            .arg(m_maxScrollAttempts));
    stop();
    return;
  }

  emit statusMessage(
      QString::fromUtf8(
          "\xf0\x9f\x93\x9c "
          "\xe5\x90\x91\xe4\xb8\x8b\xe6\xbb\x9a\xe5\x8a\xa8\xe6\x9f\xa5\xe6\x89"
          "\xbe @%1 \xe7\x9a\x84\xe5\xb8\x96\xe5\xad\x90 (%2/%3)...")
          .arg(m_currentHandle)
          .arg(m_scrollAttempts)
          .arg(m_maxScrollAttempts));

  QString script = R"JS(
(function() {
    window.scrollBy(0, 600);
})();
)JS";

  m_browser->ExecuteJavaScript(script);

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
