#include "trainingstateservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace {
static QString todayKey()
{
    return QDate::currentDate().toString(Qt::ISODate);
}

static QString blockStorageKey(int taskNo, const QString& variation)
{
    return QString::number(taskNo) + QStringLiteral("|") + variation;
}
}

TrainingStateService::TrainingStateService(QObject* parent)
    : QObject(parent)
{
}

QString TrainingStateService::dataPath(const QString& fileName) const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString base = QDir::currentPath();

    QStringList candidates;
    candidates << appDir + "/data/" + fileName
               << appDir + "/" + fileName
               << base + "/data/" + fileName
               << base + "/../data/" + fileName
               << base + "/../../data/" + fileName
               << base + "/../../../data/" + fileName
               << base + "/" + fileName
               << base + "/../" + fileName
               << base + "/../../" + fileName;

    for (const QString& path : candidates) {
        const QString clean = QDir::cleanPath(path);
        if (QFile::exists(clean)) return clean;
    }
    return QDir::cleanPath(appDir + "/data/" + fileName);
}

QString TrainingStateService::taskTitle(int taskNo) const
{
    ensureLoaded();
    return catalog_.value(taskNo).title;
}

QVector<TrainingStateService::VariationInfo> TrainingStateService::variationsForTask(int taskNo) const
{
    ensureLoaded();

    QVector<VariationInfo> out;
    const CatalogTask task = catalog_.value(taskNo);
    for (auto it = task.variations.begin(); it != task.variations.end(); ++it) {
        VariationInfo info;
        info.name = it.key();
        info.total = it.value().total;
        out.push_back(info);
    }
    return out;
}

int TrainingStateService::totalTasksInVariation(int taskNo, const QString& variation) const
{
    ensureLoaded();
    return catalog_.value(taskNo).variations.value(variation).total;
}

int TrainingStateService::solvedTasksInVariation(int taskNo, const QString& variation) const
{
    ensureLoaded();

    const QString key = blockStorageKey(taskNo, variation);
    if (taskNo >= 1 && taskNo <= 19) {
        return progressDone_.value(key).size();
    }
    return submissionsAttempted_.value(key).size();
}

int TrainingStateService::remainingTasksInVariation(int taskNo, const QString& variation) const
{
    const int total = totalTasksInVariation(taskNo, variation);
    const int solved = solvedTasksInVariation(taskNo, variation);
    const int remaining = total - solved;
    return remaining > 0 ? remaining : 0;
}

QVector<TrainingStateService::Block> TrainingStateService::loadManualDraftForToday()
{
    QSettings settings("OgeMath", "OgeMath");
    if (settings.value("training/manualDraftDate").toString() != todayKey()) {
        settings.remove("training/manualDraftBlocks");
        settings.setValue("training/manualDraftDate", todayKey());
        return {};
    }
    return blocksFromJsonString(settings.value("training/manualDraftBlocks").toString());
}

void TrainingStateService::saveManualDraftForToday(const QVector<Block>& blocks)
{
    QSettings settings("OgeMath", "OgeMath");
    settings.setValue("training/manualDraftDate", todayKey());
    settings.setValue("training/manualDraftBlocks", blocksToJsonString(blocks));
}

void TrainingStateService::clearManualDraft()
{
    QSettings settings("OgeMath", "OgeMath");
    settings.setValue("training/manualDraftDate", todayKey());
    settings.remove("training/manualDraftBlocks");
}

bool TrainingStateService::hasManualDraftForToday() const
{
    QSettings settings("OgeMath", "OgeMath");
    if (settings.value("training/manualDraftDate").toString() != todayKey()) {
        settings.remove("training/manualDraftBlocks");
        settings.setValue("training/manualDraftDate", todayKey());
        return false;
    }
    const QVector<Block> blocks = blocksFromJsonString(settings.value("training/manualDraftBlocks").toString());
    return !blocks.isEmpty();
}

TrainingStateService::PlannedInfo TrainingStateService::plannedInfoForToday() const
{
    ensureLoaded();

    PlannedInfo info;
    const QDate today = QDate::currentDate();
    info.blocks = plannedByDate_.value(today);
    info.available = !info.blocks.isEmpty();
    info.totalTasks = totalCount(info.blocks);

    QSettings settings("OgeMath", "OgeMath");
    info.alreadyStartedToday = settings.value("training/plannedLastStartedDate").toString() == todayKey();
    return info;
}

void TrainingStateService::markPlannedStartedToday()
{
    QSettings settings("OgeMath", "OgeMath");
    settings.setValue("training/plannedLastStartedDate", todayKey());
}

int TrainingStateService::totalCount(const QVector<Block>& blocks) const
{
    int total = 0;
    for (const Block& block : blocks) total += block.count;
    return total;
}

QString TrainingStateService::manualDraftStatusText() const
{
    QSettings settings("OgeMath", "OgeMath");
    if (settings.value("training/manualDraftDate").toString() != todayKey()) {
        settings.remove("training/manualDraftBlocks");
        settings.setValue("training/manualDraftDate", todayKey());
        return QString::fromUtf8("Набор не сохранён сегодня");
    }

    const QVector<Block> blocks = blocksFromJsonString(settings.value("training/manualDraftBlocks").toString());
    if (blocks.isEmpty()) return QString::fromUtf8("Набор не сохранён сегодня");

    return QString::fromUtf8("Сохранённый набор: %1 блок%2 • %3 задач")
        .arg(blocks.size())
        .arg((blocks.size() % 10 == 1 && blocks.size() % 100 != 11) ? "" : "а")
        .arg(totalCount(blocks));
}

QString TrainingStateService::plannedStatusText() const
{
    const PlannedInfo info = plannedInfoForToday();
    if (!info.available) return QString::fromUtf8("На сегодня план не найден");
    if (info.alreadyStartedToday) return QString::fromUtf8("Сегодня уже запускалась");
    return QString::fromUtf8("По плану: %1 блок%2 • %3 задач")
        .arg(info.blocks.size())
        .arg((info.blocks.size() % 10 == 1 && info.blocks.size() % 100 != 11) ? "" : "а")
        .arg(info.totalTasks);
}

bool TrainingStateService::ensureLoaded() const
{
    if (loaded_) return true;
    loaded_ = loadCatalog() && loadProgress() && loadSubmissions() && loadPlan();
    return loaded_;
}

bool TrainingStateService::loadCatalog() const
{
    catalog_.clear();

    QFile file(dataPath("catalog.json"));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonObject tasks = doc.object().value("tasks").toObject();
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        const int taskNo = it.key().toInt();
        const QJsonObject taskObj = it.value().toObject();

        CatalogTask task;
        task.title = taskObj.value("title").toString();

        const QJsonObject vars = taskObj.value("variations").toObject();
        for (auto vit = vars.begin(); vit != vars.end(); ++vit) {
            CatalogVariation var;
            var.name = vit.key();
            const QJsonObject vObj = vit.value().toObject();
            const QJsonArray items = vObj.value("items").toArray();
            var.total = items.size();
            task.variations.insert(var.name, var);
        }

        catalog_.insert(taskNo, task);
    }
    return true;
}

bool TrainingStateService::loadProgress() const
{
    progressDone_.clear();

    QFile file(dataPath("progress.json"));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonObject progress = doc.object().value("progress").toObject();
    for (auto taskIt = progress.begin(); taskIt != progress.end(); ++taskIt) {
        const int taskNo = taskIt.key().toInt();
        const QJsonObject taskObj = taskIt.value().toObject();
        const QJsonObject vars = taskObj.value("variations").toObject();

        for (auto varIt = vars.begin(); varIt != vars.end(); ++varIt) {
            const QString variation = varIt.key();
            const QJsonObject varObj = varIt.value().toObject();
            const QJsonArray done = varObj.value("done").toArray();

            QMap<int, bool> solvedIds;
            for (const QJsonValue& value : done) {
                const QJsonArray pair = value.toArray();
                if (pair.size() < 2) continue;
                const int taskId = pair.at(0).toInt();
                const bool isDone = pair.at(1).toBool(false);
                if (isDone) solvedIds.insert(taskId, true);
            }
            progressDone_.insert(blockStorageKey(taskNo, variation), solvedIds);
        }
    }
    return true;
}

bool TrainingStateService::loadSubmissions() const
{
    submissionsAttempted_.clear();

    QFile file(dataPath("submissions.json"));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonArray subs = doc.object().value("submissions").toArray();
    for (const QJsonValue& value : subs) {
        const QJsonObject obj = value.toObject();
        const QJsonObject taskRef = obj.value("taskRef").toObject();
        const int taskNo = taskRef.value("taskNo").toInt();
        const QString variation = taskRef.value("variation").toString();
        const int taskId = taskRef.value("taskId").toInt();
        if (taskNo <= 0 || variation.isEmpty() || taskId <= 0) continue;
        submissionsAttempted_[blockStorageKey(taskNo, variation)].insert(taskId, true);
    }
    return true;
}

bool TrainingStateService::loadPlan() const
{
    plannedByDate_.clear();

    QFile file(dataPath("plan.json"));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonArray days = doc.object().value("days").toArray();
    for (const QJsonValue& value : days) {
        const QJsonObject dayObj = value.toObject();
        const QDate date = QDate::fromString(dayObj.value("date").toString(), Qt::ISODate);
        if (!date.isValid()) continue;

        QVector<Block> blocks;
        const QJsonArray items = dayObj.value("items").toArray();
        for (const QJsonValue& itemValue : items) {
            const QJsonObject item = itemValue.toObject();
            Block block;
            block.taskNo = item.value("taskNo").toInt();
            block.variation = item.value("variation").toString();
            block.count = item.value("count").toInt();
            if (block.taskNo > 0 && !block.variation.isEmpty() && block.count > 0)
                blocks.push_back(block);
        }
        plannedByDate_.insert(date, blocks);
    }
    return true;
}

QVector<TrainingStateService::Block> TrainingStateService::blocksFromJsonString(const QString& raw) const
{
    QVector<Block> blocks;
    if (raw.trimmed().isEmpty()) return blocks;

    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) return blocks;

    for (const QJsonValue& value : doc.array()) {
        const QJsonObject obj = value.toObject();
        Block block;
        block.taskNo = obj.value("taskNo").toInt();
        block.variation = obj.value("variation").toString();
        block.count = obj.value("count").toInt();
        if (block.taskNo > 0 && !block.variation.isEmpty() && block.count > 0)
            blocks.push_back(block);
    }
    return blocks;
}

QString TrainingStateService::blocksToJsonString(const QVector<Block>& blocks) const
{
    QJsonArray array;
    for (const Block& block : blocks) {
        QJsonObject obj;
        obj.insert("taskNo", block.taskNo);
        obj.insert("variation", block.variation);
        obj.insert("count", block.count);
        array.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
