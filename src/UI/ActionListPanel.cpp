#include "ActionListPanel.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
#include "StatsPanel.h"
#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFont>
#include <QHeaderView>
#include <QMenu>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

ActionListPanel::ActionListPanel(DataStorage *storage, QWidget *parent)
    : QWidget(parent), m_storage(storage) {
  setupUI();
  refreshAll();
}

ActionListPanel::~ActionListPanel() {}

void ActionListPanel::setupUI() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  // 标题
  QLabel *titleLabel =
      new QLabel(QString::fromUtf8("\xf0\x9f\x93\x92 "
                                   "\xe7\xa4\xbe\xe4\xba\xa4\xe4\xba\x92\xe5"
                                   "\x8a\xa8\xe8\xae\xb0\xe5\xbd\x95"),
                 this);
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(12);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setStyleSheet(
      "padding: 8px; color: #e0e0e0; background: #1a1a2e;");
  layout->addWidget(titleLabel);

  // 统计标签
  m_statsLabel = new QLabel(this);
  m_statsLabel->setAlignment(Qt::AlignCenter);
  m_statsLabel->setStyleSheet(
      "padding: 6px; background: #16213e; color: #a0d2db; "
      "border-radius: 4px; margin: 2px 4px;");
  layout->addWidget(m_statsLabel);

  // Hide reciprocated checkbox
  m_hideReciprocatedCheck = new QCheckBox(
      QString::fromUtf8(
          "\xe9\x9a\x90\xe8\x97\x8f\xe5\xb7\xb2\xe5\x9b\x9e\xe9\xa6\x88"),
      this);
  m_hideReciprocatedCheck->setStyleSheet(
      "QCheckBox { color: #a0d2db; padding: 4px 8px; }"
      "QCheckBox::indicator { width: 16px; height: 16px; }");
  loadHideReciprocatedSetting();
  connect(m_hideReciprocatedCheck, &QCheckBox::toggled, this, [this](bool) {
    saveHideReciprocatedSetting();
    refreshAll();
  });
  layout->addWidget(m_hideReciprocatedCheck);

  // Tab 页
  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setStyleSheet(
      "QTabWidget::pane { border: 1px solid #2a2a4a; background: #0f0f23; }"
      "QTabBar::tab { background: #1a1a2e; color: #8888aa; padding: 8px 16px; "
      "  border: 1px solid #2a2a4a; border-bottom: none; margin-right: 2px; }"
      "QTabBar::tab:selected { background: #16213e; color: #e0e0e0; }"
      "QTabBar::tab:hover { background: #1f1f3a; }");

  // 点赞表格
  m_likeTable = new QTableWidget(this);
  m_likeTable->setColumnCount(4);
  m_likeTable->setHorizontalHeaderLabels(
      {QString::fromUtf8("\xe7\x94\xa8\xe6\x88\xb7"),
       QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4"),
       QString::fromUtf8("\xe5\xb8\x96\xe5\xad\x90\xe7\x89\x87\xe6\xae\xb5"),
       QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81")});
  m_likeTable->horizontalHeader()->setStretchLastSection(true);
  m_likeTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_likeTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_likeTable->horizontalHeader()->setSectionResizeMode(2,
                                                        QHeaderView::Stretch);
  m_likeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_likeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_likeTable->setAlternatingRowColors(true);
  m_likeTable->setContextMenuPolicy(Qt::CustomContextMenu);
  m_likeTable->verticalHeader()->setVisible(false);
  m_likeTable->setStyleSheet(
      "QTableWidget { background: #0f0f23; color: #d0d0d0; gridline-color: "
      "#2a2a4a; "
      "  selection-background-color: #1e3a5f; }"
      "QTableWidget::item:alternate { background: #141428; }"
      "QHeaderView::section { background: #1a1a2e; color: #a0a0c0; "
      "  padding: 4px; border: 1px solid #2a2a4a; }");
  connect(m_likeTable, &QTableWidget::customContextMenuRequested, this,
          &ActionListPanel::onLikeContextMenu);
  connect(m_likeTable, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) {
            QTableWidgetItem *userItem = m_likeTable->item(row, 0);
            if (!userItem)
              return;
            bool reciprocated = userItem->data(Qt::UserRole + 2).toBool();
            if (!reciprocated) {
              QString actionId = userItem->data(Qt::UserRole).toString();
              QString userHandle = userItem->data(Qt::UserRole + 1).toString();
              qDebug() << "[ActionListPanel] Double-click reciprocate:"
                       << userHandle << actionId;
              emit reciprocateLikeRequested(userHandle, actionId);
            }
          });
  m_tabWidget->addTab(
      m_likeTable,
      QString::fromUtf8("\xe2\x9d\xa4\xef\xb8\x8f \xe7\x82\xb9\xe8\xb5\x9e"));

  // 回复表格
  m_replyTable = new QTableWidget(this);
  m_replyTable->setColumnCount(4);
  m_replyTable->setHorizontalHeaderLabels(
      {QString::fromUtf8("\xe7\x94\xa8\xe6\x88\xb7"),
       QString::fromUtf8("\xe6\x97\xb6\xe9\x97\xb4"),
       QString::fromUtf8("\xe5\xb8\x96\xe5\xad\x90\xe7\x89\x87\xe6\xae\xb5"),
       QString::fromUtf8("\xe7\x8a\xb6\xe6\x80\x81")});
  m_replyTable->horizontalHeader()->setStretchLastSection(true);
  m_replyTable->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  m_replyTable->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  m_replyTable->horizontalHeader()->setSectionResizeMode(2,
                                                         QHeaderView::Stretch);
  m_replyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_replyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_replyTable->setAlternatingRowColors(true);
  m_replyTable->setContextMenuPolicy(Qt::CustomContextMenu);
  m_replyTable->verticalHeader()->setVisible(false);
  m_replyTable->setStyleSheet(
      "QTableWidget { background: #0f0f23; color: #d0d0d0; gridline-color: "
      "#2a2a4a; "
      "  selection-background-color: #1e3a5f; }"
      "QTableWidget::item:alternate { background: #141428; }"
      "QHeaderView::section { background: #1a1a2e; color: #a0a0c0; "
      "  padding: 4px; border: 1px solid #2a2a4a; }");
  connect(m_replyTable, &QTableWidget::customContextMenuRequested, this,
          &ActionListPanel::onReplyContextMenu);
  m_tabWidget->addTab(
      m_replyTable,
      QString::fromUtf8("\xf0\x9f\x92\xac \xe5\x9b\x9e\xe5\xa4\x8d"));

  // Stats tab
  m_statsPanel = new StatsPanel(m_storage, this);
  m_tabWidget->addTab(
      m_statsPanel,
      QString::fromUtf8("\xf0\x9f\x93\x8a \xe7\xbb\x9f\xe8\xae\xa1"));

  layout->addWidget(m_tabWidget);
}

void ActionListPanel::populateTable(QTableWidget *table, const QString &type) {
  QList<SocialAction> actions;
  if (type == "like") {
    actions = m_storage->loadLikes();
  } else {
    actions = m_storage->loadReplies();
  }

  // Sort by timestamp descending (newest first)
  std::sort(actions.begin(), actions.end(),
            [](const SocialAction &a, const SocialAction &b) {
              return a.timestamp > b.timestamp;
            });

  // Filter out reciprocated if checkbox is checked
  if (m_hideReciprocatedCheck && m_hideReciprocatedCheck->isChecked()) {
    actions.erase(
        std::remove_if(actions.begin(), actions.end(),
                       [](const SocialAction &a) { return a.reciprocated; }),
        actions.end());
  }

  table->setRowCount(actions.size());

  for (int i = 0; i < actions.size(); i++) {
    const SocialAction &action = actions[i];

    // Username
    QString displayName =
        action.userName.isEmpty() ? ("@" + action.userHandle) : action.userName;
    QTableWidgetItem *userItem = new QTableWidgetItem(displayName);
    userItem->setData(Qt::UserRole, action.id);
    userItem->setData(Qt::UserRole + 1, action.userHandle);
    userItem->setData(Qt::UserRole + 2, action.reciprocated);
    userItem->setToolTip("@" + action.userHandle);
    table->setItem(i, 0, userItem);

    // Time
    QDateTime dt = QDateTime::fromString(action.timestamp, Qt::ISODate);
    QString timeStr = dt.isValid() ? dt.toLocalTime().toString("MM-dd HH:mm")
                                   : action.timestamp;
    QTableWidgetItem *timeItem = new QTableWidgetItem(timeStr);
    timeItem->setToolTip(action.timestamp);
    table->setItem(i, 1, timeItem);

    // Post snippet
    QString snippet = action.postSnippet;
    if (snippet.length() > 50) {
      snippet = snippet.left(50) + "...";
    }
    QTableWidgetItem *snippetItem = new QTableWidgetItem(snippet);
    snippetItem->setToolTip(action.postSnippet);
    table->setItem(i, 2, snippetItem);

    // Status
    QString status =
        action.reciprocated
            ? QString::fromUtf8(
                  "\xe2\x9c\x85 \xe5\xb7\xb2\xe5\x9b\x9e\xe9\xa6\x88")
            : QString::fromUtf8(
                  "\xe2\x8f\xb3 \xe5\xbe\x85\xe5\x9b\x9e\xe9\xa6\x88");
    QTableWidgetItem *statusItem = new QTableWidgetItem(status);
    if (action.reciprocated) {
      statusItem->setForeground(QColor("#4caf50"));
    } else {
      statusItem->setForeground(QColor("#ff9800"));
    }
    table->setItem(i, 3, statusItem);
  }
}

void ActionListPanel::refreshLikes() {
  populateTable(m_likeTable, "like");
  updateStats();
}

void ActionListPanel::refreshReplies() {
  populateTable(m_replyTable, "reply");
  updateStats();
}

void ActionListPanel::refreshAll() {
  refreshLikes();
  refreshReplies();
}

void ActionListPanel::updateStats() {
  int likes = m_storage->likeCount();
  int replies = m_storage->replyCount();
  int pendingLikes = m_storage->pendingLikeCount();
  int pendingReplies = m_storage->pendingReplyCount();

  m_statsLabel->setText(
      QString::fromUtf8("\xe2\x9d\xa4\xef\xb8\x8f \xe7\x82\xb9\xe8\xb5\x9e: %1 "
                        "(\xe5\xbe\x85\xe5\x9b\x9e\xe9\xa6\x88 %2)  |  "
                        "\xf0\x9f\x92\xac \xe5\x9b\x9e\xe5\xa4\x8d: %3 "
                        "(\xe5\xbe\x85\xe5\x9b\x9e\xe9\xa6\x88 %4)")
          .arg(likes)
          .arg(pendingLikes)
          .arg(replies)
          .arg(pendingReplies));
}

void ActionListPanel::onNewLike(const QString &userName,
                                const QString &timestamp) {
  refreshLikes();
}

void ActionListPanel::onNewReply(const QString &userName,
                                 const QString &timestamp) {
  refreshReplies();
}

void ActionListPanel::onLikeContextMenu(const QPoint &pos) {
  QTableWidgetItem *item = m_likeTable->itemAt(pos);
  if (!item)
    return;

  int row = item->row();
  QTableWidgetItem *userItem = m_likeTable->item(row, 0);
  if (!userItem)
    return;

  QString actionId = userItem->data(Qt::UserRole).toString();
  QString userHandle = userItem->data(Qt::UserRole + 1).toString();

  QMenu menu(this);
  menu.setStyleSheet("QMenu { background: #1a1a2e; color: #d0d0d0; border: 1px "
                     "solid #3a3a5a; }"
                     "QMenu::item:selected { background: #2a3a5e; }");

  QAction *markAction = menu.addAction(QString::fromUtf8(
      "\xe2\x9c\x85 "
      "\xe6\xa0\x87\xe8\xae\xb0\xe5\xb7\xb2\xe5\x9b\x9e\xe9\xa6\x88"));
  QAction *unmarkAction =
      menu.addAction(QString::fromUtf8("\xe2\x8f\xb3 "
                                       "\xe5\x8f\x96\xe6\xb6\x88\xe5\x9b\x9e"
                                       "\xe9\xa6\x88\xe6\xa0\x87\xe8\xae\xb0"));
  menu.addSeparator();
  QAction *openProfileAction =
      menu.addAction(QString::fromUtf8("\xf0\x9f\x94\x97 "
                                       "\xe6\x89\x93\xe5\xbc\x80\xe7\x94\xa8"
                                       "\xe6\x88\xb7\xe4\xb8\xbb\xe9\xa1\xb5"));

  QAction *selected = menu.exec(m_likeTable->viewport()->mapToGlobal(pos));

  if (selected == markAction) {
    onMarkReciprocated(actionId);
    m_storage->markReciprocated(actionId, true);
    refreshLikes();
  } else if (selected == unmarkAction) {
    m_storage->markReciprocated(actionId, false);
    refreshLikes();
  } else if (selected == openProfileAction) {
    QDesktopServices::openUrl(QUrl("https://x.com/" + userHandle));
  }
}

void ActionListPanel::onReplyContextMenu(const QPoint &pos) {
  QTableWidgetItem *item = m_replyTable->itemAt(pos);
  if (!item)
    return;

  int row = item->row();
  QTableWidgetItem *userItem = m_replyTable->item(row, 0);
  if (!userItem)
    return;

  QString actionId = userItem->data(Qt::UserRole).toString();
  QString userHandle = userItem->data(Qt::UserRole + 1).toString();

  QMenu menu(this);
  menu.setStyleSheet("QMenu { background: #1a1a2e; color: #d0d0d0; border: 1px "
                     "solid #3a3a5a; }"
                     "QMenu::item:selected { background: #2a3a5e; }");

  QAction *markAction = menu.addAction(QString::fromUtf8(
      "\xe2\x9c\x85 "
      "\xe6\xa0\x87\xe8\xae\xb0\xe5\xb7\xb2\xe5\x9b\x9e\xe9\xa6\x88"));
  QAction *unmarkAction =
      menu.addAction(QString::fromUtf8("\xe2\x8f\xb3 "
                                       "\xe5\x8f\x96\xe6\xb6\x88\xe5\x9b\x9e"
                                       "\xe9\xa6\x88\xe6\xa0\x87\xe8\xae\xb0"));
  menu.addSeparator();
  QAction *openProfileAction =
      menu.addAction(QString::fromUtf8("\xf0\x9f\x94\x97 "
                                       "\xe6\x89\x93\xe5\xbc\x80\xe7\x94\xa8"
                                       "\xe6\x88\xb7\xe4\xb8\xbb\xe9\xa1\xb5"));

  QAction *selected = menu.exec(m_replyTable->viewport()->mapToGlobal(pos));

  if (selected == markAction) {
    m_storage->markReciprocated(actionId, true);
    refreshReplies();
  } else if (selected == unmarkAction) {
    m_storage->markReciprocated(actionId, false);
    refreshReplies();
  } else if (selected == openProfileAction) {
    QDesktopServices::openUrl(QUrl("https://x.com/" + userHandle));
  }
}

void ActionListPanel::onMarkReciprocated(const QString &actionId) {
  // Already handled in context menu
}

void ActionListPanel::loadHideReciprocatedSetting() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  bool hide = settings.value("hideReciprocated", true).toBool();
  m_hideReciprocatedCheck->setChecked(hide);
}

void ActionListPanel::saveHideReciprocatedSetting() {
  QSettings settings("XSocialLedger", "XSocialLedger");
  settings.setValue("hideReciprocated", m_hideReciprocatedCheck->isChecked());
}
