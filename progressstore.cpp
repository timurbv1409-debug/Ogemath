#include "progressstore.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>

ProgressStore& ProgressStore::instance() {
    static ProgressStore s;
    return s;
}

QString ProgressStore::filePath() const
{
    // ./data/progress.json рядом с exe
    const QString base = QCoreApplication::applicationDirPath();
    const QString dir = base + "/data";
    QDir().mkpath(dir);
    return dir + "/progress.json";
}

bool ProgressStore::load()
{
    QFile f(filePath());

    if (!f.exists()) {
        root_ = QJsonObject{
            {"schemaVersion", 1},
            {"tasks", QJsonObject{}}
        };
        return save();
    }

    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << "progress load: cannot open" << f.fileName();
        return false;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        qDebug() << "progress load: invalid json";
        return false;
    }

    root_ = doc.object();
    if (!root_.contains("schemaVersion")) root_["schemaVersion"] = 1;
    if (!root_.contains("tasks") || !root_.value("tasks").isObject()) root_["tasks"] = QJsonObject{};

    return true;
}

bool ProgressStore::save() const
{
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "progress save: cannot open" << f.fileName();
        return false;
    }
    f.write(QJsonDocument(root_).toJson(QJsonDocument::Indented));
    return true;
}

void ProgressStore::normalizeVariation(QJsonObject& varObj) const
{
    if (!varObj.contains("totalTasks")) varObj["totalTasks"] = 0;
    if (!varObj.contains("solved") || !varObj.value("solved").isArray())
        varObj["solved"] = QJsonArray{};
}

int ProgressStore::findPairIndex(const QJsonArray& solved, int problemNo)
{
    for (int i = 0; i < solved.size(); ++i) {
        const auto v = solved.at(i);
        if (!v.isArray()) continue;
        const auto pair = v.toArray();
        if (pair.size() < 2) continue;
        if (pair.at(0).toInt(-1) == problemNo)
            return i;
    }
    return -1;
}

void ProgressStore::ensureVariation(int taskNo, const QString& variantCode, int totalTasks)
{
    QJsonObject tasksObj = root_.value("tasks").toObject();
    const QString taskKey = QString::number(taskNo);

    QJsonObject taskObj = tasksObj.value(taskKey).toObject();
    QJsonObject varObj = taskObj.value(variantCode).toObject();

    if (varObj.isEmpty()) {
        varObj["totalTasks"] = totalTasks;
        varObj["solved"] = QJsonArray{};
    } else {
        normalizeVariation(varObj);
        varObj["totalTasks"] = totalTasks; // обновляем, если поменялось
    }

    taskObj[variantCode] = varObj;
    tasksObj[taskKey] = taskObj;
    root_["tasks"] = tasksObj;
}

void ProgressStore::markSolved(int taskNo, const QString& variantCode, int problemNo, bool correct)
{
    QJsonObject tasksObj = root_.value("tasks").toObject();
    const QString taskKey = QString::number(taskNo);

    QJsonObject taskObj = tasksObj.value(taskKey).toObject();
    QJsonObject varObj = taskObj.value(variantCode).toObject();

    if (varObj.isEmpty()) {
        varObj["totalTasks"] = 0;
        varObj["solved"] = QJsonArray{};
    } else {
        normalizeVariation(varObj);
    }

    QJsonArray solved = varObj.value("solved").toArray();

    const int idx = findPairIndex(solved, problemNo);
    QJsonArray pair;
    pair.append(problemNo);
    pair.append(correct);

    if (idx >= 0) {
        solved[idx] = pair; // обновили существующую
    } else {
        solved.append(pair); // добавили новую
    }

    varObj["solved"] = solved;

    taskObj[variantCode] = varObj;
    tasksObj[taskKey] = taskObj;
    root_["tasks"] = tasksObj;

    save();
}

int ProgressStore::totalTasks(int taskNo, const QString& variantCode) const
{
    const auto tasksObj = root_.value("tasks").toObject();
    const auto taskObj = tasksObj.value(QString::number(taskNo)).toObject();
    const auto varObj = taskObj.value(variantCode).toObject();
    return varObj.value("totalTasks").toInt(0);
}

int ProgressStore::solvedCount(int taskNo, const QString& variantCode) const
{
    const auto tasksObj = root_.value("tasks").toObject();
    const auto taskObj = tasksObj.value(QString::number(taskNo)).toObject();
    const auto varObj = taskObj.value(variantCode).toObject();
    const auto solved = varObj.value("solved").toArray();
    return solved.size(); // уникальные задачи (мы не допускаем дублей)
}

int ProgressStore::correctCount(int taskNo, const QString& variantCode) const
{
    const auto tasksObj = root_.value("tasks").toObject();
    const auto taskObj = tasksObj.value(QString::number(taskNo)).toObject();
    const auto varObj = taskObj.value(variantCode).toObject();
    const auto solved = varObj.value("solved").toArray();

    int c = 0;
    for (const auto& v : solved) {
        if (!v.isArray()) continue;
        const auto pair = v.toArray();
        if (pair.size() < 2) continue;
        if (pair.at(1).toBool(false))
            ++c;
    }
    return c;
}

std::optional<bool> ProgressStore::getSolvedResult(int taskNo, const QString& variantCode, int problemNo) const
{
    const auto tasksObj = root_.value("tasks").toObject();
    const auto taskObj = tasksObj.value(QString::number(taskNo)).toObject();
    const auto varObj = taskObj.value(variantCode).toObject();
    const auto solved = varObj.value("solved").toArray();

    const int idx = findPairIndex(solved, problemNo);
    if (idx < 0) return std::nullopt;

    const auto pair = solved.at(idx).toArray();
    if (pair.size() < 2) return std::nullopt;

    return pair.at(1).toBool(false);
}
