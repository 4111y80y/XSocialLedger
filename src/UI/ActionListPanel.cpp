#include "ActionListPanel.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFont>
#include <QHeaderView>
#include <QMenu>
#include <QUrl>
#include <QVBoxLayout>


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
  QLabel *titleLabel = new QLabel("📒 社交互动记录", this);
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
  m_likeTable->setHorizontalHeaderLabels({"用户", "时间", "帖子片段", "状态"});
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
  m_tabWidget->addTab(m_likeTable, "❤️ 点赞");

  // 回复表格
  m_replyTable = new QTableWidget(this);
  m_replyTable->setColumnCount(4);
  m_replyTable->setHorizontalHeaderLabels({"用户", "时间", "帖子片段", "状态"});
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
  m_tabWidget->addTab(m_replyTable, "💬 回复");

  layout->addWidget(m_tabWidget);
}

void ActionListPanel::populateTable(QTableWidget *table, const QString &type) {
  QList<SocialAction> actions;
  if (type == "like") {
    actions = m_storage->loadLikes();
  } else {
    actions = m_storage->loadReplies();
  }

  table->setRowCount(actions.size());

  for (int i = 0; i < actions.size(); i++) {
    const SocialAction &action = actions[i];

    // 用户名
    QString displayName =
        action.userName.isEmpty() ? ("@" + action.userHandle) : action.userName;
    QTableWidgetItem *userItem = new QTableWidgetItem(displayName);
    userItem->setData(Qt::UserRole, action.id);
    userItem->setData(Qt::UserRole + 1, action.userHandle);
    userItem->setToolTip("@" + action.userHandle);
    table->setItem(i, 0, userItem);

    // 时间
    QDateTime dt = QDateTime::fromString(action.timestamp, Qt::ISODate);
    QString timeStr = dt.isValid() ? dt.toLocalTime().toString("MM-dd HH:mm")
                                   : action.timestamp;
    QTableWidgetItem *timeItem = new QTableWidgetItem(timeStr);
    timeItem->setToolTip(action.timestamp);
    table->setItem(i, 1, timeItem);

    // 帖子片段
    QString snippet = action.postSnippet;
    if (snippet.length() > 50) {
      snippet = snippet.left(50) + "...";
    }
    QTableWidgetItem *snippetItem = new QTableWidgetItem(snippet);
    snippetItem->setToolTip(action.postSnippet);
    table->setItem(i, 2, snippetItem);

    // 状态
    QString status = action.reciprocated ? "✅ 已回馈" : "⏳ 待回馈";
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
      QString("❤️ 点赞: %1 (待回馈 %2)  |  💬 回复: %3 (待回馈 %4)")
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

  QAction *markAction = menu.addAction("✅ 标记已回馈");
  QAction *unmarkAction = menu.addAction("⏳ 取消回馈标记");
  menu.addSeparator();
  QAction *openProfileAction = menu.addAction("🔗 打开用户主页");

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

  QAction *markAction = menu.addAction("✅ 标记已回馈");
  QAction *unmarkAction = menu.addAction("⏳ 取消回馈标记");
  menu.addSeparator();
  QAction *openProfileAction = menu.addAction("🔗 打开用户主页");

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
  // 已在上下文菜单处理中调用
}
