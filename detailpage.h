#pragma once

#include <QWidget>
#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QString>

#include <QSortFilterProxyModel>

class QTabWidget;
class QStackedWidget;
class QListWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QTableView;
class QTableWidget;
class QStandardItemModel;
class QTextEdit;

class QChartView;

struct DetailEvent {
    QDateTime ts;
    QString source;      // "app" | "bot"
    int taskNo = 0;
    QString varKey;      // key in catalog
    QString varName;     // displayName
    int taskId = 0;
    QString result;      // "ok" | "partial" | "wrong"
    int score = -1;      // bot only
    QDate sessionDate;   // if day found in sessions
    QString sessionType; // training/mock/rest/unknown
};

class EventsProxyFilter final : public QSortFilterProxyModel {
public:
    explicit EventsProxyFilter(QObject* parent = nullptr);

    void setTextQuery(const QString& q);
    void setPeriodDays(int days);                // 0 = all
    void setSourceFilter(const QString& s);      // all/app/bot
    void setSessionTypeFilter(const QString& s); // all/training/mock/rest
    void setOnlyWrong(bool v);
    void setOnlyPartial(bool v);
    void setTaskFilter(int taskNo);              // 0 = all

    void refresh();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    QString q_;
    int periodDays_ = 0;
    QString source_ = "all";
    QString sessType_ = "all";
    bool onlyWrong_ = false;
    bool onlyPartial_ = false;
    int taskFilter_ = 0;
};

class DetailPage final : public QWidget {
    Q_OBJECT
public:
    explicit DetailPage(QWidget* parent = nullptr);

    // dataDir: directory that contains json files (or its /data subdir)
    void reloadFromDataDir(const QString& dataDir);

public slots:
    void setQuickFilterTask(int taskNo); // 0 = clear

private slots:
    void onNavChanged();
    void onFiltersChanged();
    void onEventSelectionChanged();
    void onDayPickedFromCalendar(const QDate& d); // optional external hook
    void onShowTaskEventsClicked();

private:
    // file io
    QString resolvePath(const QString& name) const;
    bool readJson(const QString& name, QJsonDocument& out, QString* err = nullptr) const;

    void buildUi();
    void buildEventsTab(QWidget* page);
    void buildDayTab(QWidget* page);
    void buildTaskTab(QWidget* page);
    void buildCheckTab(QWidget* page);

    void loadAll(); // loads json into caches + rebuild models

    void rebuildEventsModel();
    void rebuildDayView(const QDate& d);
    void rebuildTaskView(int taskNo);
    void runChecks();

    QString fmtResult(const QString& r) const;
    QString safeVarName(int taskNo, const QString& varKey) const;

private:
    QString dataDir_;

    // catalog: task -> (title, varKey->displayName, varKey->itemsTotal)
    struct VarInfo { QString name; int total = 0; };
    struct TaskInfo { QString title; QMap<QString, VarInfo> vars; };
    QMap<int, TaskInfo> catalog_;

    // sessions by date
    struct DaySession {
        QDate date;
        int doneCount = 0;
        double correctCount = 0.0;
        int durationMin = 0;
        QString type;
        int mockScore = -1;
        int mockMax = 32;
        QMap<int, QPair<int,double>> byTask; // taskNo -> (attempts, correctWeighted)
    };
    QMap<QDate, DaySession> sessionsByDate_;
    QMap<int, QDate> lastTouchByTask_; // from sessions

    QVector<DetailEvent> events_; // all events

    // UI
    QTabWidget* topTabs_ = nullptr;
    QListWidget* nav_ = nullptr;
    QStackedWidget* stack_ = nullptr;

    // events page
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* periodBox_ = nullptr;
    QComboBox* sourceBox_ = nullptr;
    QComboBox* sessTypeBox_ = nullptr;
    QCheckBox* onlyWrongBox_ = nullptr;
    QCheckBox* onlyPartialBox_ = nullptr;
    QComboBox* taskPickBox_ = nullptr;

    QTableView* eventsView_ = nullptr;
    QStandardItemModel* eventsModel_ = nullptr;
    EventsProxyFilter* eventsProxy_ = nullptr;

    QTextEdit* daySummary_ = nullptr;
    QTableWidget* dayByTask_ = nullptr;
    QTableWidget* dayByVar_ = nullptr;

    // day page
    QLabel* dayTitle_ = nullptr;
    QTextEdit* dayText_ = nullptr;
    QTableWidget* dayTasksTable_ = nullptr;

    // task page
    QComboBox* taskBox_ = nullptr;
    QTextEdit* taskText_ = nullptr;
    QTableWidget* taskVarsTable_ = nullptr;
    QPushButton* showTaskEventsBtn_ = nullptr;
    QChartView* taskAttemptsChart_ = nullptr;
    QChartView* taskAccuracyChart_ = nullptr;

    // checks page
    QTextEdit* checkText_ = nullptr;

    QDate selectedDate_;
    int selectedTaskNo_ = 0;
};
