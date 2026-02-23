#include "MainWindow.h"
#include "ActionListPanel.h"
#include "Core/NotificationCollector.h"
#include "Data/DataStorage.h"
#include "WebView2Widget.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {

  // 初始化数据存储
  m_storage = new DataStorage(this);

  setupUI();
  setupToolBar();
  setupConnections();

  // 创建浏览器并导航到通知页面
  m_browser->CreateBrowser("https://x.com/notifications");

  qDebug() << "[MainWindow] Initialized";
}

MainWindow::~MainWindow() {
  if (m_collector) {
    m_collector->stopCollecting();
  }
  m_storage->flush();
}

void MainWindow::setupUI() {
  setWindowTitle("XSocialLedger - X.com 社交互动记录");
  resize(1400, 900);

  // 深色窗口样式
  setStyleSheet("QMainWindow { background: #0a0a1a; }"
                "QToolBar { background: #12122a; border-bottom: 1px solid "
                "#2a2a4a; spacing: 6px; padding: 4px; }"
                "QStatusBar { background: #12122a; color: #8888aa; border-top: "
                "1px solid #2a2a4a; }");

  // 主布局 - 水平分割
  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setStyleSheet(
      "QSplitter::handle { background: #2a2a4a; width: 2px; }");

  // 左侧 - 浏览器
  m_browser = new WebView2Widget(m_splitter);
  m_browser->setMinimumWidth(600);

  // 右侧 - 记录面板
  m_actionPanel = new ActionListPanel(m_storage, m_splitter);
  m_actionPanel->setMinimumWidth(350);

  m_splitter->addWidget(m_browser);
  m_splitter->addWidget(m_actionPanel);
  m_splitter->setStretchFactor(0, 7); // 浏览器 70%
  m_splitter->setStretchFactor(1, 3); // 面板 30%

  setCentralWidget(m_splitter);

  // 创建采集器
  m_collector = new NotificationCollector(m_browser, m_storage, this);

  // 状态栏
  m_statusLabel = new QLabel("就绪", this);
  statusBar()->addPermanentWidget(m_statusLabel);
}

void MainWindow::setupToolBar() {
  QToolBar *toolbar = addToolBar("操作");
  toolbar->setMovable(false);
  toolbar->setIconSize(QSize(24, 24));

  QString btnStyle = "QPushButton { background: #1e3a5f; color: #e0e0e0; "
                     "border: 1px solid #3a5a8a; "
                     "  border-radius: 6px; padding: 6px 16px; font-size: "
                     "13px; min-width: 80px; }"
                     "QPushButton:hover { background: #2a4a7a; }"
                     "QPushButton:pressed { background: #163050; }"
                     "QPushButton:disabled { background: #1a1a2e; color: "
                     "#555566; border-color: #2a2a3a; }";

  m_startBtn = new QPushButton("▶ 开始采集", this);
  m_startBtn->setStyleSheet("QPushButton { background: #1b5e20; color: "
                            "#e0e0e0; border: 1px solid #2e7d32; "
                            "  border-radius: 6px; padding: 6px 16px; "
                            "font-size: 13px; min-width: 80px; }"
                            "QPushButton:hover { background: #2e7d32; }"
                            "QPushButton:pressed { background: #1b5e20; }"
                            "QPushButton:disabled { background: #1a1a2e; "
                            "color: #555566; border-color: #2a2a3a; }");
  toolbar->addWidget(m_startBtn);

  m_stopBtn = new QPushButton("⏹ 停止", this);
  m_stopBtn->setEnabled(false);
  m_stopBtn->setStyleSheet("QPushButton { background: #b71c1c; color: #e0e0e0; "
                           "border: 1px solid #d32f2f; "
                           "  border-radius: 6px; padding: 6px 16px; "
                           "font-size: 13px; min-width: 80px; }"
                           "QPushButton:hover { background: #d32f2f; }"
                           "QPushButton:pressed { background: #b71c1c; }"
                           "QPushButton:disabled { background: #1a1a2e; color: "
                           "#555566; border-color: #2a2a3a; }");
  toolbar->addWidget(m_stopBtn);

  toolbar->addSeparator();

  m_refreshBtn = new QPushButton("🔄 刷新页面", this);
  m_refreshBtn->setStyleSheet(btnStyle);
  toolbar->addWidget(m_refreshBtn);

  m_exportBtn = new QPushButton("📤 导出数据", this);
  m_exportBtn->setStyleSheet(btnStyle);
  toolbar->addWidget(m_exportBtn);

  // 弹性空间
  QWidget *spacer = new QWidget(this);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);
}

void MainWindow::setupConnections() {
  connect(m_startBtn, &QPushButton::clicked, this,
          &MainWindow::onStartCollecting);
  connect(m_stopBtn, &QPushButton::clicked, this,
          &MainWindow::onStopCollecting);
  connect(m_refreshBtn, &QPushButton::clicked, this,
          &MainWindow::onRefreshPage);
  connect(m_exportBtn, &QPushButton::clicked, this, &MainWindow::onExportData);

  // 采集器信号
  connect(m_collector, &NotificationCollector::statusMessage, this,
          &MainWindow::onStatusMessage);
  connect(m_collector, &NotificationCollector::collectingStateChanged, this,
          &MainWindow::onCollectingStateChanged);
  connect(m_collector, &NotificationCollector::newLikeCollected, m_actionPanel,
          &ActionListPanel::onNewLike);
  connect(m_collector, &NotificationCollector::newReplyCollected, m_actionPanel,
          &ActionListPanel::onNewReply);
  connect(m_collector, &NotificationCollector::selfRecordsCleaned,
          m_actionPanel, [this](int) { m_actionPanel->refreshAll(); });
}

void MainWindow::onStartCollecting() { m_collector->startCollecting(); }

void MainWindow::onStopCollecting() { m_collector->stopCollecting(); }

void MainWindow::onRefreshPage() {
  m_browser->LoadUrl("https://x.com/notifications");
}

void MainWindow::onExportData() {
  QString filename = QFileDialog::getSaveFileName(
      this, "导出社交互动数据", "social_actions.json", "JSON Files (*.json)");
  if (filename.isEmpty())
    return;

  QJsonObject root;

  // 导出点赞
  QJsonArray likesArray;
  for (const auto &action : m_storage->loadLikes()) {
    likesArray.append(action.toJson());
  }
  root["likes"] = likesArray;

  // 导出回复
  QJsonArray repliesArray;
  for (const auto &action : m_storage->loadReplies()) {
    repliesArray.append(action.toJson());
  }
  root["replies"] = repliesArray;

  // 统计
  QJsonObject stats;
  stats["totalLikes"] = m_storage->likeCount();
  stats["totalReplies"] = m_storage->replyCount();
  stats["pendingLikes"] = m_storage->pendingLikeCount();
  stats["pendingReplies"] = m_storage->pendingReplyCount();
  stats["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  root["stats"] = stats;

  QFile file(filename);
  if (file.open(QIODevice::WriteOnly)) {
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    onStatusMessage("数据已导出到: " + filename);
  } else {
    QMessageBox::warning(this, "导出失败", "无法写入文件: " + filename);
  }
}

void MainWindow::onStatusMessage(const QString &message) {
  m_statusLabel->setText(message);
  qDebug() << "[Status]" << message;
}

void MainWindow::onCollectingStateChanged(bool collecting) {
  m_startBtn->setEnabled(!collecting);
  m_stopBtn->setEnabled(collecting);

  if (collecting) {
    setWindowTitle("XSocialLedger - 采集中... 🔴");
  } else {
    setWindowTitle("XSocialLedger - X.com 社交互动记录");
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_collector && m_collector->isCollecting()) {
    m_collector->stopCollecting();
  }

  if (m_storage) {
    m_storage->flush();
  }

  m_browser->CloseBrowser();
  event->accept();
}
