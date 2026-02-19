#pragma once
#include <QString>
#include <QJsonObject>
#include <optional>

class ProgressStore final {
public:
    static ProgressStore& instance();

    bool load();        // грузит ./data/progress.json или создаёт новый
    bool save() const;

    QString filePath() const;

    // Инициализация вариации (если не существует)
    void ensureVariation(int taskNo, const QString& variantCode, int totalTasks);

    // Записать результат: номер задачи внутри вариации (например 12) + correct
    void markSolved(int taskNo, const QString& variantCode, int problemNo, bool correct);

    // Статистика
    int totalTasks(int taskNo, const QString& variantCode) const;
    int solvedCount(int taskNo, const QString& variantCode) const;    // сколько уникальных решено
    int correctCount(int taskNo, const QString& variantCode) const;   // сколько из них верно

    // Удобно для UI
    std::optional<bool> getSolvedResult(int taskNo, const QString& variantCode, int problemNo) const;

    QJsonObject root() const { return root_; }

private:
    ProgressStore() = default;

    // helpers
    static int clampPercent(int v);
    void normalizeVariation(QJsonObject& varObj) const;

    // работа с массивом solved = [[n,bool],...]
    static int findPairIndex(const QJsonArray& solved, int problemNo);

    QJsonObject root_;
};
