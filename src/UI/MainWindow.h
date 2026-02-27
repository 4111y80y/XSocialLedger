#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>

class WebView2Widget;
class ActionListPanel;
class DataStorage;
class NotificationCollector;
class ReciprocatorEngine;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onStartCollecting();
  void onStopCollecting();
  void onRefreshPage();
  void onExportData();
  void onStatusMessage(const QString &message);
  void onCollectingStateChanged(bool collecting);
  void onReciprocateLike(const QString &userHandle, const QString &actionId);

private:
  void setupUI();
  void setupToolBar();
  void setupConnections();
  void saveLayout();
  void restoreLayout();
  void loadSettings();
  void saveSettings();
  void updateCountdownLabel();

  // UI 组件
  QSplitter *m_splitter;
  WebView2Widget *m_browser;
  WebView2Widget *m_recipBrowser;
  ActionListPanel *m_actionPanel;

  QPushButton *m_startBtn;
  QPushButton *m_stopBtn;
  QPushButton *m_refreshBtn;
  QPushButton *m_exportBtn;
  QPushButton *m_batchBtn;
  QSpinBox *m_pagesSpin;
  QSpinBox *m_refreshMinSpin;
  QSpinBox *m_refreshMaxSpin;

  // 自动回馈配置控件
  QSpinBox *m_scrollMinSpin;   // 滚动间隔最小(秒)
  QSpinBox *m_scrollMaxSpin;   // 滚动间隔最大(秒)
  QSpinBox *m_likeWaitMinSpin; // 点赞等待最小(秒)
  QSpinBox *m_likeWaitMaxSpin; // 点赞等待最大(秒)
  QSpinBox *m_browseMinSpin;   // 浏览时长最小(分钟)
  QSpinBox *m_browseMaxSpin;   // 浏览时长最大(分钟)
  QSpinBox *m_restMinSpin;     // 休息时长最小(分钟)
  QSpinBox *m_restMaxSpin;     // 休息时长最大(分钟)

  // Status bar
  QLabel *m_statusLabel;
  QLabel *m_countdownLabel;
  int m_refreshCountdown;
  int m_sessionCountdown;

  // Data and logic
  DataStorage *m_storage;
  NotificationCollector *m_collector;
  ReciprocatorEngine *m_reciprocator;
};

#endif // MAINWINDOW_H
