#include "StatsPanel.h"
#include "Data/DataStorage.h"
#include "Data/SocialAction.h"
#include <QDateTime>
#include <QMap>
#include <QVBoxLayout>

StatsPanel::StatsPanel(DataStorage *storage, QWidget *parent)
    : QWidget(parent), m_storage(storage) {

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  m_calendar = new QCalendarWidget(this);
  m_calendar->setMaximumHeight(200);
  m_calendar->setSelectedDate(QDate::currentDate());
  m_calendar->setStyleSheet(
      "QCalendarWidget { background: #0f0f23; color: #d0d0d0; }"
      "QCalendarWidget QToolButton { color: #d0d0d0; background: #1a1a2e; "
      "  border: 1px solid #2a2a4a; padding: 4px; }"
      "QCalendarWidget QMenu { background: #1a1a2e; color: #d0d0d0; }"
      "QCalendarWidget QSpinBox { background: #1a1a2e; color: #d0d0d0; "
      "  border: 1px solid #2a2a4a; }"
      "QCalendarWidget QAbstractItemView { background: #0f0f23; color: "
      "#d0d0d0; "
      "  selection-background-color: #1e3a5f; selection-color: white; }"
      "QCalendarWidget QAbstractItemView:enabled { color: #d0d0d0; }"
      "QCalendarWidget QWidget#qt_calendar_navigationbar { background: "
      "#1a1a2e; }");
  connect(m_calendar, &QCalendarWidget::selectionChanged, this,
          [this]() { onDateSelected(m_calendar->selectedDate()); });
  layout->addWidget(m_calendar);

  m_textEdit = new QTextEdit(this);
  m_textEdit->setReadOnly(true);
  m_textEdit->setStyleSheet(
      "QTextEdit { background: #0f0f23; color: #d0d0d0; "
      "  border: 1px solid #2a2a4a; font-family: 'Consolas', monospace; "
      "  font-size: 12px; padding: 8px; }");
  layout->addWidget(m_textEdit);

  // Initial load
  generateMarkdown(QDate::currentDate());
}

void StatsPanel::refresh() { generateMarkdown(m_calendar->selectedDate()); }

void StatsPanel::onDateSelected(const QDate &date) { generateMarkdown(date); }

void StatsPanel::generateMarkdown(const QDate &date) {
  QList<SocialAction> actions = m_storage->getReciprocatedByDate(date);

  // Group by user
  struct UserStats {
    QString userName;
    int likes = 0;
    int replies = 0;
  };
  QMap<QString, UserStats> userMap;

  for (const auto &a : actions) {
    auto &stats = userMap[a.userHandle];
    if (stats.userName.isEmpty()) {
      stats.userName = a.userName.isEmpty() ? ("@" + a.userHandle) : a.userName;
    }
    if (a.type == "like")
      stats.likes++;
    else
      stats.replies++;
  }

  // Sort by total descending
  QList<QPair<QString, UserStats>> sorted;
  for (auto it = userMap.begin(); it != userMap.end(); ++it) {
    sorted.append({it.key(), it.value()});
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const QPair<QString, UserStats> &a,
               const QPair<QString, UserStats> &b) {
              return (a.second.likes + a.second.replies) >
                     (b.second.likes + b.second.replies);
            });

  // Count totals
  int totalLikes = 0, totalReplies = 0;
  for (const auto &p : sorted) {
    totalLikes += p.second.likes;
    totalReplies += p.second.replies;
  }

  // Generate Markdown
  QString md;
  QString dateStr = date.toString("yyyy-MM-dd");
  QString dayOfWeek = date.toString("dddd");

  md += QString("# %1 %2\n\n").arg(dateStr, dayOfWeek);
  md += QString("## Daily Reciprocation Summary\n\n");
  md += QString("- Total Reciprocated Users: **%1**\n").arg(sorted.size());
  md += QString("- Total Likes Given: **%1**\n").arg(totalLikes);
  md += QString("- Total Replies Given: **%1**\n").arg(totalReplies);
  md += QString("- Grand Total: **%1**\n\n").arg(totalLikes + totalReplies);

  if (sorted.isEmpty()) {
    md += "*No reciprocation records for this date.*\n";
  } else {
    md += "| # | User | Likes | Replies | Total |\n";
    md += "|---|------|-------|---------|-------|\n";
    int rank = 1;
    for (const auto &p : sorted) {
      int total = p.second.likes + p.second.replies;
      md += QString("| %1 | @%2 | %3 | %4 | %5 |\n")
                .arg(rank++)
                .arg(p.first)
                .arg(p.second.likes)
                .arg(p.second.replies)
                .arg(total);
    }
  }

  m_textEdit->setPlainText(md);
}
