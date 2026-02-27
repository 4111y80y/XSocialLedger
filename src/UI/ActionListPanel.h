#ifndef ACTIONLISTPANEL_H
#define ACTIONLISTPANEL_H

#include <QCheckBox>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

class DataStorage;
class StatsPanel;

// 社交互动记录面板 (右侧面板)
class ActionListPanel : public QWidget {
  Q_OBJECT

public:
  explicit ActionListPanel(DataStorage *storage, QWidget *parent = nullptr);
  ~ActionListPanel();

  // 刷新列表
  void refreshLikes();
  void refreshReplies();
  void refreshAll();

  // 更新统计
  void updateStats();

public slots:
  void onNewLike(const QString &userName, const QString &timestamp);
  void onNewReply(const QString &userName, const QString &timestamp);

signals:
  void reciprocateLikeRequested(const QString &userHandle,
                                const QString &actionId);

private slots:
  void onLikeContextMenu(const QPoint &pos);
  void onReplyContextMenu(const QPoint &pos);
  void onMarkReciprocated(const QString &actionId);

private:
  void setupUI();
  void populateTable(QTableWidget *table, const QString &type);
  void loadHideReciprocatedSetting();
  void saveHideReciprocatedSetting();
  void loadOnly24hSetting();
  void saveOnly24hSetting();

  DataStorage *m_storage;
  QTabWidget *m_tabWidget;
  QTableWidget *m_likeTable;
  QTableWidget *m_replyTable;
  QLabel *m_statsLabel;
  QCheckBox *m_hideReciprocatedCheck;
  QCheckBox *m_only24hCheck;
  StatsPanel *m_statsPanel;
};

#endif // ACTIONLISTPANEL_H
