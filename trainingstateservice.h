#pragma once

#include <QDate>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

class QJsonDocument;

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
        int orderIndex = 0;
    };

    struct PlannedInfo {
        QVector<Block> blocks;
        bool available = false;
        bool alreadyStartedToday = false;
        int totalTasks = 0;
    };

    struct AccountInfo {
        QString accountNumber;
        QString login;
        QString password;
        bool isValid() const { return !accountNumber.trimmed().isEmpty(); }
    };

    struct SessionTask {
        QString code;
        int taskNo = 0;
        QString taskTitle;
        QString variation;
        int variationIndex = 0;
        int itemId = 0;
        QString imageUrl;
        QString answer;
        bool isWritten = false;
        bool solved = false;
        bool hasAttempt = false;
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

    AccountInfo accountInfo() const;
    QVector<SessionTask> buildSessionTasks(const QVector<Block>& blocks) const;
    QString makeTaskCode(int taskNo, const QString& variation, int itemId) const;

    QString loadSavedTestAnswer(const QString& taskCode) const;
    void saveTestAnswer(const QString& taskCode, const QString& answer);

    bool markWrittenTaskSolved(const SessionTask& task);

private:
    struct CatalogItem {
        int id = 0;
        QString url;
        QString answer;
    };

    struct CatalogVariation {
        QString name;
        int total = 0;
        int orderIndex = 0;
        QVector<CatalogItem> items;
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
    bool loadAccount() const;

    QVector<Block> blocksFromJsonString(const QString& raw) const;
    QString blocksToJsonString(const QVector<Block>& blocks) const;

    CatalogVariation variationData(int taskNo, const QString& variation) const;
    bool isSolvedOrAttempted(int taskNo, const QString& variation, int itemId) const;
    bool writeJsonFile(const QString& filePath, const QJsonDocument& doc) const;

private:
    mutable bool loaded_ = false;
    mutable QMap<int, CatalogTask> catalog_;
    mutable QMap<QString, QMap<int, bool>> progressDone_;
    mutable QMap<QString, QMap<int, bool>> submissionsAttempted_;
    mutable QMap<QDate, QVector<Block>> plannedByDate_;
    mutable AccountInfo accountInfo_;
};
