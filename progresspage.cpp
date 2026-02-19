#include "progresspage.h"

#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>

static QString dataPath(const QString& name)
{
    const QString base = QCoreApplication::applicationDirPath();
    QDir().mkpath(base + "/data");
    return base + "/data/" + name;
}

static QJsonObject loadJsonObject(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    return doc.object();
}

ProgressPage::ProgressPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyStyles();
    reload();
}

void ProgressPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    // header
    auto* header = new QHBoxLayout();
    header->setSpacing(10);

    backBtn_ = new QPushButton(QString::fromUtf8("← Назад"), this);
    backBtn_->setObjectName("backBtn");
    backBtn_->setCursor(Qt::PointingHandCursor);
    backBtn_->setMinimumHeight(36);
    connect(backBtn_, &QPushButton::clicked, this, &ProgressPage::backRequested);

    title_ = new QLabel(QString::fromUtf8("Мой прогресс"), this);
    title_->setObjectName("pageTitle");

    header->addWidget(backBtn_, 0);
    header->addStretch(1);
    header->addWidget(title_, 0);
    header->addStretch(2);

    root->addLayout(header);

    // tree table
    tree_ = new QTreeWidget(this);
    tree_->setObjectName("progressTree");
    tree_->setColumnCount(5);
    tree_->setHeaderLabels({
        QString::fromUtf8("№ / Тема"),
        QString::fromUtf8("Всего"),
        QString::fromUtf8("Решено"),
        QString::fromUtf8("Верно"),
        QString::fromUtf8("Успех %")
    });

    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 5; ++c)
        tree_->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    tree_->setRootIsDecorated(true);   // стрелочки
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->setIndentation(18);
    tree_->setColumnWidth(4, 190);

    root->addWidget(tree_, 1);
}

void ProgressPage::applyStyles()
{
    setStyleSheet(R"(
        QWidget { background: #f5f6f8; color: #111827; font-size: 13px; }

        QLabel#pageTitle { font-size: 18px; font-weight: 800; color: #111827; }

        QPushButton#backBtn {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 6px 12px;
            color: #111827;
            font-weight: 700;
        }
        QPushButton#backBtn:hover { background: #f3f4f6; }
        QPushButton#backBtn:pressed { background: #e5e7eb; }

        QTreeWidget#progressTree {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            padding: 6px;
            color: #111827;
        }
        QHeaderView::section {
            background: #f3f4f6;
            border: none;
            padding: 8px 10px;
            font-weight: 800;
            color: #111827;
        }
        QTreeWidget::item { padding: 6px; }
        QTreeWidget::item:selected { background: #2563eb; color: white; }
    )");
}

int ProgressPage::percent(int correct, int total)
{
    if (total <= 0) return 0;
    return int((100.0 * correct) / total + 0.5);
}

void ProgressPage::setPercentBar(QTreeWidgetItem* item, int column, int value)
{
    auto* bar = new QProgressBar(tree_);
    bar->setRange(0, 100);
    bar->setValue(qBound(0, value, 100));
    bar->setTextVisible(true);
    bar->setFormat(QString("%1%").arg(bar->value()));
    bar->setMinimumWidth(150);
    bar->setMaximumHeight(18);

    QString chunkColor;
    if (bar->value() < 40) chunkColor = "#ef4444";       // red
    else if (bar->value() < 70) chunkColor = "#f59e0b";  // amber
    else chunkColor = "#22c55e";                         // green

    bar->setStyleSheet(QString(R"(
        QProgressBar {
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            background: #f3f4f6;
            color: #111827;
            text-align: center;
            font-weight: 700;
        }
        QProgressBar::chunk {
            border-radius: 8px;
            background: %1;
        }
    )").arg(chunkColor));

    tree_->setItemWidget(item, column, bar);
}

void ProgressPage::loadCatalog()
{
    catalogVars_.clear();
    taskTitles_.clear();

    const QJsonObject root = loadJsonObject(dataPath("catalog.json"));
    const QJsonObject tasks = root.value("tasks").toObject();

    for (int taskNo = 1; taskNo <= 25; ++taskNo) {
        const QString key = QString::number(taskNo);
        const QJsonObject taskObj = tasks.value(key).toObject();

        const QString title = taskObj.value("title").toString();
        if (!title.isEmpty())
            taskTitles_[taskNo] = title;

        const QJsonObject varsObj = taskObj.value("variations").toObject();
        QStringList vars;
        for (auto it = varsObj.begin(); it != varsObj.end(); ++it)
            vars << it.key();

        vars.sort(Qt::CaseInsensitive);
        if (!vars.isEmpty())
            catalogVars_[taskNo] = vars;
    }
}

QStringList ProgressPage::varsForTask(int taskNo) const
{
    return catalogVars_.value(taskNo);
}

ProgressPage::Stats ProgressPage::variationStats(int taskNo, const QString& varName) const
{
    Stats s;

    const QJsonObject pr = loadJsonObject(dataPath("progress.json"));
    const QJsonObject tasks = pr.value("tasks").toObject();
    const QJsonObject taskObj = tasks.value(QString::number(taskNo)).toObject();
    const QJsonObject varObj = taskObj.value(varName).toObject();

    s.total = varObj.value("totalTasks").toInt(0);

    const QJsonArray solved = varObj.value("solved").toArray(); // [[n,bool],...]
    s.solved = solved.size();

    int c = 0;
    for (const auto& v : solved) {
        if (!v.isArray()) continue;
        const auto pair = v.toArray();
        if (pair.size() < 2) continue;
        if (pair.at(1).toBool(false)) ++c;
    }
    s.correct = c;

    return s;
}

ProgressPage::Stats ProgressPage::taskStats(int taskNo, const QStringList& vars) const
{
    Stats s;
    for (const QString& v : vars) {
        const Stats vs = variationStats(taskNo, v);
        s.total   += vs.total;
        s.solved  += vs.solved;
        s.correct += vs.correct;
    }
    return s;
}

void ProgressPage::reload()
{
    loadCatalog();
    tree_->clear();

    for (int taskNo = 1; taskNo <= 25; ++taskNo) {
        const QStringList vars = varsForTask(taskNo);
        const QString title = taskTitles_.value(taskNo, QString::fromUtf8("Задание %1").arg(taskNo));

        const Stats ts = taskStats(taskNo, vars);

        auto* top = new QTreeWidgetItem(tree_);
        top->setText(0, QString("%1. %2").arg(taskNo).arg(title));
        top->setText(1, QString::number(ts.total));
        top->setText(2, QString::number(ts.solved));
        top->setText(3, QString::number(ts.correct));
        setPercentBar(top, 4, percent(ts.correct, ts.total));

        for (const QString& v : vars) {
            const Stats vs = variationStats(taskNo, v);
            auto* child = new QTreeWidgetItem(top);
            child->setText(0, QString::fromUtf8("   %1").arg(v));
            child->setText(1, QString::number(vs.total));
            child->setText(2, QString::number(vs.solved));
            child->setText(3, QString::number(vs.correct));
            setPercentBar(child, 4, percent(vs.correct, vs.total));
        }

        top->setExpanded(false);
    }
}
