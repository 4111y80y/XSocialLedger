#ifndef LISTMONITORENGINE_H
#define LISTMONITORENGINE_H

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

class WebView2Widget;
class DataStorage;

// LIST监控引擎 - 轮流监控多个Twitter List页面，自动点赞所有新帖子
class ListMonitorEngine : public QObject {
  Q_OBJECT

public:
  explicit ListMonitorEngine(WebView2Widget *browser, DataStorage *storage,
                             QObject *parent = nullptr);
  ~ListMonitorEngine();

  // 启停
  void start(const QStringList &listUrls);
  void stop();
  bool isRunning() const { return m_running; }

  // 风控配置 - 设置即生效
  void setLikeInterval(int minSec, int maxSec);
  void setScrollInterval(int minSec, int maxSec);
  void setListStayDuration(int minMin, int maxMin);
  void setSwitchWait(int minSec, int maxSec);
  void setMaxLikesPerSession(int maxLikes);

signals:
  void statusMessage(const QString &message);
  void likedPost(const QString &userHandle, const QString &tweetUrl);
  void runningChanged(bool running);

private slots:
  void onPageLoaded(bool success);
  void onWebMessage(const QString &message);

private:
  enum State {
    Idle,
    NavigatingList, // 正在导航到List页面
    Scanning,       // 浏览中 - 滚动+扫描+点赞
    LikePause,      // 点赞后暂停
    SwitchingList   // 切换List等待中
  };

  void setState(State state);
  void navigateToCurrentList();
  void scheduleNextScroll();
  void doScroll();
  void injectScanScript();
  void injectLikeScript(int articleIndex);
  void switchToNextList();
  int randomInRange(int minVal, int maxVal);

  WebView2Widget *m_browser;
  DataStorage *m_storage;
  State m_state;
  bool m_running;

  // List管理
  QStringList m_listUrls;
  int m_currentListIndex;

  // 已点赞的帖子ID避免重复
  QSet<QString> m_likedTweetIds;

  // 定时器
  QTimer *m_scrollTimer;
  QTimer *m_listStayTimer; // 每个List停留时长

  int m_scrollCount;
  int m_sessionLikeCount; // 本次会话点赞计数

  // 风控参数
  int m_likeIntervalMinSec; // 两次点赞间隔最小(秒) 默认15
  int m_likeIntervalMaxSec; // 两次点赞间隔最大(秒) 默认45
  int m_scrollMinSec;       // 滚动间隔最小(秒) 默认3
  int m_scrollMaxSec;       // 滚动间隔最大(秒) 默认8
  int m_listStayMinMin;     // List停留最小(分钟) 默认3
  int m_listStayMaxMin;     // List停留最大(分钟) 默认8
  int m_switchWaitMinSec;   // 切换等待最小(秒) 默认10
  int m_switchWaitMaxSec;   // 切换等待最大(秒) 默认30
  int m_maxLikesPerSession; // 单次会话最大点赞 默认50
};

#endif // LISTMONITORENGINE_H
