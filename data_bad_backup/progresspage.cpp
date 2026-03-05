#include "progresspage.h"
#include "calendarpage.h"

#include <QTabWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProgressBar>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QScrollArea>
#include <QToolTip>
#include <QColor>
#include <QCursor>
#include <QPushButton>
#include <QtMath>
#include <algorithm>
#include <cmath>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

static QString keyVar(int taskNo, const QString& var) { return QString::number(taskNo) + "|" + var; }
static int clampInt(int v, int lo, int hi) { if (v<lo) return lo; if (v>hi) return hi; return v; }

static QChartView* makeChartView(QWidget* parent)
{
    QChartView* v = new QChartView(new QChart(), parent);
    v->setRenderHint(QPainter::Antialiasing, true);
    v->setMinimumHeight(340);
    v->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    v->setObjectName("chartCard");
    return v;
}


static void tuneCategoryAxis(QBarCategoryAxis* ax, int angleDeg)
{
    if (!ax) return;
    QFont f = ax->labelsFont();
    if (f.pointSize() <= 0) f.setPointSize(8);
    else f.setPointSize(qMax(7, f.pointSize() - 2));
    ax->setLabelsFont(f);
    ax->setLabelsAngle(angleDeg);
    ax->setTruncateLabels(false);
}

static QFrame* makeCard(QWidget* parent, const QString& title, const QString& subtitle)
{
    QFrame* card = new QFrame(parent);
    card->setObjectName("card");
    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12,12,12,12);
    lay->setSpacing(6);

    QLabel* t = new QLabel(title, card);
    t->setObjectName("cardTitle");
    t->setWordWrap(true);
    lay->addWidget(t);

    if (!subtitle.isEmpty()) {
        QLabel* s = new QLabel(subtitle, card);
        s->setObjectName("cardText");
        s->setWordWrap(true);
        lay->addWidget(s);
    }
    return card;
}


static QStringList buildDailyLabels(const QDate& from, const QDate& to)
{
    QStringList out;
    if (!from.isValid() || !to.isValid() || from > to) return out;
    QDate d = from;
    while (d <= to) {
        out << d.toString("dd.MM");
        d = d.addDays(1);
    }
    return out;
}

static int dayIndex(const QDate& from, const QDate& d)
{
    if (!from.isValid() || !d.isValid()) return 0;
    return from.daysTo(d);
}

static QStringList downsampleLabels(const QStringList& labels, int maxLabels)
{
    if (maxLabels <= 0) maxLabels = 12;
    if (labels.size() <= maxLabels) return labels;

    int step = (int)std::ceil((double)labels.size() / (double)maxLabels);
    if (step < 1) step = 1;

    QStringList out;
    out.reserve(labels.size());
    for (int i = 0; i < labels.size(); ++i) {
        if (i % step == 0 || i == labels.size() - 1) out << labels[i];
        else out << "";
    }
    return out;
}

ProgressPage::ProgressPage(QWidget* parent) : QWidget(parent)
{
    buildUi();
    applyProductStyles();
    setupWatcher();
    reloadAllData();
}

void ProgressPage::buildUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(14,14,14,14);
    root->setSpacing(10);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("statsTabs");

    overviewTab_ = buildOverviewTab();
    tabs_->addTab(overviewTab_, QString::fromUtf8("Общее"));

    chartsTab_ = buildChartsTab();
    tabs_->addTab(chartsTab_, QString::fromUtf8("Графики"));
    calendarTab_ = new CalendarPage(this);
    tabs_->addTab(calendarTab_, QString::fromUtf8("Календарь"));
tabs_->addTab(buildPlaceholderTab(QString::fromUtf8("Детально"),
                                      QString::fromUtf8("Здесь будет подробная статистика по попыткам и источникам.")),
                  QString::fromUtf8("Детально"));

    root->addWidget(tabs_, 1);
}

QWidget* ProgressPage::buildPlaceholderTab(const QString& title, const QString& hint)
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* lay = new QVBoxLayout(w);
    lay->setContentsMargins(18,18,18,18);
    lay->setSpacing(10);

    QLabel* t = new QLabel(title, w);
    t->setObjectName("placeholderTitle");
    QLabel* h = new QLabel(hint, w);
    h->setObjectName("placeholderHint");
    h->setWordWrap(true);

    lay->addWidget(t);
    lay->addWidget(h);
    lay->addStretch(1);
    return w;
}

QWidget* ProgressPage::buildChartsTab()
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* root = new QVBoxLayout(w);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    QHBoxLayout* top = new QHBoxLayout();
    top->setSpacing(10);

    QLabel* title = new QLabel(QString::fromUtf8("Графики и аналитика"), w);
    title->setObjectName("chartsTitle");

    chartsPeriod_ = new QComboBox(w);
    chartsPeriod_->addItem(QString::fromUtf8("7 дней"), 7);
    chartsPeriod_->addItem(QString::fromUtf8("30 дней"), 30);
    chartsPeriod_->addItem(QString::fromUtf8("90 дней"), 90);
    chartsPeriod_->addItem(QString::fromUtf8("Всё"), 0);
    chartsPeriod_->setCurrentIndex(1);

    chartsSource_ = new QComboBox(w);
    chartsSource_->addItem(QString::fromUtf8("Тренировки"), "training");
    chartsSource_->addItem(QString::fromUtf8("Пробники"), "mock");
    chartsSource_->addItem(QString::fromUtf8("Всё"), "all");
    chartsSource_->setCurrentIndex(2);

    chartsType_ = new QComboBox(w);
    chartsType_->addItem(QString::fromUtf8("Тестовые"), 1);
    chartsType_->addItem(QString::fromUtf8("Письменные"), 2);
    chartsType_->addItem(QString::fromUtf8("Всё"), 0);
    chartsType_->setCurrentIndex(2);

    chartsForecast_ = new QCheckBox(QString::fromUtf8("Прогноз 14 дней"), w);
    chartsForecast_->setChecked(true);

    chartsCompare_ = new QCheckBox(QString::fromUtf8("Сравнить"), w);
    chartsCompare_->setChecked(false);

    chartsCompareTask_ = new QComboBox(w);
    chartsCompareTask_->setMinimumWidth(90);
    chartsCompareTask_->setEnabled(false);
    for (int no = 1; no <= 25; ++no) chartsCompareTask_->addItem(QString::number(no), no);

    top->addWidget(title);
    top->addStretch(1);
    top->addWidget(new QLabel(QString::fromUtf8("Период:"), w));
    top->addWidget(chartsPeriod_);
    top->addWidget(new QLabel(QString::fromUtf8("Источник:"), w));
    top->addWidget(chartsSource_);
    top->addWidget(new QLabel(QString::fromUtf8("Тип:"), w));
    top->addWidget(chartsType_);
    top->addWidget(chartsForecast_);
    top->addWidget(chartsCompare_);
    top->addWidget(new QLabel(QString::fromUtf8("с №"), w));
    top->addWidget(chartsCompareTask_);

    root->addLayout(top);

    QHBoxLayout* split = new QHBoxLayout();
    split->setSpacing(12);

    chartsNav_ = new QTreeWidget(w);
    chartsNav_->setObjectName("chartsNav");
    chartsNav_->setHeaderHidden(true);
    chartsNav_->setMinimumWidth(260);

    chartsScroll_ = new QScrollArea(w);
    chartsScroll_->setObjectName("chartsScroll");
    chartsScroll_->setWidgetResizable(true);

    chartsDash_ = new QWidget(chartsScroll_);
    chartsGrid_ = new QGridLayout(chartsDash_);
    chartsGrid_->setContentsMargins(0,0,0,0);
    chartsGrid_->setHorizontalSpacing(12);
    chartsGrid_->setVerticalSpacing(12);

    // Cards
    {
        QFrame* c = makeCard(chartsDash_, QString::fromUtf8("Прогноз и динамика"),
                             QString::fromUtf8("Оценка по мастерству + пробники. Скользящее среднее сглаживает шум."));
        cvScoreTrend_ = makeChartView(c);
        cvScoreTrend_->setMinimumHeight(520);
        chartsInfo_ = new QLabel(QString::fromUtf8("—"), c);
        chartsInfo_->setObjectName("cardText");
        chartsInfo_->setWordWrap(true);
        QVBoxLayout* v = (QVBoxLayout*)c->layout();
        v->addWidget(cvScoreTrend_);
        v->addWidget(chartsInfo_);
        chartsGrid_->addWidget(c, 0, 0, 1, 2);
    }
    {
        QFrame* c1 = makeCard(chartsDash_, QString::fromUtf8("Пробники по датам"),
                              QString::fromUtf8("Наведи на точку: дата и балл."));
        cvMocks_ = makeChartView(c1);
        cvMocks_->setMinimumHeight(380);
        ((QVBoxLayout*)c1->layout())->addWidget(cvMocks_);
        chartsGrid_->addWidget(c1, 1, 0, 1, 1);

        QFrame* c2 = makeCard(chartsDash_, QString::fromUtf8("Стабильность"),
                              QString::fromUtf8("Среднее и σ по последним N пробникам."));
        cvStability_ = makeChartView(c2);
        ((QVBoxLayout*)c2->layout())->addWidget(cvStability_);
        chartsGrid_->addWidget(c2, 1, 1, 1, 1);
    }
    {
        QFrame* c1 = makeCard(chartsDash_, QString::fromUtf8("Активность"),
                              QString::fromUtf8("Задачи/день. Наведи: дата, тип, решено/верно."));
        cvActivity_ = makeChartView(c1);
        ((QVBoxLayout*)c1->layout())->addWidget(cvActivity_);
        chartsGrid_->addWidget(c1, 2, 0, 1, 1);

        QFrame* c2 = makeCard(chartsDash_, QString::fromUtf8("Вклад номеров в балл"),
                              QString::fromUtf8("Сколько баллов даёт каждый номер при текущем мастерстве."));
        cvContrib_ = makeChartView(c2);
        ((QVBoxLayout*)c2->layout())->addWidget(cvContrib_);
        chartsGrid_->addWidget(c2, 2, 1, 1, 1);
    }

    // row2b: accuracy + risk trend
    {
        QFrame* c1 = makeCard(chartsDash_, QString::fromUtf8("Точность по дням"),
                              QString::fromUtf8("Доля верных ответов (верно/решено) в процентах + сглаживание."));
        cvAccuracyTrend_ = makeChartView(c1);
        ((QVBoxLayout*)c1->layout())->addWidget(cvAccuracyTrend_);
        chartsGrid_->addWidget(c1, 9, 0, 1, 1);

        QFrame* c2 = makeCard(chartsDash_, QString::fromUtf8("Риск во времени"),
                              QString::fromUtf8("Средний риск по номерам: растёт, если долго не повторять."));
        cvRiskTrend_ = makeChartView(c2);
        ((QVBoxLayout*)c2->layout())->addWidget(cvRiskTrend_);
        chartsGrid_->addWidget(c2, 9, 1, 1, 1);
    }
    // row3b: weekly load
    {
        QFrame* c = makeCard(chartsDash_, QString::fromUtf8("Нагрузка по неделям"),
                             QString::fromUtf8("Сколько задач и сколько разных номеров ты трогал каждую неделю."));
        cvWeeklyLoad_ = makeChartView(c);
        ((QVBoxLayout*)c->layout())->addWidget(cvWeeklyLoad_);
        chartsGrid_->addWidget(c, 4, 0, 1, 2);
    }
    {
        QFrame* c1 = makeCard(chartsDash_, QString::fromUtf8("Где теряются баллы"),
                              QString::fromUtf8("Потенциал роста, если довести номер до 70%."));
        cvBottlenecks_ = makeChartView(c1);
        ((QVBoxLayout*)c1->layout())->addWidget(cvBottlenecks_);
        chartsGrid_->addWidget(c1, 3, 0, 1, 1);

        QFrame* c2 = makeCard(chartsDash_, QString::fromUtf8("Мастерство по вариациям (№)"),
                              QString::fromUtf8("Выбери № слева, чтобы увидеть детали."));
        cvTaskVarMastery_ = makeChartView(c2);
        ((QVBoxLayout*)c2->layout())->addWidget(cvTaskVarMastery_);
        chartsGrid_->addWidget(c2, 3, 1, 1, 1);
    }
    {
        QFrame* c1 = makeCard(chartsDash_, QString::fromUtf8("Активность выбранного номера"),
                              QString::fromUtf8("Попытки/день (из sessions.taskSummary)."));
        cvTaskAttempts_ = makeChartView(c1);
        ((QVBoxLayout*)c1->layout())->addWidget(cvTaskAttempts_);
        chartsGrid_->addWidget(c1, 7, 0, 1, 1);

        QFrame* c2 = makeCard(chartsDash_, QString::fromUtf8("Эффект повторения"),
                              QString::fromUtf8("Без повторения мастерство постепенно падает."));
        cvForgetting_ = makeChartView(c2);
        ((QVBoxLayout*)c2->layout())->addWidget(cvForgetting_);
        chartsGrid_->addWidget(c2, 7, 1, 1, 1);
    }
    {
        QFrame* c = makeCard(chartsDash_, QString::fromUtf8("Теплокарта по вариациям"),
                             QString::fromUtf8("Таблица «номер × вариация». Наведи на ячейку — подсказка."));
        heatmap_ = new QTableWidget(c);
        heatmap_->setObjectName("heatmap");
        heatmap_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        heatmap_->setSelectionMode(QAbstractItemView::NoSelection);
        heatmap_->setFocusPolicy(Qt::NoFocus);
        heatmap_->verticalHeader()->setVisible(true);
        heatmap_->horizontalHeader()->setVisible(true);
        heatmap_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        heatmap_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        heatmap_->horizontalHeader()->setDefaultSectionSize(110);
        heatmap_->verticalHeader()->setDefaultSectionSize(22);
        ((QVBoxLayout*)c->layout())->addWidget(heatmap_, 1);
        chartsGrid_->addWidget(c, 8, 0, 1, 2);
    }

    chartsScroll_->setWidget(chartsDash_);

    split->addWidget(chartsNav_, 0);
    split->addWidget(chartsScroll_, 1);
    root->addLayout(split, 1);

    connect(chartsNav_, &QTreeWidget::itemSelectionChanged, this, &ProgressPage::onChartsNavSelectionChanged);
    connect(chartsPeriod_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProgressPage::onChartsControlsChanged);
    connect(chartsSource_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProgressPage::onChartsControlsChanged);
    connect(chartsType_,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProgressPage::onChartsControlsChanged);
    connect(chartsForecast_, &QCheckBox::toggled, this, &ProgressPage::onChartsControlsChanged);
    connect(chartsCompare_, &QCheckBox::toggled, this, &ProgressPage::onChartsControlsChanged);
    connect(chartsCompareTask_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProgressPage::onChartsControlsChanged);

    return w;
}

void ProgressPage::onChartsNavSelectionChanged()
{
    if (chartsNavRebuilding_) return;

    selectedTaskNo_ = 0;
    QList<QTreeWidgetItem*> items = chartsNav_->selectedItems();
    if (!items.isEmpty()) selectedTaskNo_ = items.first()->data(0, Qt::UserRole).toInt();

    // Быстрый перерендер графиков без пересчёта данных
    if (chartsTab_) {
        renderCharts(cachedTasks_, cachedOverview_);
    }
}

void ProgressPage::onChartsControlsChanged()
{
    if (chartsCompareTask_) {
        compareTaskNo_ = chartsCompareTask_->currentData().toInt();
        chartsCompareTask_->setEnabled(chartsCompare_ && chartsCompare_->isChecked() && selectedTaskNo_ > 0);
    }
    reloadAllData();
}


// ---- path/json ----

QString ProgressPage::dataPath(const QString& fileName) const
{
    // Важно: запуск из Qt Creator идёт из build/.../debug,
    // а данные часто лежат в папке проекта ./data.
    // Поэтому ищем по нескольким "корням" и по цепочке родителей.

    const QStringList roots = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath()
    };

    QStringList candidates;
    for (const QString& r0 : roots) {
        QString r = r0;
        for (int up = 0; up <= 6; ++up) { // до 6 уровней вверх — хватает для build/.../debug
            candidates << r + "/data/" + fileName;
            candidates << r + "/" + fileName;
            // следующий уровень
            r = QDir(r).absoluteFilePath("..");
        }
    }

    for (const QString& p : candidates) {
        const QString clean = QDir::cleanPath(p);
        if (QFile::exists(clean)) return clean;
    }

    // fallback: пусть хотя бы возвращает ожидаемое место
    return QDir::cleanPath(QDir::currentPath() + "/data/" + fileName);
}

bool ProgressPage::readJsonFile(const QString& path, QJsonDocument& outDoc, QString* err) const
{
    QFile f(path);
    if (!f.exists()) { if (err) *err = "File not found: " + path; return false; }
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = "Cannot open: " + path; return false; }
    QByteArray bytes = f.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (doc.isNull()) { if (err) *err = "Invalid JSON: " + path; return false; }
    outDoc = doc;
    return true;
}

// ---- loaders ----

bool ProgressPage::loadCatalog(QString* err)
{
    catalog_.clear();
    QJsonDocument doc;
    QString path = dataPath("catalog.json");
    if (!readJsonFile(path, doc, err)) return false;

    QJsonObject root = doc.object();
    QJsonObject tasksObj = root.value("tasks").toObject();

    for (QJsonObject::const_iterator it = tasksObj.begin(); it != tasksObj.end(); ++it) {
        int taskNo = it.key().toInt();
        QJsonObject tObj = it.value().toObject();

        CatalogTaskInfo info;
        info.title = tObj.value("title").toString();

        QJsonObject varsObj = tObj.value("variations").toObject();
        for (QJsonObject::const_iterator vit = varsObj.begin(); vit != varsObj.end(); ++vit) {
            QString varKey = vit.key();
            QJsonObject vObj = vit.value().toObject();

            CatalogVarInfo vinfo;
            vinfo.displayName = vObj.value("displayName").toString(varKey);
            vinfo.itemsTotal = vObj.value("items").toArray().size();
            info.vars.insert(varKey, vinfo);
        }
        catalog_.insert(taskNo, info);
    }
    return true;
}

bool ProgressPage::loadProgress(QString* err)
{
    progressDone_.clear();
    QJsonDocument doc;
    QString path = dataPath("progress.json");
    if (!QFile::exists(path)) return true;
    if (!readJsonFile(path, doc, err)) return false;

    QJsonObject root = doc.object();
    QJsonObject tasksObj = root.value("tasks").toObject();

    for (QJsonObject::const_iterator it = tasksObj.begin(); it != tasksObj.end(); ++it) {
        int taskNo = it.key().toInt();
        QJsonObject tObj = it.value().toObject();
        QJsonObject varsObj = tObj.value("variations").toObject();

        for (QJsonObject::const_iterator vit = varsObj.begin(); vit != varsObj.end(); ++vit) {
            QString var = vit.key();
            QJsonObject vObj = vit.value().toObject();

            QMap<int,bool> mp;
            QJsonArray doneArr = vObj.value("done").toArray();
            for (const QJsonValue& dv : doneArr) {
                QJsonArray pair = dv.toArray();
                if (pair.size() < 2) continue;
                mp[pair.at(0).toInt()] = pair.at(1).toBool();
            }
            progressDone_.insert(keyVar(taskNo, var), mp);
        }
    }
    return true;
}

bool ProgressPage::loadSessions(QString* err)
{
    sessionDays_.clear();
    QJsonDocument doc;
    QString path = dataPath("sessions.json");
    if (!QFile::exists(path)) return true;
    if (!readJsonFile(path, doc, err)) return false;

    QJsonObject root = doc.object();
    int schema = root.value("schemaVersion").toInt(1);
    QJsonArray days = root.value("days").toArray();

    for (const QJsonValue& dv : days) {
        QJsonObject o = dv.toObject();
        QDate d = QDate::fromString(o.value("date").toString(), Qt::ISODate);
        if (!d.isValid()) continue;

        DaySession ds;
        ds.date = d;
        ds.doneCount = o.value("doneCount").toInt();
        ds.correctCount = o.value("correctCount").toInt(ds.doneCount);
        ds.durationMin = o.value("durationMin").toInt(0);
        ds.type = o.value("type").toString(schema >= 2 ? "training" : "training");
        ds.mockScore = o.value("mockScore").toInt(-1);
        ds.mockMax = o.value("mockMax").toInt(32);

        QJsonArray summ = o.value("taskSummary").toArray();
        for (const QJsonValue& tv : summ) {
            QJsonObject to = tv.toObject();
            int task = to.value("task").toInt();
            int att  = to.value("attempts").toInt();
            int cor  = to.value("correct").toInt();
            if (task >= 1 && task <= 25) ds.byTask[task] = QPair<int,int>(att, cor);
        }
        sessionDays_.push_back(ds);
    }

    std::sort(sessionDays_.begin(), sessionDays_.end(), [](const DaySession& a, const DaySession& b){
        return a.date < b.date;
    });
    return true;
}

bool ProgressPage::loadSubmissions(QString* err)
{
    submissionEvents_.clear();
    QJsonDocument doc;
    QString path = dataPath("submissions.json");
    if (!QFile::exists(path)) return true;
    if (!readJsonFile(path, doc, err)) return false;

    QJsonObject root = doc.object();
    QJsonArray subs = root.value("submissions").toArray();

    for (const QJsonValue& sv : subs) {
        QJsonObject s = sv.toObject();
        if (s.value("status").toString() != "REVIEWED") continue;

        QJsonObject tr = s.value("taskRef").toObject();
        int taskNo = tr.value("taskNo").toInt();
        QString var = tr.value("variation").toString();
        int taskId = tr.value("taskId").toInt();

        QJsonObject review = s.value("review").toObject();
        int score = review.value("score").toInt(-1);

        AttemptEvent ev;
        ev.ref.taskNo = taskNo;
        ev.ref.variation = var;
        ev.ref.taskId = taskId;
        ev.source = "bot";
        ev.score = score;

        if (score >= 80) { ev.correct = true; ev.partial = false; }
        else if (score >= 30) { ev.correct = false; ev.partial = true; }
        else { ev.correct = false; ev.partial = false; }

        ev.ts = QDateTime::fromString(s.value("reviewedAt").toString(), Qt::ISODate);
        if (!ev.ts.isValid()) ev.ts = QDateTime::fromString(s.value("createdAt").toString(), Qt::ISODate);

        if (taskNo >= 1 && taskNo <= 25 && !var.isEmpty() && taskId > 0)
            submissionEvents_.push_back(ev);
    }
    return true;
}

// ---- aggregates helpers ----

QVector<ProgressPage::AttemptEvent> ProgressPage::buildAttemptEvents(int typeFilter) const
{
    QVector<AttemptEvent> events;

    if (typeFilter == 0 || typeFilter == 1) {
        for (QMap<QString, QMap<int,bool>>::const_iterator it = progressDone_.begin(); it != progressDone_.end(); ++it) {
            QStringList parts = it.key().split('|');
            if (parts.size() != 2) continue;
            int taskNo = parts[0].toInt();
            QString var = parts[1];

            const QMap<int,bool>& mp = it.value();
            for (QMap<int,bool>::const_iterator mit = mp.begin(); mit != mp.end(); ++mit) {
                AttemptEvent ev;
                ev.ref.taskNo = taskNo;
                ev.ref.variation = var;
                ev.ref.taskId = mit.key();
                ev.correct = mit.value();
                ev.source = "app";
                events.push_back(ev);
            }
        }
    }

    if (typeFilter == 0 || typeFilter == 2) {
        for (const AttemptEvent& e : submissionEvents_) events.push_back(e);
    }

    QSet<QString> seen;
    QVector<AttemptEvent> uniq;
    uniq.reserve(events.size());
    for (const AttemptEvent& e : events) {
        QString k = QString("%1|%2|%3|%4").arg(e.ref.taskNo).arg(e.ref.variation).arg(e.ref.taskId).arg(e.source);
        if (seen.contains(k)) continue;
        seen.insert(k);
        uniq.push_back(e);
    }
    return uniq;
}

void ProgressPage::computeDerivedMetricsForVar(VarAgg& v) const
{
    v.accuracy = (v.attemptedUnique > 0) ? (double)v.correctUnique / (double)v.attemptedUnique : 0.0;
    int target = (v.itemsTotal > 0) ? qMin(20, v.itemsTotal) : 20;
    v.targetAttempts = target;
    v.confidence = qBound(0.0, (double)v.attemptedUnique / (double)target, 1.0);
    v.mastery = (int)qRound(100.0 * v.accuracy * v.confidence);

    int risk = 100 - v.mastery;
    if (v.lastTs.isValid()) {
        int daysIdle = v.lastTs.date().daysTo(QDate::currentDate());
        risk += qMin(15, qMax(0, daysIdle));
    }
    v.risk = clampInt(risk, 0, 100);
    v.wrongUnique = qMax(0, v.attemptedUnique - v.correctUnique - v.partialUnique);
}

void ProgressPage::computeDerivedMetricsForTask(TaskAgg& t) const
{
    int attempted = 0, correct = 0, partial = 0;
    QDateTime last;
    double wsum = 0.0, msum = 0.0;

    for (QMap<QString, VarAgg>::const_iterator it = t.vars.begin(); it != t.vars.end(); ++it) {
        const VarAgg& v = it.value();
        attempted += v.attemptedUnique;
        correct += v.correctUnique;
        partial += v.partialUnique;
        if (v.lastTs.isValid() && (!last.isValid() || v.lastTs > last)) last = v.lastTs;

        double w = qMax(1, v.targetAttempts);
        wsum += w;
        msum += (double)v.mastery * w;
    }

    t.attemptedUnique = attempted;
    t.correctUnique = correct;
    t.partialUnique = partial;
    t.lastTs = last;
    t.mastery = (wsum > 0.0) ? (int)qRound(msum/wsum) : 0;

    // fallbackLastFromSessions: если нет timestamp в progress/submissions, берём последнюю дату,
    // когда номер встречался в sessions.taskSummary
    if (!t.lastTs.isValid()) {
        QDate best;
        for (const DaySession& ds : sessionDays_) {
            if (ds.byTask.contains(t.taskNo) && ds.byTask.value(t.taskNo).first > 0) {
                if (!best.isValid() || ds.date > best) best = ds.date;
            }
        }
        if (best.isValid()) t.lastTs = QDateTime(best, QTime(19,0));
    }

    int risk = 100 - t.mastery;
    if (t.lastTs.isValid()) {
        int daysIdle = t.lastTs.date().daysTo(QDate::currentDate());
        risk += qMin(15, qMax(0, daysIdle));
    }
    t.risk = clampInt(risk, 0, 100);
}

QMap<int, ProgressPage::TaskAgg> ProgressPage::computeAggregates(const QVector<AttemptEvent>& events) const
{
    QMap<int, TaskAgg> tasks;

    for (QMap<int, CatalogTaskInfo>::const_iterator it = catalog_.begin(); it != catalog_.end(); ++it) {
        TaskAgg t;
        t.taskNo = it.key();
        t.title = it.value().title;
        t.maxPoints = (t.taskNo <= 19) ? 1 : 2;

        for (QMap<QString, CatalogVarInfo>::const_iterator vit = it.value().vars.begin(); vit != it.value().vars.end(); ++vit) {
            VarAgg v;
            v.itemsTotal = vit.value().itemsTotal;
            t.vars.insert(vit.key(), v);
        }
        tasks.insert(t.taskNo, t);
    }

    QMap<QString, QSet<int>> attemptedSet, correctSet, partialSet;
    QMap<QString, QDateTime> lastTs;

    for (const AttemptEvent& e : events) {
        QString kv = keyVar(e.ref.taskNo, e.ref.variation);
        attemptedSet[kv].insert(e.ref.taskId);
        if (e.correct) correctSet[kv].insert(e.ref.taskId);
        else if (e.partial) partialSet[kv].insert(e.ref.taskId);
        if (e.ts.isValid()) {
            if (!lastTs.contains(kv) || e.ts > lastTs[kv]) lastTs[kv] = e.ts;
        }
    }

    for (QMap<int, TaskAgg>::iterator it = tasks.begin(); it != tasks.end(); ++it) {
        TaskAgg& t = it.value();
        for (QMap<QString, VarAgg>::iterator vit = t.vars.begin(); vit != t.vars.end(); ++vit) {
            QString var = vit.key();
            VarAgg& v = vit.value();
            QString kv = keyVar(t.taskNo, var);
            v.attemptedUnique = attemptedSet[kv].size();
            v.correctUnique = correctSet[kv].size();
            v.partialUnique = partialSet[kv].size();
            v.lastTs = lastTs.value(kv);
            computeDerivedMetricsForVar(v);
        }
        computeDerivedMetricsForTask(t);
        // fallbackVarLastToTask: если у вариации нет даты, наследуем последнюю дату номера
        for (QMap<QString, VarAgg>::iterator vit2 = t.vars.begin(); vit2 != t.vars.end(); ++vit2) {
            if (!vit2.value().lastTs.isValid() && t.lastTs.isValid()) vit2.value().lastTs = t.lastTs;
        }
    }
    return tasks;
}

ProgressPage::OverviewStats ProgressPage::computeOverview(const QMap<int, TaskAgg>& tasks) const
{
    OverviewStats ov;
    double exp = 0.0;
    for (QMap<int, TaskAgg>::const_iterator it = tasks.begin(); it != tasks.end(); ++it) {
        const TaskAgg& t = it.value();
        exp += (double)t.maxPoints * ((double)t.mastery / 100.0);
    }
    ov.expectedPoints = exp;

    QSet<QDate> active;
    QDate last;
    for (const DaySession& ds : sessionDays_) {
        if (ds.doneCount > 0) {
            active.insert(ds.date);
            if (!last.isValid() || ds.date > last) last = ds.date;
        }
    }
    if (last.isValid()) ov.lastSessionTs = QDateTime(last, QTime(19,0));

    int streak = 0;
    QDate day = QDate::currentDate();
    while (active.contains(day)) { streak++; day = day.addDays(-1); }
    ov.streakDays = streak;

    struct Item { int no; int risk; int att; };
    QVector<Item> items;
    for (QMap<int, TaskAgg>::const_iterator it = tasks.begin(); it != tasks.end(); ++it) {
        Item x; x.no = it.value().taskNo; x.risk = it.value().risk; x.att = it.value().attemptedUnique;
        items.push_back(x);
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){
        if (a.risk != b.risk) return a.risk > b.risk;
        return a.no < b.no;
    });

    for (const Item& x : items) {
        if (x.att < 3) ov.lowDataTaskNos.push_back(x.no);
        else if (ov.weakTaskNos.size() < 5) ov.weakTaskNos.push_back(x.no);
    }
    return ov;
}

// ---- minimal overview visuals (same as before) ----

QFrame* ProgressPage::makeStatCard(const QString& title, const QString& tooltip)
{
    QFrame* card = new QFrame(overviewTab_);
    card->setObjectName("statCard");
    card->setToolTip(tooltip);

    QVBoxLayout* v = new QVBoxLayout(card);
    v->setContentsMargins(14,12,14,12);
    v->setSpacing(6);

    QLabel* t = new QLabel(title, card);
    t->setObjectName("statCardTitle");
    t->setWordWrap(true);

    QLabel* val = new QLabel("-", card);
    val->setObjectName("statCardValue");

    QLabel* sub = new QLabel("", card);
    sub->setObjectName("statCardSub");
    sub->setWordWrap(true);
    sub->setVisible(false);

    v->addWidget(t);
    v->addWidget(val);
    v->addWidget(sub);

    card->setProperty("valueLabel", QVariant::fromValue<void*>(val));
    card->setProperty("subLabel", QVariant::fromValue<void*>(sub));
    return card;
}

void ProgressPage::setCardValue(QFrame* card, const QString& value, const QString& sub)
{
    QLabel* val = reinterpret_cast<QLabel*>(card->property("valueLabel").value<void*>());
    QLabel* s   = reinterpret_cast<QLabel*>(card->property("subLabel").value<void*>());
    if (val) val->setText(value);
    if (s) { if (sub.isEmpty()) s->setVisible(false); else { s->setVisible(true); s->setText(sub); } }
}

QWidget* ProgressPage::buildOverviewTab()
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* root = new QVBoxLayout(w);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(12);

    QHBoxLayout* top = new QHBoxLayout();
    backBtn_ = new QPushButton(QString::fromUtf8("← Назад"), w);
    backBtn_->setObjectName("backBtn");
    backBtn_->setFixedHeight(34);
    connect(backBtn_, &QPushButton::clicked, this, &ProgressPage::backRequested);

    QLabel* header = new QLabel(QString::fromUtf8("Моя статистика — Общее"), w);
    header->setObjectName("overviewTitle");
    top->addWidget(backBtn_);
    top->addWidget(header, 1);

    QWidget* cards = new QWidget(w);
    QGridLayout* grid = new QGridLayout(cards);
    grid->setContentsMargins(0,0,0,0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    cardExpected_ = makeStatCard(QString::fromUtf8("Примерные баллы"), QString::fromUtf8("Оценка по мастерству."));
    cardStreak_   = makeStatCard(QString::fromUtf8("Серия"), QString::fromUtf8("Дней подряд."));
    cardLast_     = makeStatCard(QString::fromUtf8("Последний день"), QString::fromUtf8("По sessions.json."));
    cardFocus_    = makeStatCard(QString::fromUtf8("Слабые места"), QString::fromUtf8("Топ‑5 по риску."));

    grid->addWidget(cardExpected_,0,0);
    grid->addWidget(cardStreak_,0,1);
    grid->addWidget(cardLast_,0,2);
    grid->addWidget(cardFocus_,0,3);

    weakLabel_ = new QLabel("-", w);
    weakLabel_->setObjectName("weakLabel");
    weakLabel_->setWordWrap(true);
    lowDataLabel_ = new QLabel("-", w);
    lowDataLabel_->setObjectName("lowDataLabel");
    lowDataLabel_->setWordWrap(true);

    tree_ = new QTreeWidget(w);
    tree_->setObjectName("taskTree");
    tree_->setColumnCount(7);
    tree_->setHeaderLabels(QStringList()
        << QString::fromUtf8("№ / Тема")
        << QString::fromUtf8("Всего")
        << QString::fromUtf8("Решено")
        << QString::fromUtf8("Верно")
        << QString::fromUtf8("Мастерство")
        << QString::fromUtf8("Риск")
        << QString::fromUtf8("Последнее"));
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i=1;i<7;++i) tree_->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    root->addLayout(top);
    root->addWidget(cards);
    root->addWidget(weakLabel_);
    root->addWidget(lowDataLabel_);
    root->addWidget(tree_, 1);

    return w;
}

QWidget* ProgressPage::makeMasteryBar(int mastery) const
{
    QProgressBar* bar = new QProgressBar();
    bar->setRange(0,100);
    bar->setValue(clampInt(mastery,0,100));
    bar->setTextVisible(true);
    bar->setFormat(QString::number(mastery) + "%");
    bar->setFixedHeight(16);
    bar->setMinimumWidth(150);
    return bar;
}

QString ProgressPage::riskLabelText(int risk) const
{
    if (risk >= 75) return QString::fromUtf8("Высокий");
    if (risk >= 45) return QString::fromUtf8("Средний");
    return QString::fromUtf8("Низкий");
}

QString ProgressPage::lastDateText(const QDateTime& dt) const
{
    if (!dt.isValid()) return QString::fromUtf8("—");
    return dt.toString("dd.MM.yyyy");
}

void ProgressPage::renderOverview(const OverviewStats& ov, const QMap<int, TaskAgg>& tasks)
{
    int maxTotal = 19*1 + 6*2;
    setCardValue(cardExpected_, QString::fromUtf8("%1 / %2").arg(QString::number(ov.expectedPoints,'f',1)).arg(maxTotal));
    setCardValue(cardStreak_, QString::fromUtf8("%1 дн.").arg(ov.streakDays));
    setCardValue(cardLast_, ov.lastSessionTs.isValid()? ov.lastSessionTs.date().toString("dd.MM.yyyy") : "—");

    QStringList weak;
    for (int n : ov.weakTaskNos) weak << QString("№%1").arg(n);
    setCardValue(cardFocus_, weak.isEmpty()? "—" : weak.join(", "));

    QStringList weak2;
    for (int n : ov.weakTaskNos) {
        TaskAgg t = tasks.value(n);
        weak2 << QString("№%1 (риск %2)").arg(n).arg(t.risk);
    }
    weakLabel_->setText(QString::fromUtf8("Слабые места: %1").arg(weak2.isEmpty()?QString::fromUtf8("—"):weak2.join(", ")));

    if (!ov.lowDataTaskNos.isEmpty()) {
        QStringList ls;
        for (int n : ov.lowDataTaskNos) ls << QString::number(n);
        lowDataLabel_->setText(QString::fromUtf8("Мало данных (<3): %1").arg(ls.join(", ")));
    } else lowDataLabel_->setText(QString::fromUtf8("Мало данных: нет"));
}

void ProgressPage::addVariationRow(QTreeWidgetItem* parent, int taskNo, const QString& varKey, const VarAgg& v)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    QString display = varKey;
    if (catalog_.contains(taskNo) && catalog_[taskNo].vars.contains(varKey))
        display = catalog_[taskNo].vars[varKey].displayName;

    item->setText(0, "   " + display);
    item->setText(1, QString::number(v.itemsTotal));
    item->setText(2, QString::number(v.attemptedUnique));
    item->setText(3, QString::number(v.correctUnique));
    tree_->setItemWidget(item, 4, makeMasteryBar(v.mastery));
    item->setText(5, QString("%1 (%2)").arg(riskLabelText(v.risk)).arg(v.risk));
    item->setText(6, lastDateText(v.lastTs));
}

void ProgressPage::addTaskRow(const TaskAgg& t)
{
    QTreeWidgetItem* top = new QTreeWidgetItem(tree_);
    top->setText(0, QString("%1. %2").arg(t.taskNo).arg(t.title));

    int total = 0;
    for (QMap<QString, VarAgg>::const_iterator it = t.vars.begin(); it != t.vars.end(); ++it) total += it.value().itemsTotal;

    top->setText(1, QString::number(total));
    top->setText(2, QString::number(t.attemptedUnique));
    top->setText(3, QString::number(t.correctUnique));
    tree_->setItemWidget(top, 4, makeMasteryBar(t.mastery));
    top->setText(5, QString("%1 (%2)").arg(riskLabelText(t.risk)).arg(t.risk));
    top->setText(6, lastDateText(t.lastTs));

    for (QMap<QString, VarAgg>::const_iterator it = t.vars.begin(); it != t.vars.end(); ++it)
        addVariationRow(top, t.taskNo, it.key(), it.value());
}

void ProgressPage::renderTaskTree(const QMap<int, TaskAgg>& tasks)
{
    tree_->clear();
    for (QMap<int, TaskAgg>::const_iterator it = tasks.begin(); it != tasks.end(); ++it) addTaskRow(it.value());
    tree_->expandToDepth(0);
}

// ---- charts math ----

QVector<QPointF> ProgressPage::rollingMean(const QVector<QPointF>& pts, int window) const
{
    if (window <= 1 || pts.isEmpty()) return pts;
    QVector<QPointF> out;
    out.reserve(pts.size());
    for (int i=0;i<pts.size();++i) {
        int l = qMax(0, i-window+1);
        double s=0.0; int cnt=0;
        for (int j=l;j<=i;++j) { s += pts[j].y(); cnt++; }
        out.push_back(QPointF(pts[i].x(), cnt? s/cnt : pts[i].y()));
    }
    return out;
}

void ProgressPage::linearRegression(const QVector<QPointF>& pts, double& a, double& b) const
{
    if (pts.size() < 2) { a = pts.isEmpty()?0.0:pts.last().y(); b = 0.0; return; }
    double sx=0, sy=0, sxx=0, sxy=0;
    int n = pts.size();
    for (const QPointF& p : pts) {
        sx += p.x(); sy += p.y();
        sxx += p.x()*p.x();
        sxy += p.x()*p.y();
    }
    double denom = n*sxx - sx*sx;
    if (qFuzzyIsNull(denom)) { a = sy/n; b = 0.0; return; }
    b = (n*sxy - sx*sy)/denom;
    a = (sy - b*sx)/n;
}

double ProgressPage::stddev(const QVector<double>& v) const
{
    if (v.size() < 2) return 0.0;
    double m=0; for (double x : v) m += x; m /= v.size();
    double s=0; for (double x : v) s += (x-m)*(x-m);
    s /= v.size();
    return std::sqrt(s);
}

void ProgressPage::rebuildChartsNav(const QMap<int, TaskAgg>& tasks, const OverviewStats& ov)
{
    chartsNavRebuilding_ = true;
    chartsNav_->blockSignals(true);

    Q_UNUSED(ov);
    chartsNav_->clear();

    QTreeWidgetItem* overall = new QTreeWidgetItem(chartsNav_);
    overall->setText(0, QString::fromUtf8("Общее"));
    overall->setData(0, Qt::UserRole, 0);

    QTreeWidgetItem* nums = new QTreeWidgetItem(chartsNav_);
    nums->setText(0, QString::fromUtf8("Номера 1–25"));
    nums->setData(0, Qt::UserRole, -1);

    for (int no=1; no<=25; ++no) {
        QTreeWidgetItem* it = new QTreeWidgetItem(nums);
        QString t = tasks.contains(no) ? tasks.value(no).title : QString::fromUtf8("—");
        it->setText(0, QString("№%1 — %2").arg(no).arg(t));
        it->setData(0, Qt::UserRole, no);
    }
    chartsNav_->expandAll();

    QTreeWidgetItem* sel = overall;
    if (selectedTaskNo_ > 0 && nums->childCount() >= selectedTaskNo_) sel = nums->child(selectedTaskNo_ - 1);
    chartsNav_->setCurrentItem(sel);

    if (chartsCompareTask_) {
        chartsCompareTask_->setEnabled(chartsCompare_ && chartsCompare_->isChecked() && selectedTaskNo_ > 0);
    }

    chartsNav_->blockSignals(false);
    chartsNavRebuilding_ = false;
}

void ProgressPage::renderCharts(const QMap<int, TaskAgg>& tasks, const OverviewStats& ov)
{
    int periodDays = chartsPeriod_ ? chartsPeriod_->currentData().toInt() : 30;
    QString source = chartsSource_ ? chartsSource_->currentData().toString() : "all";
    bool showForecast = chartsForecast_ ? chartsForecast_->isChecked() : true;

    QDate minDate;
    if (periodDays > 0) minDate = QDate::currentDate().addDays(-periodDays + 1);

    QVector<DaySession> days;
    for (const DaySession& ds : sessionDays_) {
        if (periodDays > 0 && ds.date < minDate) continue;
        if (source != "all") {
            if (source == "training" && ds.type != "training") continue;
            if (source == "mock" && ds.type != "mock") continue;
        }
        days.push_back(ds);
    }

    QStringList weak;
    for (int n : ov.weakTaskNos) {
        TaskAgg t = tasks.value(n);
        weak << QString("№%1 (риск %2)").arg(n).arg(t.risk);
    }
    chartsInfo_->setText(QString::fromUtf8("Легенда: Мастерство — оценка по текущим данным; Пробник — реальные баллы; Скользящее — среднее по пробникам; Прогноз — тренд на 14 дней.\nСлабые места: %1")
                          .arg(weak.isEmpty()?QString::fromUtf8("—"):weak.join(", ")));

    // Score trend + forecast
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Примерный балл и пробники"));
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignBottom);

        // Continuous date range
        QDate from, to;
        for (const DaySession& ds : days) {
            if (!from.isValid() || ds.date < from) from = ds.date;
            if (!to.isValid() || ds.date > to) to = ds.date;
        }
        if (!from.isValid() || !to.isValid()) {
            from = QDate::currentDate().addDays(-29);
            to = QDate::currentDate();
        }

        QStringList labels = buildDailyLabels(from, to);
        int N = labels.size();

        // extend axis for forecast horizon so dashed line is visible on category axis
        int horizonDays = (showForecast ? 14 : 0);
        if (horizonDays > 0) {
            QDate d2 = to;
            for (int k = 1; k <= horizonDays; ++k) {
                d2 = d2.addDays(1);
                labels << d2.toString("dd.MM");
            }
        }

        // Expected points baseline = current mastery estimate (ov.expectedPoints)
        QLineSeries* sExpected = new QLineSeries(chart);
        sExpected->setName(QString::fromUtf8("Оценка по мастерству"));

        // Add a light "decay without practice" effect from sessions: if no activity in last X days, show a small drop
        // This is purely for visualization and explains spaced repetition.
        QSet<QDate> activeDays;
        for (const DaySession& ds : days) if (ds.doneCount > 0) activeDays.insert(ds.date);

        for (int i=0;i<N;++i) {
            QDate d = from.addDays(i);
            int idle = 0;
            // compute idle streak ending at day d
            QDate t0 = d;
            while (t0.isValid() && !activeDays.contains(t0) && t0 >= from) { idle++; t0 = t0.addDays(-1); if (idle>30) break; }
            double factor = std::exp(-(double)idle / 35.0); // gentle decay
            double y = ov.expectedPoints * factor;
            sExpected->append(i, y);
        }
        // extend expected line flat into forecast horizon
        if (horizonDays > 0 && N > 0) {
            double lastY = sExpected->pointsVector().isEmpty() ? ov.expectedPoints : sExpected->pointsVector().last().y();
            for (int k=1; k<=horizonDays; ++k) sExpected->append(N-1+k, lastY);
        }

        // Mock points as scatter (always visible)
        QScatterSeries* sMock = new QScatterSeries(chart);
        sMock->setName(QString::fromUtf8("Пробник"));
        sMock->setMarkerSize(8.0);

        QVector<QPointF> mockPts;
        for (const DaySession& ds : days) {
            if (ds.type == "mock" && ds.mockScore >= 0) {
                int x = dayIndex(from, ds.date);
                sMock->append(x, ds.mockScore);
                mockPts.push_back(QPointF(x, ds.mockScore));
            }
        }

        // Rolling mean over mocks (SMA) as line
        QLineSeries* sSma = new QLineSeries(chart);
        sSma->setName(QString::fromUtf8("Скользящее среднее"));
        QVector<QPointF> sma = rollingMean(mockPts, 3);
        for (const QPointF& p : sma) sSma->append(p);

        chart->addSeries(sExpected);
        chart->addSeries(sMock);
        chart->addSeries(sSma);

        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(labels, 18));
        tuneCategoryAxis(axX, -90);

        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 32);
        axY->setTitleText(QString::fromUtf8("Баллы"));

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        sExpected->attachAxis(axX); sExpected->attachAxis(axY);
        sMock->attachAxis(axX); sMock->attachAxis(axY);
        sSma->attachAxis(axX); sSma->attachAxis(axY);

        // Forecast from last mocks if enough points
        if (showForecast && mockPts.size() >= 3) {
            int K = qMin(8, (int)mockPts.size());
            QVector<QPointF> lastK;
            for (int i=mockPts.size()-K;i<mockPts.size();++i) lastK.push_back(mockPts[i]);
            double a=0,b=0; linearRegression(lastK, a, b);

            QLineSeries* sF = new QLineSeries(chart);
            sF->setName(QString::fromUtf8("Прогноз 14 дней"));
            QPen pen = sF->pen(); pen.setStyle(Qt::DashLine); sF->setPen(pen);

            int lastX = N-1;
            sF->append(lastX, qBound(0.0, a + b*lastX, 32.0));
            for (int k=1;k<=14;++k) sF->append(lastX+k, qBound(0.0, a + b*(lastX+k), 32.0));

            chart->addSeries(sF);
            sF->attachAxis(axX);
            sF->attachAxis(axY);

            QObject::connect(sF, &QLineSeries::hovered, this, [from, N](const QPointF& p, bool state){
                if (!state) return;
                int idx = (int)qRound(p.x());
                QDate d = from.addDays(idx);
                QToolTip::showText(QCursor::pos(),
                                   QString::fromUtf8("Прогноз %1: %2")
                                       .arg(d.toString("dd.MM.yyyy"))
                                       .arg(QString::number(p.y(),'f',1)));
            });
        }

        QObject::connect(sMock, &QScatterSeries::hovered, this, [from](const QPointF& p, bool state){
            if (!state) return;
            QDate d = from.addDays((int)qRound(p.x()));
            QToolTip::showText(QCursor::pos(),
                               QString::fromUtf8("Пробник %1: %2")
                                   .arg(d.toString("dd.MM.yyyy"))
                                   .arg((int)qRound(p.y())));
        });

        QObject::connect(sExpected, &QLineSeries::hovered, this, [from](const QPointF& p, bool state){
            if (!state) return;
            QDate d = from.addDays((int)qRound(p.x()));
            QToolTip::showText(QCursor::pos(),
                               QString::fromUtf8("Мастерство %1: %2")
                                   .arg(d.toString("dd.MM.yyyy"))
                                   .arg(QString::number(p.y(),'f',1)));
        });

        QObject::connect(sSma, &QLineSeries::hovered, this, [from](const QPointF& p, bool state){
            if (!state) return;
            QDate d = from.addDays((int)qRound(p.x()));
            QToolTip::showText(QCursor::pos(),
                               QString::fromUtf8("Ср. %1: %2")
                                   .arg(d.toString("dd.MM.yyyy"))
                                   .arg(QString::number(p.y(),'f',1)));
        });

        cvScoreTrend_->setChart(chart);
    }
// Mocks chart
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Пробники по датам"));
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->hide();

        QLineSeries* s = new QLineSeries(chart);
        QStringList cats;
        int i=0;
        for (const DaySession& ds : days) {
            if (ds.type != "mock" || ds.mockScore < 0) continue;
            s->append(i, ds.mockScore);
            cats << ds.date.toString("dd.MM.yyyy");
            i++;
        }

        chart->addSeries(s);
        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        QStringList shortCats;
        for (const QString& c : cats) shortCats << c.left(5);
        axX->append(downsampleLabels(shortCats, 12));
        tuneCategoryAxis(axX, -90);

        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 32);

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        s->attachAxis(axX); s->attachAxis(axY);

        QObject::connect(s, &QLineSeries::hovered, this, [cats](const QPointF& p, bool state){
            if (!state) return;
            int idx = (int)qRound(p.x());
            if (idx>=0 && idx<cats.size())
                QToolTip::showText(QCursor::pos(), QString("%1: %2").arg(cats[idx]).arg((int)qRound(p.y())));
        });

        cvMocks_->setChart(chart);
    }

    // Stability
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Стабильность: среднее и σ"));
        chart->legend()->setVisible(true);

        QVector<double> mocks;
        QVector<QString> dlabels;
        for (const DaySession& ds : days) {
            if (ds.type=="mock" && ds.mockScore>=0) { mocks.push_back(ds.mockScore); dlabels.push_back(ds.date.toString("dd.MM")); }
        }

        QLineSeries* sMean = new QLineSeries(chart);
        sMean->setName(QString::fromUtf8("Среднее (окно 5)"));
        QLineSeries* sSig = new QLineSeries(chart);
        sSig->setName(QString::fromUtf8("σ (окно 5)"));

        int W=5;
        QStringList cats;
        for (int i=0;i<mocks.size();++i) {
            int l=qMax(0,i-W+1);
            QVector<double> win;
            for (int j=l;j<=i;++j) win.push_back(mocks[j]);
            double mean=0; for(double v:win) mean+=v; mean/=win.size();
            double sig=stddev(win);
            sMean->append(i, mean);
            sSig->append(i, sig);
            cats << dlabels[i];
        }

        chart->addSeries(sMean);
        chart->addSeries(sSig);
        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(cats, 25));
        tuneCategoryAxis(axX, -90);
        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 32);
        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        sMean->attachAxis(axX); sMean->attachAxis(axY);
        sSig->attachAxis(axX); sSig->attachAxis(axY);

        cvStability_->setChart(chart);
    }

    // Activity
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Активность по дням"));
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignBottom);

        QLineSeries* sDone = new QLineSeries(chart);
        sDone->setName(QString::fromUtf8("Решено"));
        QLineSeries* sCorrect = new QLineSeries(chart);
        sCorrect->setName(QString::fromUtf8("Верно"));

        // Build continuous date axis (daily)
        QDate from, to;
        for (const DaySession& ds : days) {
            if (!from.isValid() || ds.date < from) from = ds.date;
            if (!to.isValid() || ds.date > to) to = ds.date;
        }
        if (!from.isValid() || !to.isValid()) {
            from = QDate::currentDate().addDays(-6);
            to = QDate::currentDate();
        }

        QStringList labels = buildDailyLabels(from, to);
        // Map date -> (done, correct)
        QMap<QDate, QPair<int,int>> mp;
        for (const DaySession& ds : days) mp[ds.date] = QPair<int,int>(ds.doneCount, ds.correctCount);

        for (QDate d = from; d <= to; d = d.addDays(1)) {
            int x = dayIndex(from, d);
            QPair<int,int> v = mp.value(d, QPair<int,int>(0,0));
            sDone->append(x, v.first);
            sCorrect->append(x, v.second);
        }

        chart->addSeries(sDone);
        chart->addSeries(sCorrect);

        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(labels, 18));
        tuneCategoryAxis(axX, -90);

        QValueAxis* axY = new QValueAxis(chart);
        axY->setMin(0);
        axY->setTitleText(QString::fromUtf8("Задач"));

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        sDone->attachAxis(axX); sDone->attachAxis(axY);
        sCorrect->attachAxis(axX); sCorrect->attachAxis(axY);

        QObject::connect(sDone, &QLineSeries::hovered, this, [from](const QPointF& p, bool state){
            if (!state) return;
            int idx = (int)qRound(p.x());
            QDate d = from.addDays(idx);
            QToolTip::showText(QCursor::pos(),
                               QString::fromUtf8("%1: решено %2").arg(d.toString("dd.MM.yyyy")).arg((int)qRound(p.y())));
        });
        QObject::connect(sCorrect, &QLineSeries::hovered, this, [from](const QPointF& p, bool state){
            if (!state) return;
            int idx = (int)qRound(p.x());
            QDate d = from.addDays(idx);
            QToolTip::showText(QCursor::pos(),
                               QString::fromUtf8("%1: верно %2").arg(d.toString("dd.MM.yyyy")).arg((int)qRound(p.y())));
        });

        cvActivity_->setChart(chart);
    }
// Accuracy per day (correct/done %)
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Точность по дням (верно/решено)"));

        QLineSeries* sAcc = new QLineSeries(chart);
        sAcc->setName(QString::fromUtf8("Точность, %"));

        QVector<QPointF> pts;
        QStringList cats;
        int x = 0;
        for (const DaySession& ds : days) {
            double acc = 0.0;
            if (ds.doneCount > 0) acc = 100.0 * (double)ds.correctCount / (double)ds.doneCount;
            pts.push_back(QPointF(x, acc));
            sAcc->append(x, acc);
            cats << ds.date.toString("dd.MM");
            x++;
        }

        QLineSeries* sSmooth = new QLineSeries(chart);
        sSmooth->setName(QString::fromUtf8("Сглаживание (окно 5)"));
        QVector<QPointF> sm = rollingMean(pts, 5);
        for (const QPointF& p : sm) sSmooth->append(p);

        chart->addSeries(sAcc);
        chart->addSeries(sSmooth);

        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(cats, 25));
        tuneCategoryAxis(axX, -90);

        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 100);
        axY->setTitleText(QString::fromUtf8("%"));

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        sAcc->attachAxis(axX); sAcc->attachAxis(axY);
        sSmooth->attachAxis(axX); sSmooth->attachAxis(axY);

        cvAccuracyTrend_->setChart(chart);
    }

    // Risk trend over time (average risk using "days since last touch" per task + current mastery baseline)
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Средний риск по номерам во времени"));

        QLineSeries* sRisk = new QLineSeries(chart);
        sRisk->setName(QString::fromUtf8("Средний риск (0–100)"));

        // last day when each task was practiced (from sessions.taskSummary)
        QMap<int, QDate> lastTouch;
        QMap<int, int> baseRisk;
        for (int no = 1; no <= 25; ++no) {
            int r = tasks.contains(no) ? tasks.value(no).risk : 100;
            baseRisk[no] = r;
        }

        QStringList cats;
        int x = 0;
        for (const DaySession& ds : days) {
            // update lastTouch for tasks present that day
            for (QMap<int, QPair<int,int>>::const_iterator it = ds.byTask.begin(); it != ds.byTask.end(); ++it) {
                if (it.value().first > 0) lastTouch[it.key()] = ds.date;
            }

            double sum = 0.0;
            int cnt = 0;
            for (int no = 1; no <= 25; ++no) {
                int base = baseRisk.value(no, 100);
                int idle = 30;
                if (lastTouch.contains(no)) idle = lastTouch[no].daysTo(ds.date);
                int r = clampInt(base + qMin(15, qMax(0, idle)), 0, 100);
                sum += r;
                cnt++;
            }
            double avg = (cnt > 0) ? (sum / (double)cnt) : 0.0;
            sRisk->append(x, avg);
            cats << ds.date.toString("dd.MM");
            x++;
        }

        chart->addSeries(sRisk);

        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(cats, 25));
        tuneCategoryAxis(axX, -90);

        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 100);
        axY->setTitleText(QString::fromUtf8("Риск"));

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        sRisk->attachAxis(axX);
        sRisk->attachAxis(axY);

        cvRiskTrend_->setChart(chart);
    }

    // Weekly load: tasks count and distinct task numbers per week
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Нагрузка по неделям"));

        QLineSeries* sTasks = new QLineSeries(chart);
        sTasks->setName(QString::fromUtf8("Задач за неделю"));

        QLineSeries* sKinds = new QLineSeries(chart);
        sKinds->setName(QString::fromUtf8("Разных номеров"));

        // group by ISO week (year-week)
        QMap<QString, int> sumTasks;
        QMap<QString, QSet<int>> distinct;
        QMap<QString, QDate> weekDate;

        for (const DaySession& ds : days) {
            int y = 0, w = 0;
            ds.date.getDate(&y, nullptr, nullptr);
            w = ds.date.weekNumber(&y);
            QString key = QString("%1-W%2").arg(y).arg(w, 2, 10, QChar('0'));
            sumTasks[key] += ds.doneCount;
            for (QMap<int, QPair<int,int>>::const_iterator it = ds.byTask.begin(); it != ds.byTask.end(); ++it) {
                if (it.value().first > 0) distinct[key].insert(it.key());
            }
            if (!weekDate.contains(key) || ds.date < weekDate[key]) weekDate[key] = ds.date;
        }

        QStringList keys = sumTasks.keys();
        std::sort(keys.begin(), keys.end(), [&](const QString& a, const QString& b){
            return weekDate.value(a) < weekDate.value(b);
        });

        QStringList cats;
        int x = 0;
        for (const QString& k : keys) {
            sTasks->append(x, sumTasks.value(k));
            sKinds->append(x, distinct.value(k).size());
            // label as week range (Mon–Sun)
            QDate ws = weekDate.value(k);
            QString lbl = ws.isValid() ? (ws.toString("dd.MM") + "-" + ws.addDays(6).toString("dd.MM")) : k;
            cats << lbl;
            x++;
        }

        chart->addSeries(sTasks);
        chart->addSeries(sKinds);

        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(cats, 25));
        tuneCategoryAxis(axX, -60);

        QValueAxis* axY = new QValueAxis(chart);
        axY->setMin(0);

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        sTasks->attachAxis(axX); sTasks->attachAxis(axY);
        sKinds->attachAxis(axX); sKinds->attachAxis(axY);

        QObject::connect(sTasks, &QLineSeries::hovered, this, [cats](const QPointF& p, bool state){
            if (!state) return;
            int idx = (int)qRound(p.x());
            if (idx >= 0 && idx < cats.size()) {
                QToolTip::showText(QCursor::pos(), QString::fromUtf8("Неделя %1: задач %2").arg(cats[idx]).arg((int)qRound(p.y())));
            }
        });
        QObject::connect(sKinds, &QLineSeries::hovered, this, [cats](const QPointF& p, bool state){
            if (!state) return;
            int idx = (int)qRound(p.x());
            if (idx >= 0 && idx < cats.size()) {
                QToolTip::showText(QCursor::pos(), QString::fromUtf8("Неделя %1: разных номеров %2").arg(cats[idx]).arg((int)qRound(p.y())));
            }
        });

        cvWeeklyLoad_->setChart(chart);
    }

    // Contribution
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Вклад каждого номера в балл"));
        chart->legend()->hide();

        QBarSeries* series = new QBarSeries(chart);
        QBarSet* set = new QBarSet(QString::fromUtf8("Баллы"));
        QStringList cats;

        for (int no=1; no<=25; ++no) {
            cats << QString::number(no);
            if (tasks.contains(no)) {
                TaskAgg t = tasks.value(no);
                double pts = (double)t.maxPoints * (double)t.mastery / 100.0;
                set->append(pts);
            } else set->append(0.0);
        }
        series->append(set);
        chart->addSeries(series);
        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(cats, 25));
        tuneCategoryAxis(axX, -60);
        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 2.0);
        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        QObject::connect(set, &QBarSet::hovered, this, [cats, set](bool status, int index){
            if (!status) return;
            if (index < 0 || index >= cats.size()) return;
            double v = set->at(index);
            QToolTip::showText(QCursor::pos(), QString::fromUtf8("№%1: %2 балла")
                               .arg(cats[index])
                               .arg(QString::number(v, 'f', 2)));
        });

        series->attachAxis(axX); series->attachAxis(axY);

        cvContrib_->setChart(chart);
    }

    // Bottlenecks
    {
        QChart* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Потенциал роста (до 70%)"));
        chart->legend()->hide();

        struct BN { int no; double gain; int mastery; };
        QVector<BN> bns;
        int target=70;
        for (QMap<int, TaskAgg>::const_iterator it = tasks.begin(); it != tasks.end(); ++it) {
            const TaskAgg& t = it.value();
            if (t.mastery >= target) continue;
            double gain = (double)(target - t.mastery)/100.0 * (double)t.maxPoints;
            BN x; x.no=t.taskNo; x.gain=gain; x.mastery=t.mastery;
            bns.push_back(x);
        }
        std::sort(bns.begin(), bns.end(), [](const BN& a, const BN& b){
            if (a.gain != b.gain) return a.gain > b.gain;
            return a.no < b.no;
        });
        if (bns.size() > 10) bns.resize(10);

        QBarSeries* series = new QBarSeries(chart);
        QBarSet* set = new QBarSet(QString::fromUtf8("+ баллы"));
        QStringList cats;
        for (const BN& x : bns) { cats << QString("№%1").arg(x.no); set->append(x.gain); }
        series->append(set);
        chart->addSeries(series);
        QBarCategoryAxis* axX = new QBarCategoryAxis(chart);
        axX->append(downsampleLabels(cats, 25));
        QValueAxis* axY = new QValueAxis(chart);
        axY->setRange(0, 2.5);
        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        QObject::connect(set, &QBarSet::hovered, this, [cats, set](bool status, int index){
            if (!status) return;
            if (index < 0 || index >= cats.size()) return;
            double v = set->at(index);
            QToolTip::showText(QCursor::pos(), QString::fromUtf8("%1: +%2 балла до 70%%")
                               .arg(cats[index])
                               .arg(QString::number(v, 'f', 2)));
        });

        series->attachAxis(axX); series->attachAxis(axY);
        cvBottlenecks_->setChart(chart);
    }

    // Per-task charts
    {
        QChart* chartVar = new QChart();
        QChart* chartAtt = new QChart();
        QChart* chartFor = new QChart();

        if (selectedTaskNo_ <= 0) {
            chartVar->setTitle(QString::fromUtf8("Выбери номер слева"));
            chartAtt->setTitle(QString::fromUtf8("Выбери номер слева"));
            chartFor->setTitle(QString::fromUtf8("Выбери номер слева"));
        } else {
            TaskAgg t = tasks.value(selectedTaskNo_);
            chartVar->setTitle(QString::fromUtf8("№%1: мастерство по вариациям").arg(selectedTaskNo_));
            chartAtt->setTitle(QString::fromUtf8("№%1: попытки по дням").arg(selectedTaskNo_));
            chartFor->setTitle(QString::fromUtf8("№%1: эффект повторений").arg(selectedTaskNo_));
            chartFor->legend()->setVisible(true);
            chartFor->legend()->setAlignment(Qt::AlignBottom);

            QBarSeries* series = new QBarSeries(chartVar);
            QBarSet* set1 = new QBarSet(QString::fromUtf8("Мастерство"));
            QStringList cats;

            for (QMap<QString, VarAgg>::const_iterator it = t.vars.begin(); it != t.vars.end(); ++it) {
                QString var = it.key();
                QString name = var;
                if (catalog_.contains(selectedTaskNo_) && catalog_[selectedTaskNo_].vars.contains(var))
                    name = catalog_[selectedTaskNo_].vars[var].displayName;
                cats << name.left(18);
                set1->append(it.value().mastery);
            }

            series->append(set1);
chartVar->addSeries(series);
            QBarCategoryAxis* axX = new QBarCategoryAxis(chartVar);
            axX->append(downsampleLabels(cats, 25));
            tuneCategoryAxis(axX, -60);
            QValueAxis* axY = new QValueAxis(chartVar);
            axY->setRange(0, 100);
            chartVar->addAxis(axX, Qt::AlignBottom);
            chartVar->addAxis(axY, Qt::AlignLeft);
            series->attachAxis(axX); series->attachAxis(axY);
            QObject::connect(set1, &QBarSet::hovered, this, [cats, set1](bool status, int index){
                if (!status) return;
                if (index < 0 || index >= cats.size()) return;
                QToolTip::showText(QCursor::pos(), QString::fromUtf8("%1: %2%%")
                                   .arg(cats[index])
                                   .arg((int)set1->at(index)));
            });
            QLineSeries* sA = new QLineSeries(chartAtt);
            sA->setName(QString::fromUtf8("Попытки"));
            QLineSeries* sC = new QLineSeries(chartAtt);
            sC->setName(QString::fromUtf8("Верно"));

            QLineSeries* sA2 = nullptr;
            QLineSeries* sC2 = nullptr;
            int cmp = (chartsCompare_ && chartsCompare_->isChecked()) ? compareTaskNo_ : 0;
            if (cmp > 0 && cmp != selectedTaskNo_) {
                sA2 = new QLineSeries(chartAtt);
                sA2->setName(QString::fromUtf8("Попытки №%1").arg(cmp));
                sC2 = new QLineSeries(chartAtt);
                sC2->setName(QString::fromUtf8("Верно №%1").arg(cmp));
            }

            QStringList cats2;
            int x=0;
            for (const DaySession& ds : days) {
                int att=0, cor=0;
                if (ds.byTask.contains(selectedTaskNo_)) { att = ds.byTask[selectedTaskNo_].first; cor = ds.byTask[selectedTaskNo_].second; }
                sA->append(x, att);
                sC->append(x, cor);
                if (sA2 && sC2) {
                    int att2 = 0, cor2 = 0;
                    if (ds.byTask.contains(cmp)) { att2 = ds.byTask[cmp].first; cor2 = ds.byTask[cmp].second; }
                    sA2->append(x, att2);
                    sC2->append(x, cor2);
                }
                cats2 << ds.date.toString("dd.MM.yyyy");
                x++;
            }
            chartAtt->addSeries(sA);
            chartAtt->addSeries(sC);
            if (sA2 && sC2) { chartAtt->addSeries(sA2); chartAtt->addSeries(sC2); }
            QBarCategoryAxis* ax2 = new QBarCategoryAxis(chartAtt);
            QStringList shortCats2;
            for (const QString& s : cats2) shortCats2 << s.left(5);
            ax2->append(downsampleLabels(shortCats2, 12));
            ax2->setLabelsAngle(-60);
            QValueAxis* ay2 = new QValueAxis(chartAtt);
            ay2->setMin(0);
            chartAtt->addAxis(ax2, Qt::AlignBottom);
            chartAtt->addAxis(ay2, Qt::AlignLeft);
            sA->attachAxis(ax2); sA->attachAxis(ay2);
            sC->attachAxis(ax2); sC->attachAxis(ay2);
            if (sA2 && sC2) { sA2->attachAxis(ax2); sA2->attachAxis(ay2); sC2->attachAxis(ax2); sC2->attachAxis(ay2); }

            QObject::connect(sA, &QLineSeries::hovered, this, [cats2](const QPointF& p, bool state){
                if (!state) return;
                int idx = (int)qRound(p.x());
                if (idx >= 0 && idx < cats2.size()) {
                    QToolTip::showText(QCursor::pos(),
                                       QString::fromUtf8("Дата: %1\nПопытки: %2")
                                       .arg(cats2[idx]).arg((int)qRound(p.y())));
                }
            });
            QObject::connect(sC, &QLineSeries::hovered, this, [cats2](const QPointF& p, bool state){
                if (!state) return;
                int idx = (int)qRound(p.x());
                if (idx >= 0 && idx < cats2.size()) {
                    QToolTip::showText(QCursor::pos(),
                                       QString::fromUtf8("Дата: %1\nВерно: %2")
                                       .arg(cats2[idx]).arg((int)qRound(p.y())));
                }
            });

            QLineSeries* sF = new QLineSeries(chartFor);
            sF->setName(QString::fromUtf8("№%1").arg(selectedTaskNo_));
            QPen p1 = sF->pen();
            p1.setWidth(3);
            p1.setColor(QColor(37, 99, 235)); // blue
            p1.setStyle(Qt::SolidLine);
            sF->setPen(p1);

            QLineSeries* sF2 = nullptr;
            int cmpNo2 = (chartsCompare_ && chartsCompare_->isChecked()) ? compareTaskNo_ : 0;
            TaskAgg tCmp;
            bool hasCmp2 = false;
            if (cmpNo2 > 0 && cmpNo2 != selectedTaskNo_ && tasks.contains(cmpNo2)) { tCmp = tasks.value(cmpNo2); hasCmp2 = true; }
            if (hasCmp2) {
                sF2 = new QLineSeries(chartFor);
                sF2->setName(QString::fromUtf8("№%1").arg(cmpNo2));
                QPen p2 = sF2->pen();
                p2.setWidth(3);
                p2.setColor(QColor(239, 68, 68)); // red
                p2.setStyle(Qt::DashLine);
                sF2->setPen(p2);
            }
            double tau = 18.0;
            int horizon = 30;
            int idleDays = 0;
            if (t.lastTs.isValid()) idleDays = t.lastTs.date().daysTo(QDate::currentDate());
            if (idleDays < 0) idleDays = 0;
            double mToday = (double)t.mastery * std::exp(-(double)idleDays/tau);
            if (mToday < 0.0) mToday = 0.0;
            if (mToday > 100.0) mToday = 100.0;
            for (int d=0; d<=horizon; ++d) {
                double m = mToday * std::exp(-(double)d/tau);
                if (m < 0.0) m = 0.0;
                if (m > 100.0) m = 100.0;
                sF->append(d, m);
            }
            if (sF2) {
                int idle2 = 0;
                if (tCmp.lastTs.isValid()) idle2 = tCmp.lastTs.date().daysTo(QDate::currentDate());
                if (idle2 < 0) idle2 = 0;
                double mToday2 = (double)tCmp.mastery * std::exp(-(double)idle2/tau);
                if (mToday2 < 0.0) mToday2 = 0.0;
                if (mToday2 > 100.0) mToday2 = 100.0;
                for (int d=0; d<=horizon; ++d) {
                    double m2 = mToday2 * std::exp(-(double)d/tau);
                    if (m2 < 0.0) m2 = 0.0;
                    if (m2 > 100.0) m2 = 100.0;
                    sF2->append(d, m2);
                }
            }
            chartFor->addSeries(sF);
            if (sF2) chartFor->addSeries(sF2);
            QValueAxis* axF = new QValueAxis(chartFor);
            axF->setRange(0, horizon);
            QValueAxis* ayF = new QValueAxis(chartFor);
            ayF->setRange(0, qMax(100, t.mastery));
            chartFor->addAxis(axF, Qt::AlignBottom);
            chartFor->addAxis(ayF, Qt::AlignLeft);
            sF->attachAxis(axF);
            sF->attachAxis(ayF);
            if (sF2) { sF2->attachAxis(axF); sF2->attachAxis(ayF); }

            QObject::connect(sF, &QLineSeries::hovered, this, [](const QPointF& p, bool state){
                if (!state) return;
                QToolTip::showText(QCursor::pos(), QString::fromUtf8("День %1: %2%%")
                                   .arg((int)qRound(p.x()))
                                   .arg(QString::number(p.y(),'f',1)));
            });
            if (sF2) {
                QObject::connect(sF2, &QLineSeries::hovered, this, [](const QPointF& p, bool state){
                    if (!state) return;
                    QToolTip::showText(QCursor::pos(), QString::fromUtf8("День %1: %2%%")
                                       .arg((int)qRound(p.x()))
                                       .arg(QString::number(p.y(),'f',1)));
                });
            }
        }

        cvTaskVarMastery_->setChart(chartVar);
        cvTaskAttempts_->setChart(chartAtt);
        cvForgetting_->setChart(chartFor);
    }

    // Heatmap
    {
        QSet<QString> varKeys;
        for (QMap<int, TaskAgg>::const_iterator it = tasks.begin(); it != tasks.end(); ++it) {
            for (QMap<QString, VarAgg>::const_iterator vit = it.value().vars.begin(); vit != it.value().vars.end(); ++vit)
                varKeys.insert(vit.key());
        }
        QStringList cols = varKeys.values();
        std::sort(cols.begin(), cols.end());

        heatmap_->clear();
        heatmap_->setRowCount(25);
        heatmap_->setColumnCount(qMax(1, int(cols.size())));
        heatmap_->setHorizontalHeaderLabels(cols.isEmpty()? QStringList{QString::fromUtf8("—")} : cols);

        for (int r=0;r<25;++r) {
            int no=r+1;
            heatmap_->setVerticalHeaderItem(r, new QTableWidgetItem(QString::number(no)));
            for (int c=0;c<heatmap_->columnCount();++c) {
                QString var = cols.isEmpty()? QString::fromUtf8("—") : cols[c];
                int mastery=0;
                QString tip = QString::fromUtf8("Нет данных");
                if (!cols.isEmpty() && tasks.contains(no) && tasks.value(no).vars.contains(var)) {
                    VarAgg v = tasks.value(no).vars.value(var);
                    mastery=v.mastery;
                    tip = QString::fromUtf8("№%1 / %2\nМастерство: %3%\nРешено: %4\nВерно: %5\nРиск: %6")
                        .arg(no).arg(var).arg(mastery).arg(v.attemptedUnique).arg(v.correctUnique).arg(v.risk);
                }
                QTableWidgetItem* item = new QTableWidgetItem(QString::number(mastery));
                item->setTextAlignment(Qt::AlignCenter);
                item->setToolTip(tip);
                int g = clampInt(245 - int(mastery*1.4), 90, 245);
                item->setBackground(QColor(g,g,g));
                heatmap_->setItem(r,c,item);
            }
        }
        heatmap_->resizeColumnsToContents();
        int w = heatmap_->horizontalHeader()->defaultSectionSize();
        for (int c=0;c<heatmap_->columnCount();++c) heatmap_->setColumnWidth(c, w);
    }
}


static bool writeJsonFileAtomic(const QString& path, const QJsonDocument& doc, QString* err=nullptr)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = "Cannot write: " + path;
        return false;
    }
    const QByteArray data = doc.toJson(QJsonDocument::Indented);
    if (f.write(data) != data.size()) {
        if (err) *err = "Write failed: " + path;
        return false;
    }
    f.close();
    return true;
}

void ProgressPage::reloadAllData()
{
    QString err;

    // 1) load base catalog (names)
    if (!loadCatalog(&err)) {
        if (tree_) {
            tree_->clear();
            QTreeWidgetItem* it = new QTreeWidgetItem(tree_);
            it->setText(0, "catalog.json: " + err);
        }
        return;
    }

    // 2) load all raw data (facts)
    loadProgress(&err);
    loadSessions(&err);
    loadSubmissions(&err);

    // 3) build attempt events + aggregates
    int typeFilter = chartsType_ ? chartsType_->currentData().toInt() : 0;
    QVector<AttemptEvent> events = buildAttemptEvents(typeFilter);

    QMap<int, TaskAgg> tasks = computeAggregates(events);
    OverviewStats ov = computeOverview(tasks);

    cachedTasks_ = tasks;
    cachedOverview_ = ov;

    renderOverview(ov, tasks);
    renderTaskTree(tasks);

    if (chartsTab_) {
        rebuildChartsNav(tasks, ov);
        renderCharts(tasks, ov);
    }

    // 4) build plan.json on-the-fly for Calendar (derived file)
    //    Plan is rebuilt every time user enters "Моя статистика".
    int trainingsPerWeek = 3;
    int dailyTasksTarget = 20;
    int minDiversityTopics = 4;
    int horizonDays = 35;
    QSet<int> plannedWeekdays; // 1..7

    {
        QJsonDocument d;
        QString e;
        const QString p = dataPath("settings.json");
        if (QFile::exists(p) && readJsonFile(p, d, &e)) {
            const QJsonObject o = d.object();
            trainingsPerWeek = o.value("trainingsPerWeek").toInt(trainingsPerWeek);
            dailyTasksTarget = o.value("dailyTasksTarget").toInt(dailyTasksTarget);
            minDiversityTopics = o.value("minDiversityTopics").toInt(minDiversityTopics);
            horizonDays = o.value("planHorizonDays").toInt(horizonDays);

            const QJsonArray wd = o.value("weekdays").toArray();
            for (const auto& v : wd) {
                const int x = v.toInt();
                if (x >= 1 && x <= 7) plannedWeekdays.insert(x);
            }
        }
        if (plannedWeekdays.isEmpty()) {
            // default: Mon/Wed/Fri
            plannedWeekdays.insert(1);
            plannedWeekdays.insert(3);
            plannedWeekdays.insert(5);
        }
        if (trainingsPerWeek <= 0) trainingsPerWeek = 3;
        if (dailyTasksTarget <= 0) dailyTasksTarget = 20;
        if (minDiversityTopics <= 0) minDiversityTopics = 4;
        if (horizonDays < 14) horizonDays = 14;
        if (horizonDays > 90) horizonDays = 90;
    }

    // last practice date per topic (taskNo + variation)
    struct TopicKey { int no; QString var; };
    struct TopicStat {
        int taskNo = 0;
        QString var;
        QString displayName;
        QString taskTitle;
        int mastery = 0;
        int risk = 0;
        int daysSince = 999;
        double due = 1.0;
        double expectedAcc = 0.45;
        double priority = 0.0;
    };

    QMap<QString, QDate> lastByTopic; // key "no|var"
    const QDate today = QDate::currentDate();
    for (const auto& ev : events) {
        const int no = ev.ref.taskNo;
        const QString var = ev.ref.variation;
        if (no <= 0 || var.isEmpty() || !ev.ts.isValid()) continue;
        const QString k = QString::number(no) + "|" + var;
        const QDate d = ev.ts.date();
        if (!lastByTopic.contains(k) || lastByTopic[k] < d) lastByTopic[k] = d;
    }

    QVector<TopicStat> allTopics;
    allTopics.reserve(256);

    for (auto it = tasks.begin(); it != tasks.end(); ++it) {
        const int no = it.key();
        const TaskAgg& ta = it.value();
        // if no variants, skip
        for (auto vit = ta.vars.begin(); vit != ta.vars.end(); ++vit) {
            const QString var = vit.key();
            const VarAgg& va = vit.value();

            TopicStat ts;
            ts.taskNo = no;
            ts.var = var;
            ts.mastery = va.mastery;
            ts.risk = 100 - ts.mastery;
            ts.taskTitle = ta.title;

            // display name from catalog (already normalized, not A/B)
            if (catalog_.contains(no) && catalog_[no].vars.contains(var))
                ts.displayName = catalog_[no].vars[var].displayName;
            if (ts.displayName.isEmpty())
                ts.displayName = var; // fallback

            const QString k = QString::number(no) + "|" + var;
            if (lastByTopic.contains(k)) {
                ts.daysSince = lastByTopic[k].daysTo(today);
            } else {
                ts.daysSince = 999;
            }

            // spaced repetition due
            const double m01 = qBound(0.0, (double)ts.mastery / 100.0, 1.0);
            const double targetDays = 2.0 + 18.0 * m01;
            ts.due = qBound(0.0, (double)ts.daysSince / targetDays, 1.0);

            // expected accuracy ~ mastery
            ts.expectedAcc = qBound(0.15, 0.20 + 0.75*m01, 0.95);

            ts.priority = 0.7 * (double)ts.risk + 0.3 * (ts.due * 100.0);
            allTopics.push_back(ts);
        }
    }

    std::sort(allTopics.begin(), allTopics.end(), [](const TopicStat& a, const TopicStat& b){
        return a.priority > b.priority;
    });

    auto makeReasons = [](const TopicStat& t) -> QJsonArray {
        QJsonArray rs;
        auto add = [&](const QString& code, const QString& text){
            QJsonObject o; o["code"]=code; o["text"]=text; rs.append(o);
        };
        if (t.risk >= 70) add("risk_high", QString::fromUtf8("Риск %1/100").arg(t.risk));
        if (t.mastery <= 40) add("low_mastery", QString::fromUtf8("Низкое мастерство %1/100").arg(t.mastery));
        if (t.daysSince >= 7 && t.daysSince < 900) add("spaced_due", QString::fromUtf8("Не повторял %1 дней").arg(t.daysSince));
        if (t.daysSince >= 900) add("never_practiced", QString::fromUtf8("Ещё не решал в приложении"));
        if (t.due >= 0.8) add("due_now", QString::fromUtf8("Пора повторить (spaced repetition)"));
        return rs;
    };

    auto countFor = [&](const TopicStat& t, int remaining) -> int {
        int c = 1;
        if (t.due >= 0.8) c += 2;
        if (t.mastery <= 40) c += 1;
        if (t.risk >= 80) c += 1;
        c = qBound(1, c, 4);
        if (c > remaining) c = remaining;
        return c;
    };

    // Generate plan days
    QJsonArray planDays;

    QDate d = today;
    int built = 0;

    while (built < horizonDays) {
        const bool isPlanned = plannedWeekdays.contains(d.dayOfWeek());
        if (isPlanned) {
            QJsonObject day;
            day["date"] = d.toString(Qt::ISODate);
            day["planned"] = true;
            day["type"] = "training";
            day["totalTasks"] = dailyTasksTarget;

            QJsonArray items;
            QStringList topicsFlat;

            int remaining = dailyTasksTarget;
            QSet<int> usedTaskNos;
            int diversity = 0;

            // greedy selection
            for (int i=0; i<allTopics.size() && remaining>0; ++i) {
                const TopicStat& t = allTopics[i];

                // encourage diversity: until diversity reached, avoid repeating same taskNo
                if (diversity < minDiversityTopics && usedTaskNos.contains(t.taskNo)) continue;

                const int c = countFor(t, remaining);

                QJsonObject it;
                it["taskNo"] = t.taskNo;
                it["variation"] = t.var;
                it["displayName"] = t.displayName;
                it["taskTitle"] = t.taskTitle;
                it["count"] = c;
                it["risk"] = t.risk;
                it["mastery"] = t.mastery;
                it["daysSince"] = t.daysSince >= 900 ? -1 : t.daysSince;
                it["due"] = t.due;
                it["expectedAccuracy"] = t.expectedAcc;
                it["reasons"] = makeReasons(t);
                items.append(it);

                topicsFlat << QString::fromUtf8("№%1 %2 ×%3").arg(t.taskNo).arg(t.displayName).arg(c);

                remaining -= c;
                if (!usedTaskNos.contains(t.taskNo)) {
                    usedTaskNos.insert(t.taskNo);
                    diversity++;
                }
                if (diversity >= minDiversityTopics && remaining <= dailyTasksTarget/2) {
                    // once minimum diversity achieved, allow filling
                }
            }

            day["items"] = items;
            day["topics"] = QJsonArray::fromStringList(topicsFlat);

            // прогноз качества (чисто подсказка)
            // volume is 1.0 by definition for plan (target)
            const double volume = 1.0;
            double expAcc = 0.45;
            if (!items.isEmpty()) {
                double s=0.0; int n=0;
                for (const auto& iv : items) { s += iv.toObject().value("expectedAccuracy").toDouble(0.45); n++; }
                expAcc = n>0 ? s/(double)n : 0.45;
            }
            const double diversityN = qBound(0.0, (double)diversity / (double)qMax(1, minDiversityTopics), 1.0);
            const double focusWeak = 0.8; // high by construction
            const int score = (int)qRound(100.0 * (0.25*volume + 0.35*expAcc + 0.20*diversityN + 0.20*focusWeak));
            day["dayQuality"] = score;
            day["qualityExplain"] = QString::fromUtf8("План собран из тем с высоким риском и/или давно не повторявшихся.");

            planDays.append(day);
        }

        d = d.addDays(1);
        built++;
    }

    QJsonObject planObj;
    planObj["version"] = 1;
    planObj["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    planObj["horizonDays"] = horizonDays;
    planObj["days"] = planDays;

    // write derived plan.json
    {
        QString e;
        writeJsonFileAtomic(dataPath("plan.json"), QJsonDocument(planObj), &e);
    }

    // 5) push data into CalendarPage
    if (calendarTab_) {
        QVector<CalendarPage::DaySession> sess;
        sess.reserve(sessionDays_.size());
        for (const DaySession& ds : sessionDays_) {
            CalendarPage::DaySession cs;
            cs.date = ds.date;
            cs.doneCount = ds.doneCount;
            cs.correctCount = ds.correctCount;
            cs.durationMin = ds.durationMin;
            cs.type = ds.type;
            cs.mockScore = ds.mockScore;
            cs.mockMax = ds.mockMax;
            cs.byTask = ds.byTask;
            sess.push_back(cs);
        }

        QMap<int, CalendarPage::TaskAggLite> tmap;
        for (QMap<int, TaskAgg>::const_iterator it = cachedTasks_.begin(); it != cachedTasks_.end(); ++it) {
            CalendarPage::TaskAggLite tt;
            tt.taskNo = it.value().taskNo;
            tt.title = it.value().title;
            tt.mastery = it.value().mastery;
            for (QMap<QString, VarAgg>::const_iterator vit = it.value().vars.begin(); vit != it.value().vars.end(); ++vit) {
                CalendarPage::VarAggLite vv;
                vv.mastery = vit.value().mastery;
                if (catalog_.contains(tt.taskNo) && catalog_[tt.taskNo].vars.contains(vit.key()))
                    vv.displayName = catalog_[tt.taskNo].vars[vit.key()].displayName;
                tt.vars.insert(vit.key(), vv);
            }
            tmap.insert(tt.taskNo, tt);
        }

        CalendarPage::OverviewLite ovl;
        ovl.weakTaskNos = cachedOverview_.weakTaskNos;
        ovl.streakDays = cachedOverview_.streakDays;

        calendarTab_->setData(sess, tmap, ovl, planObj, trainingsPerWeek, plannedWeekdays);
    }
}



void ProgressPage::setupWatcher()
{
    watcher_ = new QFileSystemWatcher(this);

    reloadDebounce_ = new QTimer(this);
    reloadDebounce_->setSingleShot(true);
    reloadDebounce_->setInterval(350);
    connect(reloadDebounce_, &QTimer::timeout, this, [this]{ reloadAllData(); });

    connect(watcher_, &QFileSystemWatcher::fileChanged, this, &ProgressPage::onWatchedChanged);
    refreshWatcherPaths();
}

void ProgressPage::refreshWatcherPaths()
{
    if (!watcher_) return;
    QStringList want;
    want << dataPath("catalog.json") << dataPath("progress.json") << dataPath("sessions.json") << dataPath("submissions.json") << dataPath("plan.json");

    QStringList cur = watcher_->files();
    for (const QString& f : cur) watcher_->removePath(f);
    for (const QString& f : want) if (QFile::exists(f)) watcher_->addPath(f);
}

void ProgressPage::onWatchedChanged()
{
    refreshWatcherPaths();
    if (reloadDebounce_) reloadDebounce_->start();
}

void ProgressPage::applyProductStyles()
{
    // Форсируем светлую палитру внутри страницы, чтобы системная тёмная тема не ломала читабельность.
    setStyleSheet(R"(
        QWidget { background: #f5f6f8; color: #111827; }
        QTabWidget::pane { border: 1px solid #e5e7eb; border-radius: 10px; background: #ffffff; }
        QTabBar::tab {
            padding: 8px 12px; margin-right: 4px;
            border: 1px solid #e5e7eb; border-bottom: none;
            border-top-left-radius: 8px; border-top-right-radius: 8px;
            background: #f3f4f6; color: #111827;
        }
        QTabBar::tab:selected { background: #ffffff; font-weight: 600; }

        QLabel#overviewTitle { font-size: 18px; font-weight: 800; color: #111827; }
        QLabel#chartsTitle { font-size: 18px; font-weight: 800; color: #111827; }

        QPushButton#backBtn {
            padding: 6px 10px;
            border-radius: 10px;
            border: 1px solid #e5e7eb;
            background: #ffffff;
            color:#111827;
        }
        QPushButton#backBtn:hover { background:#f3f4f6; }

        QFrame#statCard, QFrame#card {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
        }
        QLabel#statCardTitle, QLabel#cardText { color: #374151; }
        QLabel#statCardValue { color: #111827; font-weight: 800; }
        QLabel#cardTitle { font-size: 13px; font-weight: 700; color: #111827; }

        QTreeWidget#taskTree, QTreeWidget#chartsNav {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            color: #111827;
            alternate-background-color: #f9fafb;
        }
        QHeaderView::section {
            background: #f3f4f6;
            color: #111827;
            padding: 6px 8px;
            border: 0px;
            border-bottom: 1px solid #e5e7eb;
            font-weight: 600;
        }

        QScrollArea#chartsScroll { border: none; background: transparent; }
        QTableWidget#heatmap {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            gridline-color: #e5e7eb;
        }
    )");
}