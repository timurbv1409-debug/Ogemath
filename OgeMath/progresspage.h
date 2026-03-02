#pragma once

#include <QWidget>
#include <QMap>
#include <QDate>
#include <QDateTime>
#include <QJsonDocument>
#include <QString>
#include <QVector>
#include <QList>

class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QFrame;
class QFileSystemWatcher;
class QTimer;
class QPushButton;

class QChartView;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QScrollArea;
class QGridLayout;
class CalendarPage;

class ProgressPage final : public QWidget {
    Q_OBJECT
public:
    explicit ProgressPage(QWidget* parent = nullptr);
    void reloadAllData();

signals:
    void backRequested();

private slots:
    void onWatchedChanged();
    void onChartsNavSelectionChanged();
    void onChartsControlsChanged();

private:
    QString dataPath(const QString& fileName) const;
    bool readJsonFile(const QString& path, QJsonDocument& outDoc, QString* err = nullptr) const;

    bool loadCatalog(QString* err = nullptr);
    bool loadProgress(QString* err = nullptr);
    bool loadSessions(QString* err = nullptr);
    bool loadSubmissions(QString* err = nullptr);

    struct TaskItemRef {
        int taskNo = 0;
        QString variation;
        int taskId = 0;
    };

    struct AttemptEvent {
        TaskItemRef ref;
        bool correct = false;
        bool partial = false;
        int score = -1;
        QDateTime ts;
        QString source; // "app"|"bot"
    };

    struct VarAgg {
        int itemsTotal = 0;
        int targetAttempts = 20;

        int attemptedUnique = 0;
        int correctUnique   = 0;
        int partialUnique   = 0;
        int wrongUnique     = 0;

        double accuracy = 0.0;
        double confidence = 0.0;
        int mastery = 0;
        int risk = 0;
        QDateTime lastTs;
    };

    struct TaskAgg {
        int taskNo = 0;
        QString title;
        int maxPoints = 1;

        int attemptedUnique = 0;
        int correctUnique   = 0;
        int partialUnique   = 0;

        int mastery = 0;
        int risk = 0;
        QDateTime lastTs;

        QMap<QString, VarAgg> vars;
    };

    struct OverviewStats {
        double expectedPoints = 0.0;
        int streakDays = 0;
        QDateTime lastSessionTs;
        QList<int> weakTaskNos;
        QList<int> lowDataTaskNos;
    };

    struct DaySession {
        QDate date;
        int doneCount = 0;
        int correctCount = 0;
        int durationMin = 0;
        QString type; // training/mock/rest
        int mockScore = -1;
        int mockMax = 32;
        QMap<int, QPair<int,int>> byTask;
    };

    QVector<AttemptEvent> buildAttemptEvents(int typeFilter) const; // 0 all, 1 test, 2 written
    QMap<int, TaskAgg> computeAggregates(const QVector<AttemptEvent>& events) const;
    OverviewStats computeOverview(const QMap<int, TaskAgg>& tasks) const;

    void computeDerivedMetricsForVar(VarAgg& v) const;
    void computeDerivedMetricsForTask(TaskAgg& t) const;

    void buildUi();
    QWidget* buildOverviewTab();
    QWidget* buildPlaceholderTab(const QString& title, const QString& hint);

    QWidget* buildChartsTab();
    void rebuildChartsNav(const QMap<int, TaskAgg>& tasks, const OverviewStats& ov);
    void renderCharts(const QMap<int, TaskAgg>& tasks, const OverviewStats& ov);

    QFrame* makeStatCard(const QString& title, const QString& tooltip);
    void setCardValue(QFrame* card, const QString& value, const QString& sub = QString());
    void renderOverview(const OverviewStats& ov, const QMap<int, TaskAgg>& tasks);
    void renderTaskTree(const QMap<int, TaskAgg>& tasks);
    void addTaskRow(const TaskAgg& t);
    void addVariationRow(QTreeWidgetItem* parent, int taskNo, const QString& varKey, const VarAgg& v);

    QWidget* makeMasteryBar(int mastery) const;
    QString riskLabelText(int risk) const;
    QString lastDateText(const QDateTime& dt) const;

    QVector<QPointF> rollingMean(const QVector<QPointF>& pts, int window) const;
    void linearRegression(const QVector<QPointF>& pts, double& a, double& b) const;
    double stddev(const QVector<double>& v) const;

    void applyProductStyles();

    void setupWatcher();
    void refreshWatcherPaths();

    struct CatalogVarInfo { int itemsTotal = 0; QString displayName; };
    struct CatalogTaskInfo { QString title; QMap<QString, CatalogVarInfo> vars; };
    QMap<int, CatalogTaskInfo> catalog_;

    QMap<QString, QMap<int,bool>> progressDone_;
    QVector<DaySession> sessionDays_;
    QVector<AttemptEvent> submissionEvents_;

    QTabWidget* tabs_ = nullptr;

    QWidget* overviewTab_ = nullptr;
    QPushButton* backBtn_ = nullptr;
    QFrame* cardExpected_ = nullptr;
    QFrame* cardStreak_ = nullptr;
    QFrame* cardLast_ = nullptr;
    QFrame* cardFocus_ = nullptr;
    QLabel* weakLabel_ = nullptr;
    QLabel* lowDataLabel_ = nullptr;
    QTreeWidget* tree_ = nullptr;

    QWidget* chartsTab_ = nullptr;
    CalendarPage* calendarTab_ = nullptr;

    QComboBox* chartsPeriod_ = nullptr;
    QComboBox* chartsSource_ = nullptr;
    QComboBox* chartsType_ = nullptr;
    QCheckBox* chartsForecast_ = nullptr;

    QCheckBox* chartsCompare_ = nullptr;
    QComboBox* chartsCompareTask_ = nullptr;
    int compareTaskNo_ = 0;

    QTreeWidget* chartsNav_ = nullptr;
    int selectedTaskNo_ = 0;

    QScrollArea* chartsScroll_ = nullptr;
    QWidget* chartsDash_ = nullptr;
    QGridLayout* chartsGrid_ = nullptr;

    QChartView* cvScoreTrend_ = nullptr;
    QChartView* cvMocks_ = nullptr;
    QChartView* cvStability_ = nullptr;
    QChartView* cvActivity_ = nullptr;
    QChartView* cvContrib_ = nullptr;
    QChartView* cvBottlenecks_ = nullptr;

    // Extra overall charts
    QChartView* cvAccuracyTrend_ = nullptr;
    QChartView* cvWeeklyLoad_ = nullptr;
    QChartView* cvRiskTrend_ = nullptr;

    QChartView* cvTaskVarMastery_ = nullptr;
    QChartView* cvTaskAttempts_ = nullptr;
    QChartView* cvForgetting_ = nullptr;

    QLabel* chartsInfo_ = nullptr;
    QTableWidget* heatmap_ = nullptr;

    // Cached aggregates for fast re-render when switching nav selection
    QMap<int, TaskAgg> cachedTasks_;
    OverviewStats cachedOverview_;
    bool chartsNavRebuilding_ = false;

    QFileSystemWatcher* watcher_ = nullptr;
    QTimer* reloadDebounce_ = nullptr;
};
