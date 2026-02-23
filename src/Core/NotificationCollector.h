#ifndef NOTIFICATIONCOLLECTOR_H
#define NOTIFICATIONCOLLECTOR_H

#include <QObject>
#include <QTimer>

class WebView2Widget;
class DataStorage;

// 通知采集引擎 - 注入 JS 到 X.com 通知页面进行数据采集
class NotificationCollector : public QObject {
  Q_OBJECT

public:
  explicit NotificationCollector(WebView2Widget *browser, DataStorage *storage,
                                 QObject *parent = nullptr);
  ~NotificationCollector();

  // 开始/停止采集
  void startCollecting();
  void stopCollecting();

  bool isCollecting() const { return m_collecting; }

  void setMaxPages(int pages) { m_maxPages = pages; }
  int maxPages() const { return m_maxPages; }

  void setAutoRefreshRange(int minSec, int maxSec);
  int refreshMinInterval() const { return m_refreshMinInterval; }
  int refreshMaxInterval() const { return m_refreshMaxInterval; }
  void setAutoRefreshEnabled(bool enabled);

signals:
  void newLikeCollected(const QString &userName, const QString &timestamp);
  void newReplyCollected(const QString &userName, const QString &timestamp);
  void collectingStateChanged(bool collecting);
  void statusMessage(const QString &message);
  void selfRecordsCleaned(int removedCount);
  void refreshCountdown(int secondsRemaining);

private slots:
  void onPageLoaded(bool success);
  void onLikeFound(const QString &jsonData);
  void onReplyFound(const QString &jsonData);
  void onCollectProgress(const QString &jsonData);
  void onPollTimer();

private:
  void injectCollectorScript();
  void triggerScroll();

  WebView2Widget *m_browser;
  DataStorage *m_storage;
  QTimer *m_pollTimer;
  QTimer *m_autoRefreshTimer;
  bool m_collecting;
  bool m_scriptInjected;
  int m_scrollCount;
  int m_maxPages;
  int m_refreshMinInterval;
  int m_refreshMaxInterval;
  QTimer *m_countdownTimer;
  int m_countdownRemaining;
};

#endif // NOTIFICATIONCOLLECTOR_H
