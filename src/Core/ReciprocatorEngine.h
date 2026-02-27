#ifndef RECIPROCATORENGINE_H
#define RECIPROCATORENGINE_H

#include <QMap>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QTimer>

class WebView2Widget;
class DataStorage;

// 自动回馈引擎 - 模拟真人在首页时间线浏览并自动点赞回馈
class ReciprocatorEngine : public QObject {
  Q_OBJECT

public:
  explicit ReciprocatorEngine(WebView2Widget *browser, DataStorage *storage,
                              QObject *parent = nullptr);
  ~ReciprocatorEngine();

  // 开始首页浏览模式（传入需要回馈的用户handle和对应actionId）
  void startBrowsing(const QList<QPair<QString, QString>> &targets);
  void stopBrowsing();

  bool isBusy() const { return m_browsing; }

  // 配置接口 - 设置即生效
  void setScrollInterval(int minSec, int maxSec);
  void setLikeWaitInterval(int minSec, int maxSec);
  void setBrowseRestCycle(int browseMinMin, int browseMaxMin, int restMinMin,
                          int restMaxMin);

signals:
  void statusMessage(const QString &message);
  void likedUser(const QString &handle, const QString &actionId);
  void
  browsingStateChanged(const QString &state); // "browsing", "resting", "idle"
  void sessionCountdown(int secondsRemaining);
  void batchFinished();

private slots:
  void onPageLoaded(bool success);
  void onWebMessage(const QString &message);

private:
  enum State {
    Idle,
    NavigatingHome,
    Browsing,  // 浏览中 - 滚动+扫描
    LikePause, // 点赞后暂停
    Resting    // 休息中
  };

  void setState(State state);
  void scheduleNextScroll();
  void doScroll();
  void injectScanScript();
  void injectLikeScript(int articleIndex);
  void injectClickMoreScript();
  void startBrowseSession();
  void startRestSession();
  int randomInRange(int minVal, int maxVal);
  QString buildTargetHandlesJs();

  WebView2Widget *m_browser;
  DataStorage *m_storage;
  State m_state;
  bool m_browsing;

  // 目标用户
  QMap<QString, QString> m_targetMap; // handle -> actionId
  QSet<QString> m_likedHandles;       // 本轮已回馈

  // 定时器
  QTimer *m_scrollTimer;    // 滚动定时器
  QTimer *m_sessionTimer;   // 浏览/休息周期定时器
  QTimer *m_countdownTimer; // 倒计时显示
  QTimer *m_clickMoreTimer; // 自动点击More

  int m_scrollCount;
  int m_countdownRemaining;

  // 可配置参数
  int m_scrollMinSec;   // 滚动间隔最小(秒) 默认5
  int m_scrollMaxSec;   // 滚动间隔最大(秒) 默认15
  int m_likeWaitMinSec; // 点赞后等待最小(秒) 默认60
  int m_likeWaitMaxSec; // 点赞后等待最大(秒) 默认180
  int m_browseMinMin;   // 浏览时长最小(分钟) 默认20
  int m_browseMaxMin;   // 浏览时长最大(分钟) 默认20
  int m_restMinMin;     // 休息时长最小(分钟) 默认20
  int m_restMaxMin;     // 休息时长最大(分钟) 默认20
};

#endif // RECIPROCATORENGINE_H
