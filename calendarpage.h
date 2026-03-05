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
#include <QColor>
class QCalendarWidget;
class QLabel;
class QComboBox;
class QPushButton;
class QTableView;

class CalendarPage final : public QWidget {
    Q_OBJECT
public:
    struct DaySession {
        QDate date;
        int doneCount = 0;
        double correctCount = 0.0;
        int durationMin = 0;
        QString type;     // "training" | "mock" | "rest"
        int mockScore = -1;
        int mockMax = 32;
        QMap<int, QPair<int,double>> byTask; // taskNo -> (attempts, correctWeighted)
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

    struct CellViz {
        QColor bg;            // background color
        bool mockBorder = false;
        double barFill = 0.0;   // 0..1 volume fill
        double barHeight = 0.0; // 0..1 accuracy height
        int streakBadge = 0;    // 🔥N on the day (usually today)
        bool critical = false;  // highlight day where streak may break
        QString tooltip;
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
    bool eventFilter(QObject* watched, QEvent* event) override;

    void buildUi();
    void applyStyles();
    void refreshAll();
    void paintCalendar();
    void rebuildVizMap();
    void showDayDetails(const QDate& d);
    QDate dateAtCalendarPos(const QPoint& pos) const;
    QString tooltipForDate(const QDate& d) const;

    struct QualityParts {
        int score = 0;          // 0..100
        double volume = 0.0;    // 0..1
        double accuracy = 0.0;  // 0..1
        double diversity = 0.0; // 0..1
        double focusWeak = 0.0; // 0..1
        QString explain;
    };

    static QualityParts computeQualityParts(int done, int correct, int distinct, double focusWeak);
    static int computeStreakDays(const QVector<DaySession>& sessions);
    static int riskFromMastery(int mastery);

private:
    // data
    QVector<DaySession> sessions_;
    QMap<int, TaskAggLite> tasks_;
    OverviewLite overview_;
    QJsonObject plan_;
    QMap<QDate, CellViz> viz_;
    int trainingsPerWeek_ = 3;
    QSet<int> plannedWeekdays_; // 1..7

    // ui
QLabel* title_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QLabel* monthLabel_ = nullptr;

    QCalendarWidget* cal_ = nullptr;
    QTableView* calView_ = nullptr; // internal calendar grid view
    QDate lastHoverDate_;
    QDate selectedDate_ = QDate::currentDate();

    // legend
    QLabel* legendPlanned_ = nullptr;
    QLabel* legendDone_ = nullptr;
    QLabel* legendFree_ = nullptr;
    QLabel* legendMissed_ = nullptr;
    QLabel* legendMock_ = nullptr;
    QLabel* legendStreak_ = nullptr;

    QLabel* details_ = nullptr;
};
