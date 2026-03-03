#pragma once
#include <QWidget>
#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QVector>
#include <QJsonObject>
#include <QTextCharFormat>
class QCalendarWidget;
class PlanCalendarWidget;
class QLabel;
class QComboBox;
class QPushButton;

class CalendarPage final : public QWidget {
    Q_OBJECT
public:
    struct DaySession {
        QDate date;
        int doneCount = 0;
        int correctCount = 0;
        int durationMin = 0;
        QString type;     // "training" | "mock" | "rest"
        int mockScore = -1;
        int mockMax = 32;
        QMap<int, QPair<int,int>> byTask; // taskNo -> (attempts, correct)
    };

    struct VarAggLite {
        int mastery = 0;
        QString displayName; // красивое имя вариации
    };

    struct TaskAggLite {
        int taskNo = 0;
        QString title;
        int mastery = 0;
        QMap<QString, VarAggLite> vars; // varKey -> info
    };

    struct OverviewLite {
        QList<int> weakTaskNos;
        int streakDays = 0;
    };

    explicit CalendarPage(QWidget* parent = nullptr);

    // planJson — объект из plan.json (root object)
    // trainingsPerWeek — сколько занятий в неделю из settings.json
    // plannedWeekdays — дни недели (1..7), когда обычно занятия (если хочешь подсветку)
    void setData(const QVector<DaySession>& sessions,
                 const QMap<int, TaskAggLite>& tasks,
                 const OverviewLite& overview,
                 const QJsonObject& planJson,
                 int trainingsPerWeek,
                 const QSet<int>& plannedWeekdays);

signals:
    void backRequested();

private slots:
    void onPrevMonth();
    void onNextMonth();
    void onMonthChanged(int year, int month);
    void onDayClicked(const QDate& d);

private:
    void buildUi();
    void applyStyles();
    void refreshAll();
    void paintCalendar();
    void showDayDetails(const QDate& d);

    static int computeQualityScore(int done, int correct, int distinct);
    static int computeStreakDays(const QVector<DaySession>& sessions);

private:
    // data
    QVector<DaySession> sessions_;
    QMap<int, TaskAggLite> tasks_;
    OverviewLite overview_;
    QJsonObject plan_;
    int trainingsPerWeek_ = 3;
    QSet<int> plannedWeekdays_; // 1..7

    // ui
    QPushButton* backBtn_ = nullptr;
    QLabel* title_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QLabel* monthLabel_ = nullptr;

    PlanCalendarWidget* cal_ = nullptr;

    // legend
    QLabel* legendPlanned_ = nullptr;
    QLabel* legendDone_ = nullptr;
    QLabel* legendMissed_ = nullptr;
    QLabel* legendMock_ = nullptr;

    QLabel* details_ = nullptr;

    // UX: сохраняем выбор пользователя, чтобы календарь не "прыгал" обратно на сегодня
    QDate lastSelectedDate_;
    bool firstShow_ = true;
};
