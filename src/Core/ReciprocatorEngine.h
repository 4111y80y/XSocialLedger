#ifndef RECIPROCATORENGINE_H
#define RECIPROCATORENGINE_H

#include <QObject>
#include <QTimer>

class WebView2Widget;
class DataStorage;

// 自动回馈引擎 - 在第三列浏览器中自动点赞回馈
class ReciprocatorEngine : public QObject {
  Q_OBJECT

public:
  explicit ReciprocatorEngine(WebView2Widget *browser, DataStorage *storage,
                              QObject *parent = nullptr);
  ~ReciprocatorEngine();

  // 开始对指定用户进行点赞回馈
  void startLikeReciprocate(const QString &userHandle, const QString &actionId);

  // 停止当前操作
  void stop();

  bool isBusy() const { return m_busy; }

signals:
  void reciprocateStarted(const QString &userHandle);
  void reciprocateSuccess(const QString &userHandle, const QString &actionId);
  void reciprocateFailed(const QString &userHandle, const QString &reason);
  void statusMessage(const QString &message);

private slots:
  void onPageLoaded(bool success);
  void onStepTimer();

private:
  enum State {
    Idle,
    NavigatingToProfile,
    WaitingForTimeline,
    ScanningPosts,
    ClickingLike,
    WaitingAfterLike,
    ScrollingDown,
    Done
  };

  void setState(State state);
  void injectScanScript();
  void injectClickScript();
  void injectScrollScript();

  WebView2Widget *m_browser;
  DataStorage *m_storage;
  QTimer *m_stepTimer;
  State m_state;
  bool m_busy;
  QString m_currentHandle;
  QString m_currentActionId;
  int m_scrollAttempts;
  int m_maxScrollAttempts;
};

#endif // RECIPROCATORENGINE_H
