#include "MainWindow.h"
#include "ActionListPanel.h"
#include "Core/NotificationCollector.h"
#include "Core/ReciprocatorEngine.h"
#include "Data/DataStorage.h"
#include "WebView2Widget.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {

  // 初始化数据存储
  m_storage = new DataStorage(this);

  setupUI();
  setupToolBar();
  setupConnections();

  // 恢复窗口布局
  restoreLayout();

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
  m_browser->setMinimumWidth(400);

  // 中间 - 记录面板
  m_actionPanel = new ActionListPanel(m_storage, m_splitter);
  m_actionPanel->setMinimumWidth(300);

  // 右侧 - 回馋浏览器
  m_recipBrowser = new WebView2Widget(m_splitter);
  m_recipBrowser->setMinimumWidth(400);

  m_splitter->addWidget(m_browser);
  m_splitter->addWidget(m_actionPanel);
  m_splitter->addWidget(m_recipBrowser);
  m_splitter->setStretchFactor(0, 4); // 浏览器 40%
  m_splitter->setStretchFactor(1, 3); // 面板 30%
  m_splitter->setStretchFactor(2, 3); // 回馋浏览器 30%

  setCentralWidget(m_splitter);

  // 创建采集器
  m_collector = new NotificationCollector(m_browser, m_storage, this);

  // 创建回馋引擎
  m_recipBrowser->CreateBrowser("https://x.com");
  m_reciprocator = new ReciprocatorEngine(m_recipBrowser, m_storage, this);

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

  m_exportBtn = new QPushButton(
      QString::fromUtf8("\xf0\x9f\x93\xa4 \xe5\xaf\xbc\xe5\x87\xba"), this);
  m_exportBtn->setStyleSheet(btnStyle);
  toolbar->addWidget(m_exportBtn);

  toolbar->addSeparator();

  // Pages spinbox
  QLabel *pagesLabel = new QLabel("Pages:", this);
  pagesLabel->setStyleSheet("color: #a0a0c0;");
  toolbar->addWidget(pagesLabel);
  m_pagesSpin = new QSpinBox(this);
  m_pagesSpin->setRange(1, 50);
  m_pagesSpin->setValue(5);
  m_pagesSpin->setStyleSheet(
      "QSpinBox { background: #1a1a2e; color: #e0e0e0; border: 1px solid "
      "#3a5a8a; border-radius: 4px; padding: 2px 6px; min-width: 50px; }");
  toolbar->addWidget(m_pagesSpin);

  // Refresh interval spinbox
  QLabel *refreshLabel = new QLabel("Refresh(s):", this);
  refreshLabel->setStyleSheet("color: #a0a0c0;");
  toolbar->addWidget(refreshLabel);
  m_refreshIntervalSpin = new QSpinBox(this);
  m_refreshIntervalSpin->setRange(30, 600);
  m_refreshIntervalSpin->setValue(150);
  m_refreshIntervalSpin->setStyleSheet(
      "QSpinBox { background: #1a1a2e; color: #e0e0e0; border: 1px solid "
      "#3a5a8a; border-radius: 4px; padding: 2px 6px; min-width: 50px; }");
  toolbar->addWidget(m_refreshIntervalSpin);

  toolbar->addSeparator();

  // Batch interval spinbox
  QLabel *batchLabel = new QLabel("Batch(s):", this);
  batchLabel->setStyleSheet("color: #a0a0c0;");
  toolbar->addWidget(batchLabel);
  m_batchIntervalSpin = new QSpinBox(this);
  m_batchIntervalSpin->setRange(30, 600);
  m_batchIntervalSpin->setValue(150);
  m_batchIntervalSpin->setStyleSheet(
      "QSpinBox { background: #1a1a2e; color: #e0e0e0; border: 1px solid "
      "#3a5a8a; border-radius: 4px; padding: 2px 6px; min-width: 50px; }");
  toolbar->addWidget(m_batchIntervalSpin);

  // Batch button
  m_batchBtn = new QPushButton(
      QString::fromUtf8(
          "\xf0\x9f\x9a\x80 \xe6\x89\xb9\xe9\x87\x8f\xe5\x9b\x9e\xe9\xa6\x88"),
      this);
  m_batchBtn->setStyleSheet(
      "QPushButton { background: #e65100; color: #e0e0e0; border: 1px solid "
      "#ff6d00; border-radius: 6px; padding: 6px 16px; font-size: 13px; "
      "min-width: 80px; }"
      "QPushButton:hover { background: #ff6d00; }"
      "QPushButton:pressed { background: #e65100; }"
      "QPushButton:disabled { background: #1a1a2e; color: #555566; "
      "border-color: #2a2a3a; }");
  toolbar->addWidget(m_batchBtn);

  // Spacer
  QWidget *spacer = new QWidget(this);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);

  loadSettings();
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

  // 回馈引擎信号
  connect(m_reciprocator, &ReciprocatorEngine::statusMessage, this,
          &MainWindow::onStatusMessage);
  connect(m_reciprocator, &ReciprocatorEngine::reciprocateSuccess, this,
          [this](const QString &, const QString &) {
            m_actionPanel->refreshAll();
          });

  // 双击回馈
  connect(m_actionPanel, &ActionListPanel::reciprocateLikeRequested, this,
          &MainWindow::onReciprocateLike);

  // Batch signals
  connect(m_reciprocator, &ReciprocatorEngine::batchProgress, this,
          [this](int done, int total) {
            onStatusMessage(QString("Batch: %1/%2").arg(done).arg(total));
          });
  connect(m_reciprocator, &ReciprocatorEngine::batchFinished, this,
          [this]() { m_actionPanel->refreshAll(); });

  // Batch button
  connect(m_batchBtn, &QPushButton::clicked, this, [this]() {
    auto likes = m_storage->loadLikes();
    QDate today = QDate::currentDate();
    QList<QPair<QString, QString>> pending;
    for (const auto &a : likes) {
      if (a.reciprocated)
        continue;
      QDateTime dt = QDateTime::fromString(a.timestamp, Qt::ISODate);
      if (dt.isValid() && dt.toLocalTime().date() == today) {
        pending.prepend({a.userHandle, a.id});
      }
    }
    if (pending.isEmpty()) {
      onStatusMessage("No pending likes today.");
      return;
    }
    m_reciprocator->setBatchInterval(m_batchIntervalSpin->value());
    m_reciprocator->startBatchReciprocate(pending);
  });

  // Spinbox value changes -> save settings
  connect(m_pagesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int val) {
            m_collector->setMaxPages(val);
            saveSettings();
          });
  connect(m_refreshIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](int val) {
            m_collector->setAutoRefreshInterval(val);
            saveSettings();
          });
  connect(m_batchIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](int) { saveSettings(); });
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
  // 保存窗口布局
  saveLayout();

  if (m_collector && m_collector->isCollecting()) {
    m_collector->stopCollecting();
  }

  if (m_storage) {
    m_storage->flush();
  }

  m_browser->CloseBrowser();
  m_recipBrowser->CloseBrowser();
  event->accept();
}

void MainWindow::onReciprocateLike(const QString &userHandle,
                                   const QString &actionId) {
  if (m_reciprocator->isBusy()) {
    onStatusMessage("回馋引擎忙碌中，请等待当前任务完成...");
    return;
  }
  m_reciprocator->startLikeReciprocate(userHandle, actionId);
}

void MainWindow::saveLayout() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  settings.setValue("splitterSizes", m_splitter->saveState());
  qDebug() << "[MainWindow] Layout saved";
}

void MainWindow::restoreLayout() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  if (settings.contains("geometry")) {
    restoreGeometry(settings.value("geometry").toByteArray());
  }
  if (settings.contains("windowState")) {
    restoreState(settings.value("windowState").toByteArray());
  }
  if (settings.contains("splitterSizes")) {
    m_splitter->restoreState(settings.value("splitterSizes").toByteArray());
  }
  qDebug() << "[MainWindow] Layout restored";
}

void MainWindow::loadSettings() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  int pages = settings.value("maxPages", 5).toInt();
  int refreshInterval = settings.value("refreshInterval", 150).toInt();
  int batchInterval = settings.value("batchInterval", 150).toInt();

  m_pagesSpin->setValue(pages);
  m_refreshIntervalSpin->setValue(refreshInterval);
  m_batchIntervalSpin->setValue(batchInterval);

  m_collector->setMaxPages(pages);
  m_collector->setAutoRefreshInterval(refreshInterval);
}

void MainWindow::saveSettings() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  settings.setValue("maxPages", m_pagesSpin->value());
  settings.setValue("refreshInterval", m_refreshIntervalSpin->value());
  settings.setValue("batchInterval", m_batchIntervalSpin->value());
}
