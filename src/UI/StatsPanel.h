#ifndef STATSPANEL_H
#define STATSPANEL_H

#include <QCalendarWidget>
#include <QTextEdit>
#include <QWidget>

class DataStorage;

class StatsPanel : public QWidget {
  Q_OBJECT

public:
  explicit StatsPanel(DataStorage *storage, QWidget *parent = nullptr);
  void refresh();

private slots:
  void onDateSelected(const QDate &date);

private:
  void generateMarkdown(const QDate &date);

  DataStorage *m_storage;
  QCalendarWidget *m_calendar;
  QTextEdit *m_textEdit;
};

#endif // STATSPANEL_H
