#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>

class WebView2Widget;
class ActionListPanel;
class DataStorage;
class NotificationCollector;

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

private:
  void setupUI();
  void setupToolBar();
  void setupConnections();
  void saveLayout();
  void restoreLayout();

  // UI 组件
  QSplitter *m_splitter;
  WebView2Widget *m_browser;
  ActionListPanel *m_actionPanel;

  // 工具栏按钮
  QPushButton *m_startBtn;
  QPushButton *m_stopBtn;
  QPushButton *m_refreshBtn;
  QPushButton *m_exportBtn;

  // 状态栏
  QLabel *m_statusLabel;

  // 数据和逻辑
  DataStorage *m_storage;
  NotificationCollector *m_collector;
};

#endif // MAINWINDOW_H
