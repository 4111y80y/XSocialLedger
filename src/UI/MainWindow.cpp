#include "MainWindow.h"
#include "ActionListPanel.h"
#include "Core/ListMonitorEngine.h"
#include "Core/NotificationCollector.h"
#include "Core/ReciprocatorEngine.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
#include "WebView2Widget.h"
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
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
  m_recipBrowser->setMinimumWidth(350);

  // 最右侧 - LIST监控浏览器
  m_listBrowser = new WebView2Widget(m_splitter);
  m_listBrowser->setMinimumWidth(350);

  m_splitter->addWidget(m_browser);
  m_splitter->addWidget(middleSplitter);
  m_splitter->addWidget(m_recipBrowser);
  m_splitter->addWidget(m_listBrowser);
  m_splitter->setStretchFactor(0, 3); // 采集浏览器 30%
  m_splitter->setStretchFactor(1, 2); // 面板+日志 20%
  m_splitter->setStretchFactor(2, 3); // 回馈浏览器 25%
  m_splitter->setStretchFactor(3, 3); // LIST浏览器 25%

  setCentralWidget(m_splitter);

  // 创建采集器
  m_collector = new NotificationCollector(m_browser, m_storage, this);

  // 创建回馈引擎
  m_recipBrowser->CreateBrowser("https://x.com");
  m_reciprocator = new ReciprocatorEngine(m_recipBrowser, m_storage, this);

  // 创建LIST监控浏览器和引擎（共享用户数据，无需重新登录）
  m_listBrowser->CreateBrowser("https://x.com");
  m_listMonitor = new ListMonitorEngine(m_listBrowser, m_storage, this);

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

  QString gearStyle =
      "QPushButton { background: #2a2a4a; color: #e0e0e0; "
      "border: 1px solid #3a5a8a; border-radius: 6px; padding: 4px 10px; "
      "font-size: 14px; min-width: 30px; }"
      "QPushButton:hover { background: #3a3a6a; }";

  QString spinStyle =
      "QSpinBox { background: #1a1a2e; color: #e0e0e0; border: 1px solid "
      "#3a5a8a; border-radius: 4px; padding: 4px 8px; min-width: 60px; "
      "font-size: 12px; }";

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

  // ⚙ 采集设置 齿轮按钮
  QPushButton *collectGearBtn =
      new QPushButton(QString::fromUtf8("⚙ 采集"), this);
  collectGearBtn->setStyleSheet(gearStyle);
  toolbar->addWidget(collectGearBtn);
  connect(collectGearBtn, &QPushButton::clicked, this, [this, spinStyle]() {
    QDialog dlg(this);
    dlg.setWindowTitle("采集设置");
    dlg.setStyleSheet(
        "QDialog { background: #12122a; }"
        "QLabel { color: #e0e0e0; font-size: 12px; }"
        "QGroupBox { color: #ffcc00; border: 1px solid #3a5a8a; "
        "border-radius: 6px; margin-top: 8px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; }");
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QGroupBox *grp = new QGroupBox("采集参数", &dlg);
    QFormLayout *form = new QFormLayout(grp);
    QSpinBox *pages = new QSpinBox(&dlg);
    pages->setRange(1, 50);
    pages->setValue(m_pagesSpin->value());
    pages->setStyleSheet(spinStyle);
    form->addRow("采集页数:", pages);

    QHBoxLayout *refreshRow = new QHBoxLayout();
    QSpinBox *rMin = new QSpinBox(&dlg);
    rMin->setRange(10, 600);
    rMin->setValue(m_refreshMinSpin->value());
    rMin->setSuffix("s");
    rMin->setStyleSheet(spinStyle);
    QSpinBox *rMax = new QSpinBox(&dlg);
    rMax->setRange(10, 600);
    rMax->setValue(m_refreshMaxSpin->value());
    rMax->setSuffix("s");
    rMax->setStyleSheet(spinStyle);
    refreshRow->addWidget(rMin);
    refreshRow->addWidget(new QLabel("-"));
    refreshRow->addWidget(rMax);
    form->addRow("刷新间隔:", refreshRow);

    layout->addWidget(grp);
    QPushButton *okBtn = new QPushButton("确定", &dlg);
    okBtn->setStyleSheet(
        "QPushButton { background: #1b5e20; color: #e0e0e0; border-radius: "
        "6px; padding: 6px 20px; }QPushButton:hover { background: #2e7d32; }");
    layout->addWidget(okBtn);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
      m_pagesSpin->setValue(pages->value());
      m_refreshMinSpin->setValue(rMin->value());
      m_refreshMaxSpin->setValue(rMax->value());
    }
  });

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

  // ⚙ 回馈设置 齿轮按钮
  QPushButton *recipGearBtn =
      new QPushButton(QString::fromUtf8("⚙ 回馈"), this);
  recipGearBtn->setStyleSheet(gearStyle);
  toolbar->addWidget(recipGearBtn);
  connect(recipGearBtn, &QPushButton::clicked, this, [this, spinStyle]() {
    QDialog dlg(this);
    dlg.setWindowTitle("回馈设置");
    dlg.setStyleSheet(
        "QDialog { background: #12122a; }"
        "QLabel { color: #e0e0e0; font-size: 12px; }"
        "QGroupBox { color: #ffcc00; border: 1px solid #3a5a8a; "
        "border-radius: 6px; margin-top: 8px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; }");
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QGroupBox *grp = new QGroupBox("回馈参数", &dlg);
    QFormLayout *form = new QFormLayout(grp);

    auto addRange = [&](const QString &label, QSpinBox *srcMin,
                        QSpinBox *srcMax, int lo, int hi, const QString &suf,
                        QSpinBox *&outMin, QSpinBox *&outMax) {
      QHBoxLayout *row = new QHBoxLayout();
      outMin = new QSpinBox(&dlg);
      outMin->setRange(lo, hi);
      outMin->setValue(srcMin->value());
      outMin->setSuffix(suf);
      outMin->setStyleSheet(spinStyle);
      outMax = new QSpinBox(&dlg);
      outMax->setRange(lo, hi);
      outMax->setValue(srcMax->value());
      outMax->setSuffix(suf);
      outMax->setStyleSheet(spinStyle);
      row->addWidget(outMin);
      row->addWidget(new QLabel("-"));
      row->addWidget(outMax);
      form->addRow(label, row);
    };

    QSpinBox *sMin, *sMax, *lMin, *lMax, *bMin, *bMax, *rMin, *rMax;
    addRange("滚动间隔:", m_scrollMinSpin, m_scrollMaxSpin, 1, 60, "s", sMin,
             sMax);
    addRange("点赞等待:", m_likeWaitMinSpin, m_likeWaitMaxSpin, 1, 120, "s",
             lMin, lMax);
    addRange("浏览时长:", m_browseMinSpin, m_browseMaxSpin, 1, 120, "m", bMin,
             bMax);
    addRange("休息时长:", m_restMinSpin, m_restMaxSpin, 1, 120, "m", rMin,
             rMax);

    layout->addWidget(grp);
    QPushButton *okBtn = new QPushButton("确定", &dlg);
    okBtn->setStyleSheet(
        "QPushButton { background: #1b5e20; color: #e0e0e0; border-radius: "
        "6px; padding: 6px 20px; }QPushButton:hover { background: #2e7d32; }");
    layout->addWidget(okBtn);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
      m_scrollMinSpin->setValue(sMin->value());
      m_scrollMaxSpin->setValue(sMax->value());
      m_likeWaitMinSpin->setValue(lMin->value());
      m_likeWaitMaxSpin->setValue(lMax->value());
      m_browseMinSpin->setValue(bMin->value());
      m_browseMaxSpin->setValue(bMax->value());
      m_restMinSpin->setValue(rMin->value());
      m_restMaxSpin->setValue(rMax->value());
    }
  });

  toolbar->addSeparator();

  // === LIST监控按钮 ===
  m_listMonitorBtn =
      new QPushButton(QString::fromUtf8("🔍 启动LIST监控"), this);
  m_listMonitorBtn->setStyleSheet(
      "QPushButton { background: #1565c0; color: #e0e0e0; border: 1px solid "
      "#1976d2; border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
      "QPushButton:hover { background: #1976d2; }"
      "QPushButton:pressed { background: #1565c0; }");
  toolbar->addWidget(m_listMonitorBtn);

  // ⚙ LIST设置 齿轮按钮
  QPushButton *listGearBtn = new QPushButton(QString::fromUtf8("⚙ LIST"), this);
  listGearBtn->setStyleSheet(gearStyle);
  toolbar->addWidget(listGearBtn);
  connect(listGearBtn, &QPushButton::clicked, this, [this, spinStyle]() {
    QDialog dlg(this);
    dlg.setWindowTitle("LIST监控设置");
    dlg.setStyleSheet(
        "QDialog { background: #12122a; }"
        "QLabel { color: #e0e0e0; font-size: 12px; }"
        "QGroupBox { color: #ffcc00; border: 1px solid #3a5a8a; "
        "border-radius: 6px; margin-top: 8px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; }");
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    // List URLs
    QGroupBox *urlGrp = new QGroupBox("List URLs", &dlg);
    QVBoxLayout *urlLayout = new QVBoxLayout(urlGrp);
    QTextEdit *urlEdit = new QTextEdit(&dlg);
    urlEdit->setMaximumHeight(80);
    urlEdit->setPlainText(m_listUrlsEdit->toPlainText());
    urlEdit->setPlaceholderText(QString::fromUtf8(
        "每行一个List URL，如: https://x.com/i/lists/123456"));
    urlEdit->setStyleSheet(
        "QTextEdit { background: #1a1a2e; color: #e0e0e0; border: 1px solid "
        "#3a5a8a; border-radius: 4px; font-size: 11px; padding: 4px; }");
    urlLayout->addWidget(urlEdit);
    layout->addWidget(urlGrp);

    // 风控参数
    QGroupBox *grp = new QGroupBox("风控参数", &dlg);
    QFormLayout *form = new QFormLayout(grp);

    auto addRange = [&](const QString &label, QSpinBox *srcMin,
                        QSpinBox *srcMax, int lo, int hi, const QString &suf,
                        QSpinBox *&outMin, QSpinBox *&outMax) {
      QHBoxLayout *row = new QHBoxLayout();
      outMin = new QSpinBox(&dlg);
      outMin->setRange(lo, hi);
      outMin->setValue(srcMin->value());
      outMin->setSuffix(suf);
      outMin->setStyleSheet(spinStyle);
      outMax = new QSpinBox(&dlg);
      outMax->setRange(lo, hi);
      outMax->setValue(srcMax->value());
      outMax->setSuffix(suf);
      outMax->setStyleSheet(spinStyle);
      row->addWidget(outMin);
      row->addWidget(new QLabel("-"));
      row->addWidget(outMax);
      form->addRow(label, row);
    };

    QSpinBox *llMin, *llMax, *lsMin, *lsMax, *lstMin, *lstMax;
    addRange("点赞间隔:", m_listLikeMinSpin, m_listLikeMaxSpin, 5, 300, "s",
             llMin, llMax);
    addRange("滚动间隔:", m_listScrollMinSpin, m_listScrollMaxSpin, 1, 60, "s",
             lsMin, lsMax);
    addRange("List停留:", m_listStayMinSpin, m_listStayMaxSpin, 1, 60, "m",
             lstMin, lstMax);

    QSpinBox *maxLikes = new QSpinBox(&dlg);
    maxLikes->setRange(1, 500);
    maxLikes->setValue(m_listMaxLikesSpin->value());
    maxLikes->setStyleSheet(spinStyle);
    form->addRow("单次点赞上限:", maxLikes);

    QSpinBox *lrMin, *lrMax;
    addRange("达标后休息:", m_listRestMinSpin, m_listRestMaxSpin, 1, 120, "m",
             lrMin, lrMax);

    QSpinBox *lcMin, *lcMax;
    addRange("同用户冷却:", m_listCooldownMinSpin, m_listCooldownMaxSpin, 60,
             3600, "s", lcMin, lcMax);

    layout->addWidget(grp);
    QPushButton *okBtn = new QPushButton("确定", &dlg);
    okBtn->setStyleSheet(
        "QPushButton { background: #1b5e20; color: #e0e0e0; border-radius: "
        "6px; padding: 6px 20px; }QPushButton:hover { background: #2e7d32; }");
    layout->addWidget(okBtn);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
      m_listUrlsEdit->setPlainText(urlEdit->toPlainText());
      m_listLikeMinSpin->setValue(llMin->value());
      m_listLikeMaxSpin->setValue(llMax->value());
      m_listScrollMinSpin->setValue(lsMin->value());
      m_listScrollMaxSpin->setValue(lsMax->value());
      m_listStayMinSpin->setValue(lstMin->value());
      m_listStayMaxSpin->setValue(lstMax->value());
      m_listMaxLikesSpin->setValue(maxLikes->value());
      m_listRestMinSpin->setValue(lrMin->value());
      m_listRestMaxSpin->setValue(lrMax->value());
      m_listCooldownMinSpin->setValue(lcMin->value());
      m_listCooldownMaxSpin->setValue(lcMax->value());
    }
  });

  // Spacer
  QWidget *spacer = new QWidget(this);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);

  // === 隐藏的存储SpinBox（不在界面上显示，仅用于存储值和触发信号） ===
  // 采集
  m_pagesSpin = new QSpinBox(this);
  m_pagesSpin->setRange(1, 50);
  m_pagesSpin->setValue(5);
  m_pagesSpin->hide();
  m_refreshMinSpin = new QSpinBox(this);
  m_refreshMinSpin->setRange(10, 600);
  m_refreshMinSpin->setValue(60);
  m_refreshMinSpin->hide();
  m_refreshMaxSpin = new QSpinBox(this);
  m_refreshMaxSpin->setRange(10, 600);
  m_refreshMaxSpin->setValue(120);
  m_refreshMaxSpin->hide();
  // 回馈
  m_scrollMinSpin = new QSpinBox(this);
  m_scrollMinSpin->setRange(1, 30);
  m_scrollMinSpin->setValue(3);
  m_scrollMinSpin->hide();
  m_scrollMaxSpin = new QSpinBox(this);
  m_scrollMaxSpin->setRange(1, 60);
  m_scrollMaxSpin->setValue(8);
  m_scrollMaxSpin->hide();
  m_likeWaitMinSpin = new QSpinBox(this);
  m_likeWaitMinSpin->setRange(1, 60);
  m_likeWaitMinSpin->setValue(3);
  m_likeWaitMinSpin->hide();
  m_likeWaitMaxSpin = new QSpinBox(this);
  m_likeWaitMaxSpin->setRange(1, 120);
  m_likeWaitMaxSpin->setValue(8);
  m_likeWaitMaxSpin->hide();
  m_browseMinSpin = new QSpinBox(this);
  m_browseMinSpin->setRange(1, 120);
  m_browseMinSpin->setValue(10);
  m_browseMinSpin->hide();
  m_browseMaxSpin = new QSpinBox(this);
  m_browseMaxSpin->setRange(1, 120);
  m_browseMaxSpin->setValue(30);
  m_browseMaxSpin->hide();
  m_restMinSpin = new QSpinBox(this);
  m_restMinSpin->setRange(1, 120);
  m_restMinSpin->setValue(15);
  m_restMinSpin->hide();
  m_restMaxSpin = new QSpinBox(this);
  m_restMaxSpin->setRange(1, 120);
  m_restMaxSpin->setValue(45);
  m_restMaxSpin->hide();
  // LIST
  m_listLikeMinSpin = new QSpinBox(this);
  m_listLikeMinSpin->setRange(5, 120);
  m_listLikeMinSpin->setValue(15);
  m_listLikeMinSpin->hide();
  m_listLikeMaxSpin = new QSpinBox(this);
  m_listLikeMaxSpin->setRange(5, 300);
  m_listLikeMaxSpin->setValue(45);
  m_listLikeMaxSpin->hide();
  m_listScrollMinSpin = new QSpinBox(this);
  m_listScrollMinSpin->setRange(1, 30);
  m_listScrollMinSpin->setValue(3);
  m_listScrollMinSpin->hide();
  m_listScrollMaxSpin = new QSpinBox(this);
  m_listScrollMaxSpin->setRange(1, 60);
  m_listScrollMaxSpin->setValue(8);
  m_listScrollMaxSpin->hide();
  m_listStayMinSpin = new QSpinBox(this);
  m_listStayMinSpin->setRange(1, 60);
  m_listStayMinSpin->setValue(3);
  m_listStayMinSpin->hide();
  m_listStayMaxSpin = new QSpinBox(this);
  m_listStayMaxSpin->setRange(1, 60);
  m_listStayMaxSpin->setValue(8);
  m_listStayMaxSpin->hide();
  m_listMaxLikesSpin = new QSpinBox(this);
  m_listMaxLikesSpin->setRange(1, 500);
  m_listMaxLikesSpin->setValue(50);
  m_listMaxLikesSpin->hide();
  m_listRestMinSpin = new QSpinBox(this);
  m_listRestMinSpin->setRange(1, 120);
  m_listRestMinSpin->setValue(10);
  m_listRestMinSpin->hide();
  m_listRestMaxSpin = new QSpinBox(this);
  m_listRestMaxSpin->setRange(1, 120);
  m_listRestMaxSpin->setValue(30);
  m_listRestMaxSpin->hide();
  m_listCooldownMinSpin = new QSpinBox(this);
  m_listCooldownMinSpin->setRange(60, 3600);
  m_listCooldownMinSpin->setValue(600);
  m_listCooldownMinSpin->hide();
  m_listCooldownMaxSpin = new QSpinBox(this);
  m_listCooldownMaxSpin->setRange(60, 7200);
  m_listCooldownMaxSpin->setValue(1200);
  m_listCooldownMaxSpin->hide();
  m_listUrlsEdit = new QTextEdit(this);
  m_listUrlsEdit->hide();

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

    // Collect pending reciprocations (仅最近24小时)
    auto likes = m_storage->loadLikes();
    QList<QPair<QString, QString>> pending;
    QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-86400);
    for (const auto &a : likes) {
      if (!a.reciprocated) {
        QDateTime dt = QDateTime::fromString(a.timestamp, Qt::ISODate);
        if (dt.isValid() && dt >= cutoff) {
          pending.append({a.userHandle, a.id});
        }
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

  // === LIST监控信号 ===
  connect(m_listMonitor, &ListMonitorEngine::statusMessage, this,
          &MainWindow::onStatusMessage);
  connect(m_listMonitor, &ListMonitorEngine::likedPost, this,
          [this](const QString &, const QString &) {
            m_actionPanel->refreshAll();
          });

  // LIST监控启停按钮
  connect(m_listMonitorBtn, &QPushButton::clicked, this, [this]() {
    if (m_listMonitor->isRunning()) {
      m_listMonitor->stop();
      m_listMonitorBtn->setText(QString::fromUtf8("🔍 启动LIST监控"));
      m_listMonitorBtn->setStyleSheet(
          "QPushButton { background: #1565c0; color: #e0e0e0; border: 1px "
          "solid #1976d2; border-radius: 6px; padding: 6px 12px; font-size: "
          "12px; }QPushButton:hover { background: #1976d2; }");
      onStatusMessage(QString::fromUtf8("⏹ LIST监控已停止"));
      return;
    }

    // 解析URL列表
    QString text = m_listUrlsEdit->toPlainText().trimmed();
    QStringList urls;
    for (const QString &line : text.split('\n')) {
      QString url = line.trimmed();
      if (!url.isEmpty() && url.startsWith("http")) {
        urls.append(url);
      }
    }
    if (urls.isEmpty()) {
      onStatusMessage(QString::fromUtf8("⚠️ 请输入至少一个List URL"));
      return;
    }

    // 应用风控设置
    m_listMonitor->setLikeInterval(m_listLikeMinSpin->value(),
                                   m_listLikeMaxSpin->value());
    m_listMonitor->setScrollInterval(m_listScrollMinSpin->value(),
                                     m_listScrollMaxSpin->value());
    m_listMonitor->setListStayDuration(m_listStayMinSpin->value(),
                                       m_listStayMaxSpin->value());
    m_listMonitor->setMaxLikesPerSession(m_listMaxLikesSpin->value());
    m_listMonitor->setRestInterval(m_listRestMinSpin->value(),
                                   m_listRestMaxSpin->value());
    m_listMonitor->setUserCooldown(m_listCooldownMinSpin->value(),
                                   m_listCooldownMaxSpin->value());

    saveSettings();
    m_listMonitor->start(urls);

    m_listMonitorBtn->setText(QString::fromUtf8("⏹ 停止LIST监控"));
    m_listMonitorBtn->setStyleSheet(
        "QPushButton { background: #b71c1c; color: #e0e0e0; border: 1px solid "
        "#d32f2f; border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #d32f2f; }");
  });

  // LIST风控SpinBox变化 -> 实时生效 + 保存
  auto updateListConfig = [this]() {
    m_listMonitor->setLikeInterval(m_listLikeMinSpin->value(),
                                   m_listLikeMaxSpin->value());
    m_listMonitor->setScrollInterval(m_listScrollMinSpin->value(),
                                     m_listScrollMaxSpin->value());
    m_listMonitor->setListStayDuration(m_listStayMinSpin->value(),
                                       m_listStayMaxSpin->value());
    m_listMonitor->setMaxLikesPerSession(m_listMaxLikesSpin->value());
    m_listMonitor->setRestInterval(m_listRestMinSpin->value(),
                                   m_listRestMaxSpin->value());
    m_listMonitor->setUserCooldown(m_listCooldownMinSpin->value(),
                                   m_listCooldownMaxSpin->value());
    saveSettings();
  };
  connect(m_listLikeMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listLikeMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listScrollMinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, updateListConfig);
  connect(m_listScrollMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, updateListConfig);
  connect(m_listStayMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listStayMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listMaxLikesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listRestMinSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listRestMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          updateListConfig);
  connect(m_listCooldownMinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, updateListConfig);
  connect(m_listCooldownMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, updateListConfig);
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

  if (m_listMonitor && m_listMonitor->isRunning()) {
    m_listMonitor->stop();
  }

  m_browser->CloseBrowser();
  m_recipBrowser->CloseBrowser();
  m_listBrowser->CloseBrowser();
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
  settings.setValue("splitterSizes_v4", m_splitter->saveState());
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
  if (settings.contains("splitterSizes_v4")) {
    m_splitter->restoreState(settings.value("splitterSizes_v4").toByteArray());
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

  // LIST监控配置
  m_listLikeMinSpin->setValue(settings.value("listLikeMin", 15).toInt());
  m_listLikeMaxSpin->setValue(settings.value("listLikeMax", 45).toInt());
  m_listScrollMinSpin->setValue(settings.value("listScrollMin", 3).toInt());
  m_listScrollMaxSpin->setValue(settings.value("listScrollMax", 8).toInt());
  m_listStayMinSpin->setValue(settings.value("listStayMin", 3).toInt());
  m_listStayMaxSpin->setValue(settings.value("listStayMax", 8).toInt());
  m_listMaxLikesSpin->setValue(settings.value("listMaxLikes", 50).toInt());
  m_listUrlsEdit->setPlainText(settings.value("listUrls", "").toString());

  m_listMonitor->setLikeInterval(m_listLikeMinSpin->value(),
                                 m_listLikeMaxSpin->value());
  m_listMonitor->setScrollInterval(m_listScrollMinSpin->value(),
                                   m_listScrollMaxSpin->value());
  m_listMonitor->setListStayDuration(m_listStayMinSpin->value(),
                                     m_listStayMaxSpin->value());
  m_listMonitor->setMaxLikesPerSession(m_listMaxLikesSpin->value());
  m_listRestMinSpin->setValue(settings.value("listRestMin", 10).toInt());
  m_listRestMaxSpin->setValue(settings.value("listRestMax", 30).toInt());
  m_listMonitor->setRestInterval(m_listRestMinSpin->value(),
                                 m_listRestMaxSpin->value());
  m_listCooldownMinSpin->setValue(
      settings.value("listCooldownMin", 600).toInt());
  m_listCooldownMaxSpin->setValue(
      settings.value("listCooldownMax", 1200).toInt());
  m_listMonitor->setUserCooldown(m_listCooldownMinSpin->value(),
                                 m_listCooldownMaxSpin->value());
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

  // LIST监控配置
  settings.setValue("listLikeMin", m_listLikeMinSpin->value());
  settings.setValue("listLikeMax", m_listLikeMaxSpin->value());
  settings.setValue("listScrollMin", m_listScrollMinSpin->value());
  settings.setValue("listScrollMax", m_listScrollMaxSpin->value());
  settings.setValue("listStayMin", m_listStayMinSpin->value());
  settings.setValue("listStayMax", m_listStayMaxSpin->value());
  settings.setValue("listMaxLikes", m_listMaxLikesSpin->value());
  settings.setValue("listRestMin", m_listRestMinSpin->value());
  settings.setValue("listRestMax", m_listRestMaxSpin->value());
  settings.setValue("listCooldownMin", m_listCooldownMinSpin->value());
  settings.setValue("listCooldownMax", m_listCooldownMaxSpin->value());
  settings.setValue("listUrls", m_listUrlsEdit->toPlainText());
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
