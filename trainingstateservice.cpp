#include "trainingstateservice.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTimeZone>
#include <algorithm>

namespace {
static QString todayKey()
{
    return QDate::currentDate().toString(Qt::ISODate);
}

static QString blockStorageKey(int taskNo, const QString& variation)
{
    return QString::number(taskNo) + QStringLiteral("|") + variation;
}

static QString twoDigits(int value)
{
    return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
}

static QString threeDigits(int value)
{
    return QStringLiteral("%1").arg(value, 3, 10, QLatin1Char('0'));
}

static QString fiveDigits(const QString& raw)
{
    QString digits;
    for (const QChar ch : raw) {
        if (ch.isDigit()) digits += ch;
    }
    if (digits.isEmpty()) digits = QStringLiteral("01010");
    if (digits.size() > 5) digits = digits.right(5);
    return digits.rightJustified(5, QLatin1Char('0'));
}
}

TrainingStateService::TrainingStateService(QObject* parent)
    : QObject(parent)
{
}

QString TrainingStateService::dataPath(const QString& fileName) const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();

    QStringList roots;
    roots << appDir
          << currentDir
          << QDir(appDir).absoluteFilePath(".")
          << QDir(appDir).absoluteFilePath("..");

    for (int up = 1; up <= 6; ++up) {
        QString rel;
        for (int i = 0; i < up; ++i) rel += "../";
        roots << QDir(appDir).absoluteFilePath(rel)
              << QDir(currentDir).absoluteFilePath(rel);
    }

    QStringList candidates;
    for (const QString& root : std::as_const(roots)) {
        const QString cleanRoot = QDir::cleanPath(root);
        candidates << QDir(cleanRoot).filePath("data/" + fileName)
                   << QDir(cleanRoot).filePath(fileName)
                   << QDir(cleanRoot).filePath("debug/data/" + fileName)
                   << QDir(cleanRoot).filePath("release/data/" + fileName);
    }

    candidates.removeDuplicates();
    for (const QString& path : std::as_const(candidates)) {
        const QString clean = QDir::cleanPath(path);
        if (QFile::exists(clean)) return clean;
    }

    return QDir(appDir).filePath("data/" + fileName);
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
        info.orderIndex = it.value().orderIndex;
        out.push_back(info);
    }
    std::sort(out.begin(), out.end(), [](const VariationInfo& a, const VariationInfo& b) {
        if (a.orderIndex != b.orderIndex) return a.orderIndex < b.orderIndex;
        return a.name < b.name;
    });
    return out;
}

int TrainingStateService::totalTasksInVariation(int taskNo, const QString& variation) const
{
    ensureLoaded();
    return variationData(taskNo, variation).total;
}

int TrainingStateService::solvedTasksInVariation(int taskNo, const QString& variation) const
{
    ensureLoaded();

    const CatalogVariation var = variationData(taskNo, variation);
    if (var.items.isEmpty()) return 0;

    const QString key = blockStorageKey(taskNo, variation);
    int solved = 0;

    if (taskNo >= 1 && taskNo <= 19) {
        const QMap<int, bool> doneMap = progressDone_.value(key);
        for (const CatalogItem& item : var.items) {
            if (doneMap.value(item.id, false)) {
                ++solved;
            }
        }
    } else {
        const QMap<int, bool> attemptedMap = submissionsAttempted_.value(key);
        for (const CatalogItem& item : var.items) {
            if (attemptedMap.value(item.id, false)) {
                ++solved;
            }
        }
    }

    return solved;
}
int TrainingStateService::remainingTasksInVariation(int taskNo, const QString& variation) const
{
    const int total = totalTasksInVariation(taskNo, variation);
    const int solved = qMin(total, solvedTasksInVariation(taskNo, variation));
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

TrainingStateService::AccountInfo TrainingStateService::accountInfo() const
{
    ensureLoaded();
    return accountInfo_;
}

QVector<TrainingStateService::SessionTask> TrainingStateService::buildSessionTasks(const QVector<Block>& blocks) const
{
    ensureLoaded();

    QVector<SessionTask> result;
    for (const Block& block : blocks) {
        const CatalogVariation variation = variationData(block.taskNo, block.variation);
        if (variation.items.isEmpty() || block.count <= 0) continue;

        QVector<CatalogItem> preferred;
        QVector<CatalogItem> fallback;
        for (const CatalogItem& item : variation.items) {
            if (isSolvedOrAttempted(block.taskNo, block.variation, item.id)) {
                fallback.push_back(item);
            } else {
                preferred.push_back(item);
            }
        }

        QVector<CatalogItem> picked;
        for (const CatalogItem& item : preferred) {
            if (picked.size() >= block.count) break;
            picked.push_back(item);
        }
        for (const CatalogItem& item : fallback) {
            if (picked.size() >= block.count) break;
            picked.push_back(item);
        }

        for (const CatalogItem& item : picked) {
            SessionTask task;
            task.taskNo = block.taskNo;
            task.taskTitle = taskTitle(block.taskNo);
            task.variation = block.variation;
            task.variationIndex = variation.orderIndex;
            task.itemId = item.id;
            task.imageUrl = item.url;
            task.answer = item.answer;
            task.isWritten = (block.taskNo >= 20 && block.taskNo <= 25);
            task.solved = isSolvedOrAttempted(block.taskNo, block.variation, item.id);
            task.hasAttempt = task.solved;
            task.code = makeTaskCode(block.taskNo, block.variation, item.id);
            result.push_back(task);
        }
    }
    return result;
}

QString TrainingStateService::makeTaskCode(int taskNo, const QString& variation, int itemId) const
{
    ensureLoaded();
    const CatalogVariation data = variationData(taskNo, variation);
    const QString account = fiveDigits(accountInfo_.accountNumber);
    const int variationIndex = data.orderIndex > 0 ? data.orderIndex : 1;
    return twoDigits(taskNo) + twoDigits(variationIndex) + threeDigits(itemId) + account;
}

QString TrainingStateService::loadSavedTestAnswer(const QString& taskCode) const
{
    QSettings settings("OgeMath", "OgeMath");
    return settings.value(QStringLiteral("training/testAnswers/") + taskCode).toString();
}

void TrainingStateService::saveTestAnswer(const QString& taskCode, const QString& answer)
{
    QSettings settings("OgeMath", "OgeMath");
    settings.setValue(QStringLiteral("training/testAnswers/") + taskCode, answer);
}

bool TrainingStateService::markWrittenTaskSolved(const SessionTask& task)
{
    if (!task.isWritten || task.taskNo <= 0 || task.variation.isEmpty() || task.itemId <= 0) {
        return false;
    }

    QFile file(dataPath(QStringLiteral("submissions.json")));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    QJsonArray submissions = root.value(QStringLiteral("submissions")).toArray();

    bool exists = false;
    for (int i = 0; i < submissions.size(); ++i) {
        QJsonObject obj = submissions.at(i).toObject();
        const QJsonObject ref = obj.value(QStringLiteral("taskRef")).toObject();
        if (ref.value(QStringLiteral("taskNo")).toInt() == task.taskNo
            && ref.value(QStringLiteral("variation")).toString() == task.variation
            && ref.value(QStringLiteral("taskId")).toInt() == task.itemId) {
            QJsonObject review = obj.value(QStringLiteral("review")).toObject();
            review.insert(QStringLiteral("score"), 100);
            obj.insert(QStringLiteral("review"), review);
            obj.insert(QStringLiteral("status"), QStringLiteral("REVIEWED"));
            obj.insert(QStringLiteral("reviewedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            submissions.replace(i, obj);
            exists = true;
            break;
        }
    }

    if (!exists) {
        int maxId = 0;
        for (const QJsonValue& value : submissions) {
            maxId = qMax(maxId, value.toObject().value(QStringLiteral("id")).toInt());
        }

        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        QJsonObject item;
        item.insert(QStringLiteral("status"), QStringLiteral("REVIEWED"));
        QJsonObject taskRef;
        taskRef.insert(QStringLiteral("taskNo"), task.taskNo);
        taskRef.insert(QStringLiteral("variation"), task.variation);
        taskRef.insert(QStringLiteral("taskId"), task.itemId);
        item.insert(QStringLiteral("taskRef"), taskRef);

        QJsonObject review;
        review.insert(QStringLiteral("score"), 100);
        item.insert(QStringLiteral("review"), review);
        item.insert(QStringLiteral("createdAt"), now);
        item.insert(QStringLiteral("reviewedAt"), now);
        item.insert(QStringLiteral("id"), maxId + 1);
        submissions.push_back(item);
    }

    root.insert(QStringLiteral("submissions"), submissions);
    if (!writeJsonFile(file.fileName(), QJsonDocument(root))) return false;

    loaded_ = false;
    ensureLoaded();
    return true;
}

bool TrainingStateService::ensureLoaded() const
{
    if (loaded_) return true;
    loaded_ = loadCatalog() && loadProgress() && loadSubmissions() && loadPlan() && loadAccount();
    return loaded_;
}

bool TrainingStateService::loadCatalog() const
{
    catalog_.clear();

    QFile file(dataPath(QStringLiteral("catalog.json")));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonObject tasks = doc.object().value(QStringLiteral("tasks")).toObject();
    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        const int taskNo = it.key().toInt();
        const QJsonObject taskObj = it.value().toObject();

        CatalogTask task;
        task.title = taskObj.value(QStringLiteral("title")).toString();

        const QJsonObject vars = taskObj.value(QStringLiteral("variations")).toObject();
        int order = 1;
        for (auto vit = vars.begin(); vit != vars.end(); ++vit, ++order) {
            CatalogVariation var;
            var.name = vit.key();
            var.orderIndex = order;
            const QJsonObject vObj = vit.value().toObject();
            const QJsonArray items = vObj.value(QStringLiteral("items")).toArray();
            var.total = items.size();
            for (const QJsonValue& itemValue : items) {
                const QJsonObject itemObj = itemValue.toObject();
                CatalogItem item;
                item.id = itemObj.value(QStringLiteral("id")).toInt();
                item.url = itemObj.value(QStringLiteral("url")).toString();
                item.answer = itemObj.value(QStringLiteral("answer")).toString();
                if (item.id > 0) var.items.push_back(item);
            }
            task.variations.insert(var.name, var);
        }

        catalog_.insert(taskNo, task);
    }
    return true;
}

bool TrainingStateService::loadProgress() const
{
    progressDone_.clear();

    QFile file(dataPath(QStringLiteral("progress.json")));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonObject tasks = doc.object().value(QStringLiteral("tasks")).toObject();
    for (auto taskIt = tasks.begin(); taskIt != tasks.end(); ++taskIt) {
        const int taskNo = taskIt.key().toInt();
        const QJsonObject taskObj = taskIt.value().toObject();
        const QJsonObject vars = taskObj.value(QStringLiteral("variations")).toObject();

        for (auto varIt = vars.begin(); varIt != vars.end(); ++varIt) {
            const QString variation = varIt.key();
            const QJsonObject varObj = varIt.value().toObject();
            const QJsonArray done = varObj.value(QStringLiteral("done")).toArray();

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

    QFile file(dataPath(QStringLiteral("submissions.json")));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonArray subs = doc.object().value(QStringLiteral("submissions")).toArray();
    for (const QJsonValue& value : subs) {
        const QJsonObject obj = value.toObject();
        const QJsonObject taskRef = obj.value(QStringLiteral("taskRef")).toObject();
        const int taskNo = taskRef.value(QStringLiteral("taskNo")).toInt();
        const QString variation = taskRef.value(QStringLiteral("variation")).toString();
        const int taskId = taskRef.value(QStringLiteral("taskId")).toInt();
        if (taskNo <= 0 || variation.isEmpty() || taskId <= 0) continue;
        submissionsAttempted_[blockStorageKey(taskNo, variation)].insert(taskId, true);
    }
    return true;
}

bool TrainingStateService::loadPlan() const
{
    plannedByDate_.clear();

    QFile file(dataPath(QStringLiteral("plan.json")));
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    const QJsonArray days = doc.object().value(QStringLiteral("days")).toArray();
    for (const QJsonValue& value : days) {
        const QJsonObject dayObj = value.toObject();
        const QDate date = QDate::fromString(dayObj.value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (!date.isValid()) continue;

        QVector<Block> blocks;
        const QJsonArray items = dayObj.value(QStringLiteral("items")).toArray();
        for (const QJsonValue& itemValue : items) {
            const QJsonObject item = itemValue.toObject();
            Block block;
            block.taskNo = item.value(QStringLiteral("taskNo")).toInt();
            block.variation = item.value(QStringLiteral("variation")).toString();
            block.count = item.value(QStringLiteral("count")).toInt();
            if (block.taskNo > 0 && !block.variation.isEmpty() && block.count > 0)
                blocks.push_back(block);
        }
        plannedByDate_.insert(date, blocks);
    }
    return true;
}

bool TrainingStateService::loadAccount() const
{
    accountInfo_ = {};

    QFile file(dataPath(QStringLiteral("account.json")));
    if (!file.open(QIODevice::ReadOnly)) return true;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return true;

    const QJsonObject obj = doc.object();
    accountInfo_.accountNumber = obj.value(QStringLiteral("accountNumber")).toString();
    accountInfo_.login = obj.value(QStringLiteral("login")).toString();
    accountInfo_.password = obj.value(QStringLiteral("password")).toString();
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
        block.taskNo = obj.value(QStringLiteral("taskNo")).toInt();
        block.variation = obj.value(QStringLiteral("variation")).toString();
        block.count = obj.value(QStringLiteral("count")).toInt();
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
        obj.insert(QStringLiteral("taskNo"), block.taskNo);
        obj.insert(QStringLiteral("variation"), block.variation);
        obj.insert(QStringLiteral("count"), block.count);
        array.push_back(obj);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

TrainingStateService::CatalogVariation TrainingStateService::variationData(int taskNo, const QString& variation) const
{
    ensureLoaded();
    return catalog_.value(taskNo).variations.value(variation);
}

bool TrainingStateService::isSolvedOrAttempted(int taskNo, const QString& variation, int itemId) const
{
    const QString key = blockStorageKey(taskNo, variation);
    if (taskNo >= 1 && taskNo <= 19) {
        return progressDone_.value(key).value(itemId, false);
    }
    return submissionsAttempted_.value(key).value(itemId, false);
}

bool TrainingStateService::writeJsonFile(const QString& filePath, const QJsonDocument& doc) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}
