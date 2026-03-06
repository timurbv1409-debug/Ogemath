#pragma once

#include <QDate>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

class TrainingStateService final : public QObject
{
    Q_OBJECT
public:
    explicit TrainingStateService(QObject* parent = nullptr);

    struct Block {
        int taskNo = 0;
        QString variation;
        int count = 0;
    };

    struct VariationInfo {
        QString name;
        int total = 0;
    };

    struct PlannedInfo {
        QVector<Block> blocks;
        bool available = false;
        bool alreadyStartedToday = false;
        int totalTasks = 0;
    };

    QString dataPath(const QString& fileName) const;

    QString taskTitle(int taskNo) const;
    QVector<VariationInfo> variationsForTask(int taskNo) const;
    int totalTasksInVariation(int taskNo, const QString& variation) const;
    int solvedTasksInVariation(int taskNo, const QString& variation) const;
    int remainingTasksInVariation(int taskNo, const QString& variation) const;

    QVector<Block> loadManualDraftForToday();
    void saveManualDraftForToday(const QVector<Block>& blocks);
    void clearManualDraft();
    bool hasManualDraftForToday() const;

    PlannedInfo plannedInfoForToday() const;
    void markPlannedStartedToday();

    int totalCount(const QVector<Block>& blocks) const;
    QString manualDraftStatusText() const;
    QString plannedStatusText() const;

private:
    struct CatalogVariation {
        QString name;
        int total = 0;
    };
    struct CatalogTask {
        QString title;
        QMap<QString, CatalogVariation> variations;
    };

    bool ensureLoaded() const;
    bool loadCatalog() const;
    bool loadProgress() const;
    bool loadSubmissions() const;
    bool loadPlan() const;

    QVector<Block> blocksFromJsonString(const QString& raw) const;
    QString blocksToJsonString(const QVector<Block>& blocks) const;

private:
    mutable bool loaded_ = false;
    mutable QMap<int, CatalogTask> catalog_;
    mutable QMap<QString, QMap<int, bool>> progressDone_;
    mutable QMap<QString, QMap<int, bool>> submissionsAttempted_;
    mutable QMap<QDate, QVector<Block>> plannedByDate_;
};
