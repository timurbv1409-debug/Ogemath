#pragma once
#include <QWidget>
#include <QMap>
#include <QStringList>

class QPushButton;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

class ProgressPage final : public QWidget {
    Q_OBJECT
public:
    explicit ProgressPage(QWidget* parent = nullptr);

signals:
    void backRequested();

public slots:
    void reload(); // перечитать progress.json + catalog.json и обновить таблицу

private:
    void buildUi();
    void applyStyles();

    void loadCatalog(); // titles + variations
    QStringList varsForTask(int taskNo) const;

    struct Stats { int total=0, solved=0, correct=0; };
    Stats variationStats(int taskNo, const QString& varName) const;
    Stats taskStats(int taskNo, const QStringList& vars) const;

    static int percent(int correct, int total);

    // ProgressBar in cell
    void setPercentBar(QTreeWidgetItem* item, int column, int value);

private:
    QPushButton* backBtn_ = nullptr;
    QLabel* title_ = nullptr;
    QTreeWidget* tree_ = nullptr;

    QMap<int, QStringList> catalogVars_;
    QMap<int, QString> taskTitles_;
};
