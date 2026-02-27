#include "MainWindow.h"
#include "ActionListPanel.h"
#include "Core/NotificationCollector.h"
#include "Core/ReciprocatorEngine.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
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
#include <QScrollBar>
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

  // 中间 - 记录面板 + 日志（垂直分割）
  QSplitter *middleSplitter = new QSplitter(Qt::Vertical, m_splitter);
  m_actionPanel = new ActionListPanel(m_storage, middleSplitter);
  m_actionPanel->setMinimumWidth(300);

  m_logBox = new QTextEdit(middleSplitter);
  m_logBox->setReadOnly(true);
  m_logBox->setMaximumHeight(200);
  m_logBox->setStyleSheet(
      "QTextEdit { background: #0a0a1a; color: #88cc88; border: 1px solid "
      "#2a2a4a; font-family: 'Consolas', 'Courier New', monospace; "
      "font-size: 11px; padding: 4px; }");
  m_logBox->setPlaceholderText(QString::fromUtf8("动作日志将显示在这里..."));

  middleSplitter->addWidget(m_actionPanel);
  middleSplitter->addWidget(m_logBox);
  middleSplitter->setStretchFactor(0, 3); // 记录面板 75%
  middleSplitter->setStretchFactor(1, 1); // 日志 25%

  // 右侧 - 回馈浏览器
  m_recipBrowser = new WebView2Widget(m_splitter);
  m_recipBrowser->setMinimumWidth(400);

  m_splitter->addWidget(m_browser);
  m_splitter->addWidget(middleSplitter);
  m_splitter->addWidget(m_recipBrowser);
  m_splitter->setStretchFactor(0, 4); // 浏览器 40%
  m_splitter->setStretchFactor(1, 3); // 面板+日志 30%
  m_splitter->setStretchFactor(2, 3); // 回馈浏览器 30%

  setCentralWidget(m_splitter);

  // 创建采集器
  m_collector = new NotificationCollector(m_browser, m_storage, this);

  // 创建回馈引擎
  m_recipBrowser->CreateBrowser("https://x.com");
  m_reciprocator = new ReciprocatorEngine(m_recipBrowser, m_storage, this);

  // 状态栏
  m_statusLabel =
      new QLabel(QString::fromUtf8("\xe5\xb0\xb1\xe7\xbb\xaa"), this);
  m_statusLabel->setStyleSheet(
      "QLabel { color: #e0e0e0; font-size: 13px; padding: 2px 8px; }");
  m_countdownLabel = new QLabel("", this);
  m_countdownLabel->setStyleSheet(
      "QLabel { color: #ffcc00; font-size: 13px; padding: 2px 8px; }");
  m_refreshCountdown = 0;
  m_sessionCountdown = 0;
  statusBar()->setStyleSheet("QStatusBar { background: #0f0f23; color: "
                             "#e0e0e0; border-top: 1px solid #2a2a4a; }");
  statusBar()->addPermanentWidget(m_countdownLabel);
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

  QString spinStyle =
      "QSpinBox { background: #1a1a2e; color: #e0e0e0; border: 1px solid "
      "#3a5a8a; border-radius: 4px; padding: 2px 6px; min-width: 50px; }";

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
  m_pagesSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_pagesSpin);

  // Refresh interval range
  QLabel *refreshLabel =
      new QLabel(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0:"), this);
  refreshLabel->setStyleSheet("color: #a0a0c0;");
  toolbar->addWidget(refreshLabel);
  m_refreshMinSpin = new QSpinBox(this);
  m_refreshMinSpin->setRange(10, 600);
  m_refreshMinSpin->setValue(60);
  m_refreshMinSpin->setSuffix("s");
  m_refreshMinSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_refreshMinSpin);
  QLabel *refreshDash = new QLabel("-", this);
  refreshDash->setStyleSheet("color: #a0a0c0;");
  toolbar->addWidget(refreshDash);
  m_refreshMaxSpin = new QSpinBox(this);
  m_refreshMaxSpin->setRange(10, 600);
  m_refreshMaxSpin->setValue(120);
  m_refreshMaxSpin->setSuffix("s");
  m_refreshMaxSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_refreshMaxSpin);

  toolbar->addSeparator();

  // === 自动回馈按钮 ===
  m_batchBtn = new QPushButton(
      QString::fromUtf8(
          "\xf0\x9f\x9a\x80 \xe8\x87\xaa\xe5\x8a\xa8\xe5\x9b\x9e\xe9\xa6\x88"),
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

  toolbar->addSeparator();

  // === 回馈配置控件 ===
  // 滚动间隔
  QLabel *scrollLabel = new QLabel(QString::fromUtf8("滚动:"), this);
  scrollLabel->setStyleSheet("color: #a0a0c0; font-size: 11px;");
  toolbar->addWidget(scrollLabel);
  m_scrollMinSpin = new QSpinBox(this);
  m_scrollMinSpin->setRange(1, 30);
  m_scrollMinSpin->setValue(3);
  m_scrollMinSpin->setSuffix("s");
  m_scrollMinSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_scrollMinSpin);
  toolbar->addWidget(new QLabel("-", this));
  m_scrollMaxSpin = new QSpinBox(this);
  m_scrollMaxSpin->setRange(1, 60);
  m_scrollMaxSpin->setValue(8);
  m_scrollMaxSpin->setSuffix("s");
  m_scrollMaxSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_scrollMaxSpin);

  // 点赞等待
  QLabel *likeLabel = new QLabel(QString::fromUtf8("点赞:"), this);
  likeLabel->setStyleSheet("color: #a0a0c0; font-size: 11px;");
  toolbar->addWidget(likeLabel);
  m_likeWaitMinSpin = new QSpinBox(this);
  m_likeWaitMinSpin->setRange(1, 60);
  m_likeWaitMinSpin->setValue(3);
  m_likeWaitMinSpin->setSuffix("s");
  m_likeWaitMinSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_likeWaitMinSpin);
  toolbar->addWidget(new QLabel("-", this));
  m_likeWaitMaxSpin = new QSpinBox(this);
  m_likeWaitMaxSpin->setRange(1, 120);
  m_likeWaitMaxSpin->setValue(8);
  m_likeWaitMaxSpin->setSuffix("s");
  m_likeWaitMaxSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_likeWaitMaxSpin);

  // 浏览时长
  QLabel *browseLabel = new QLabel(QString::fromUtf8("浏览:"), this);
  browseLabel->setStyleSheet("color: #a0a0c0; font-size: 11px;");
  toolbar->addWidget(browseLabel);
  m_browseMinSpin = new QSpinBox(this);
  m_browseMinSpin->setRange(1, 120);
  m_browseMinSpin->setValue(10);
  m_browseMinSpin->setSuffix("m");
  m_browseMinSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_browseMinSpin);
  toolbar->addWidget(new QLabel("-", this));
  m_browseMaxSpin = new QSpinBox(this);
  m_browseMaxSpin->setRange(1, 120);
  m_browseMaxSpin->setValue(30);
  m_browseMaxSpin->setSuffix("m");
  m_browseMaxSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_browseMaxSpin);

  // 休息时长
  QLabel *restLabel = new QLabel(QString::fromUtf8("休息:"), this);
  restLabel->setStyleSheet("color: #a0a0c0; font-size: 11px;");
  toolbar->addWidget(restLabel);
  m_restMinSpin = new QSpinBox(this);
  m_restMinSpin->setRange(1, 120);
  m_restMinSpin->setValue(15);
  m_restMinSpin->setSuffix("m");
  m_restMinSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_restMinSpin);
  toolbar->addWidget(new QLabel("-", this));
  m_restMaxSpin = new QSpinBox(this);
  m_restMaxSpin->setRange(1, 120);
  m_restMaxSpin->setValue(45);
  m_restMaxSpin->setSuffix("m");
  m_restMaxSpin->setStyleSheet(spinStyle);
  toolbar->addWidget(m_restMaxSpin);

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
  connect(m_reciprocator, &ReciprocatorEngine::likedUser, this,
          [this](const QString &, const QString &) {
            m_actionPanel->refreshAll();
          });

  // 双击回馈
  connect(m_actionPanel, &ActionListPanel::reciprocateLikeRequested, this,
          &MainWindow::onReciprocateLike);

  // 浏览状态变化
  connect(m_reciprocator, &ReciprocatorEngine::browsingStateChanged, this,
          [this](const QString &state) {
            if (state == "idle") {
              m_batchBtn->setText(QString::fromUtf8(
                  "\xf0\x9f\x9a\x80 "
                  "\xe8\x87\xaa\xe5\x8a\xa8\xe5\x9b\x9e\xe9\xa6\x88"));
              m_batchBtn->setStyleSheet(
                  "QPushButton { background: #e65100; color: #e0e0e0; border: "
                  "1px solid #ff6d00; border-radius: 6px; padding: 6px 16px; "
                  "font-size: 13px; min-width: 80px; }"
                  "QPushButton:hover { background: #ff6d00; }");
              m_sessionCountdown = 0;
              updateCountdownLabel();
            }
          });

  // 会话倒计时
  connect(m_reciprocator, &ReciprocatorEngine::sessionCountdown, this,
          [this](int sec) {
            m_sessionCountdown = sec;
            updateCountdownLabel();
          });

  // Countdown signals -> combined label (refresh)
  connect(m_collector, &NotificationCollector::refreshCountdown, this,
          [this](int sec) {
            m_refreshCountdown = sec;
            updateCountdownLabel();
          });

  // Batch button - toggle start/stop
  connect(m_batchBtn, &QPushButton::clicked, this, [this]() {
    // If browsing is running, stop it
    if (m_reciprocator->isBusy()) {
      m_reciprocator->stopBrowsing();
      m_sessionCountdown = 0;
      updateCountdownLabel();
      onStatusMessage(QString::fromUtf8("⏹ 已停止自动回馈"));
      return;
    }

    // Collect pending reciprocations
    auto likes = m_storage->loadLikes();
    QList<QPair<QString, QString>> pending;
    for (const auto &a : likes) {
      if (!a.reciprocated) {
        pending.append({a.userHandle, a.id});
      }
    }
    if (pending.isEmpty()) {
      onStatusMessage(QString::fromUtf8(
          "\xe6\xb2\xa1\xe6\x9c\x89\xe5\xbe\x85\xe5\x9b\x9e\xe9\xa6\x88"
          "\xe7\x9a\x84\xe7\x82\xb9\xe8\xb5\x9e"));
      return;
    }

    // Apply current settings
    m_reciprocator->setScrollInterval(m_scrollMinSpin->value(),
                                      m_scrollMaxSpin->value());
    m_reciprocator->setLikeWaitInterval(m_likeWaitMinSpin->value(),
                                        m_likeWaitMaxSpin->value());
    m_reciprocator->setBrowseRestCycle(
        m_browseMinSpin->value(), m_browseMaxSpin->value(),
        m_restMinSpin->value(), m_restMaxSpin->value());

    m_reciprocator->startBrowsing(pending);

    // Toggle button to stop mode
    m_batchBtn->setText(QString::fromUtf8(
        "\xe2\x8f\xb9 \xe5\x81\x9c\xe6\xad\xa2\xe5\x9b\x9e\xe9\xa6\x88"));
    m_batchBtn->setStyleSheet(
        "QPushButton { background: #b71c1c; color: #e0e0e0; border: 1px solid "
        "#d32f2f; border-radius: 6px; padding: 6px 16px; font-size: 13px; "
        "min-width: 80px; }"
        "QPushButton:hover { background: #d32f2f; }");
    onStatusMessage(
        QString::fromUtf8(
            "\xf0\x9f\x9a\x80 "
            "\xe8\x87\xaa\xe5\x8a\xa8\xe5\x9b\x9e\xe9\xa6\x88\xe5\xbc\x80"
            "\xe5\xa7\x8b: %1 \xe4\xb8\xaa\xe5\xbe\x85\xe5\xa4\x84\xe7\x90\x86")
            .arg(pending.size()));
  });

  // Spinbox value changes -> save settings and apply immediately
  connect(m_pagesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int val) {
            m_collector->setMaxPages(val);
            saveSettings();
          });
  auto updateRefreshRange = [this]() {
    m_collector->setAutoRefreshRange(m_refreshMinSpin->value(),
                                     m_refreshMaxSpin->value());
    saveSettings();
  };
  connect(m_refreshMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRefreshRange);
  connect(m_refreshMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRefreshRange);

  // 回馈配置 SpinBox 变化 -> 实时生效 + 保存
  auto updateRecipConfig = [this]() {
    m_reciprocator->setScrollInterval(m_scrollMinSpin->value(),
                                      m_scrollMaxSpin->value());
    m_reciprocator->setLikeWaitInterval(m_likeWaitMinSpin->value(),
                                        m_likeWaitMaxSpin->value());
    m_reciprocator->setBrowseRestCycle(
        m_browseMinSpin->value(), m_browseMaxSpin->value(),
        m_restMinSpin->value(), m_restMaxSpin->value());
    saveSettings();
  };
  connect(m_scrollMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_scrollMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_likeWaitMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_likeWaitMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_browseMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_browseMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_restMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
  connect(m_restMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateRecipConfig);
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

  // 追加到日志文本框（带时间戳）
  QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
  m_logBox->append(QString("[%1] %2").arg(timestamp, message));

  // 限制日志行数避免内存增长
  QTextDocument *doc = m_logBox->document();
  if (doc->blockCount() > 500) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
    cursor.removeSelectedText();
  }

  // 自动滚动到底部
  m_logBox->verticalScrollBar()->setValue(
      m_logBox->verticalScrollBar()->maximum());

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

  if (m_reciprocator && m_reciprocator->isBusy()) {
    m_reciprocator->stopBrowsing();
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
    onStatusMessage("回馈引擎忙碌中，请等待当前任务完成...");
    return;
  }
  // 单个回馈：启动浏览模式只针对这一个用户
  QList<QPair<QString, QString>> single;
  single.append({userHandle, actionId});
  m_reciprocator->setScrollInterval(m_scrollMinSpin->value(),
                                    m_scrollMaxSpin->value());
  m_reciprocator->setLikeWaitInterval(m_likeWaitMinSpin->value(),
                                      m_likeWaitMaxSpin->value());
  m_reciprocator->setBrowseRestCycle(
      m_browseMinSpin->value(), m_browseMaxSpin->value(),
      m_restMinSpin->value(), m_restMaxSpin->value());
  m_reciprocator->startBrowsing(single);
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
  int refreshMin = settings.value("refreshMin", 60).toInt();
  int refreshMax = settings.value("refreshMax", 120).toInt();

  // 回馈配置
  int scrollMin = settings.value("scrollMin", 3).toInt();
  int scrollMax = settings.value("scrollMax", 8).toInt();
  int likeWaitMin = settings.value("likeWaitMin", 3).toInt();
  int likeWaitMax = settings.value("likeWaitMax", 8).toInt();
  int browseMin = settings.value("browseMin", 10).toInt();
  int browseMax = settings.value("browseMax", 30).toInt();
  int restMin = settings.value("restMin", 15).toInt();
  int restMax = settings.value("restMax", 45).toInt();

  m_pagesSpin->setValue(pages);
  m_refreshMinSpin->setValue(refreshMin);
  m_refreshMaxSpin->setValue(refreshMax);

  m_scrollMinSpin->setValue(scrollMin);
  m_scrollMaxSpin->setValue(scrollMax);
  m_likeWaitMinSpin->setValue(likeWaitMin);
  m_likeWaitMaxSpin->setValue(likeWaitMax);
  m_browseMinSpin->setValue(browseMin);
  m_browseMaxSpin->setValue(browseMax);
  m_restMinSpin->setValue(restMin);
  m_restMaxSpin->setValue(restMax);

  m_collector->setMaxPages(pages);
  m_collector->setAutoRefreshRange(refreshMin, refreshMax);
  m_reciprocator->setScrollInterval(scrollMin, scrollMax);
  m_reciprocator->setLikeWaitInterval(likeWaitMin, likeWaitMax);
  m_reciprocator->setBrowseRestCycle(browseMin, browseMax, restMin, restMax);
}

void MainWindow::saveSettings() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  settings.setValue("maxPages", m_pagesSpin->value());
  settings.setValue("refreshMin", m_refreshMinSpin->value());
  settings.setValue("refreshMax", m_refreshMaxSpin->value());

  settings.setValue("scrollMin", m_scrollMinSpin->value());
  settings.setValue("scrollMax", m_scrollMaxSpin->value());
  settings.setValue("likeWaitMin", m_likeWaitMinSpin->value());
  settings.setValue("likeWaitMax", m_likeWaitMaxSpin->value());
  settings.setValue("browseMin", m_browseMinSpin->value());
  settings.setValue("browseMax", m_browseMaxSpin->value());
  settings.setValue("restMin", m_restMinSpin->value());
  settings.setValue("restMax", m_restMaxSpin->value());
}

void MainWindow::updateCountdownLabel() {
  QStringList parts;
  if (m_refreshCountdown > 0) {
    parts << QString::fromUtf8("\xe2\x8f\xb3\xe5\x88\xb7\xe6\x96\xb0:%1s")
                 .arg(m_refreshCountdown);
  }
  if (m_sessionCountdown > 0) {
    int min = m_sessionCountdown / 60;
    int sec = m_sessionCountdown % 60;
    parts << QString::fromUtf8("\xe2\x8f\xb3\xe4\xbc\x9a\xe8\xaf\x9d:%1:%2")
                 .arg(min)
                 .arg(sec, 2, 10, QChar('0'));
  }
  m_countdownLabel->setText(parts.join("  "));
}
