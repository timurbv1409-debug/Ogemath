#include "detailpage.h"

#include <QTabWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTableView>
#include <QTableWidget>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextEdit>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QToolTip>
#include <QCursor>
#include <QtMath>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>

// В Qt 6 классы QtCharts доступны без пространства имён QtCharts (в Qt 5 было иначе).
// Поэтому НЕ используем `using QtCharts::...` и пишем типы напрямую: QChartView/QChart/...

static int clampInt(int v, int lo, int hi) { if (v<lo) return lo; if (v>hi) return hi; return v; }

enum Col {
    C_TS = 0,
    C_SOURCE,
    C_TASKNO,
    C_VAR,
    C_TASKID,
    C_RESULT,
    C_SCORE,
    C_SESS,
    C__COUNT
};

enum Roles {
    R_Ts = Qt::UserRole + 1,
    R_Source,
    R_TaskNo,
    R_VarKey,
    R_Result,
    R_Score,
    R_SessType,
    R_SessDate
};

EventsProxyFilter::EventsProxyFilter(QObject* parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void EventsProxyFilter::setTextQuery(const QString& q) { q_ = q.trimmed().toLower(); refresh(); }
void EventsProxyFilter::setPeriodDays(int days) { periodDays_ = days; refresh(); }
void EventsProxyFilter::setSourceFilter(const QString& s) { source_ = s; refresh(); }
void EventsProxyFilter::setSessionTypeFilter(const QString& s) { sessType_ = s; refresh(); }
void EventsProxyFilter::setOnlyWrong(bool v) { onlyWrong_ = v; refresh(); }
void EventsProxyFilter::setOnlyPartial(bool v) { onlyPartial_ = v; refresh(); }
void EventsProxyFilter::setTaskFilter(int taskNo) { taskFilter_ = taskNo; refresh(); }

void EventsProxyFilter::refresh()
{
    invalidateFilter(); // protected, but we're inside subclass -> OK
}

bool EventsProxyFilter::filterAcceptsRow(int source_row, const QModelIndex& parent) const
{
    const auto sm = sourceModel();
    if (!sm) return true;

    const QModelIndex idxNo   = sm->index(source_row, C_TASKNO, parent);
    const QModelIndex idxVar  = sm->index(source_row, C_VAR, parent);
    const QModelIndex idxId   = sm->index(source_row, C_TASKID, parent);

    const int taskNo = sm->data(idxNo, R_TaskNo).toInt();
    const QString src = sm->data(sm->index(source_row, C_SOURCE, parent), R_Source).toString();
    const QString res = sm->data(sm->index(source_row, C_RESULT, parent), R_Result).toString();
    const QString st  = sm->data(sm->index(source_row, C_SESS, parent), R_SessType).toString();
    const QDateTime ts = sm->data(sm->index(source_row, C_TS, parent), R_Ts).toDateTime();

    if (taskFilter_ > 0 && taskNo != taskFilter_) return false;

    if (source_ != "all" && src != source_) return false;

    if (sessType_ != "all") {
        if (sessType_ == "unknown") { if (!st.isEmpty() && st != "unknown") return false; }
        else if (st != sessType_) return false;
    }

    if (onlyWrong_ && res != "wrong") return false;
    if (onlyPartial_ && res != "partial") return false;

    if (periodDays_ > 0) {
        const QDate minD = QDate::currentDate().addDays(-periodDays_ + 1);
        if (ts.isValid() && ts.date() < minD) return false;
        // если ts невалиден — считаем, что событие старое и скрываем в коротких периодах
        if (!ts.isValid()) return false;
    }

    if (!q_.isEmpty()) {
        const QString sNo = QString::number(taskNo);
        const QString sVar = sm->data(idxVar, Qt::DisplayRole).toString().toLower();
        const QString sId  = sm->data(idxId, Qt::DisplayRole).toString().toLower();
        if (!sNo.contains(q_) && !sVar.contains(q_) && !sId.contains(q_)) return false;
    }

    return true;
}

// ---------------- DetailPage ----------------

DetailPage::DetailPage(QWidget* parent) : QWidget(parent)
{
    buildUi();
}

void DetailPage::reloadFromDataDir(const QString& dataDir)
{
    dataDir_ = dataDir;
    loadAll();
}

void DetailPage::setQuickFilterTask(int taskNo)
{
    selectedTaskNo_ = taskNo;
    if (taskPickBox_) {
        // 0 -> "Все"
        const int idx = taskPickBox_->findData(taskNo);
        if (idx >= 0) taskPickBox_->setCurrentIndex(idx);
    }
    if (eventsProxy_) eventsProxy_->setTaskFilter(taskNo);
    if (stack_) { nav_->setCurrentRow(0); stack_->setCurrentIndex(0); }
}

QString DetailPage::resolvePath(const QString& name) const
{
    const QString base = QDir::currentPath();
    QStringList candidates;

    if (!dataDir_.isEmpty()) {
        const QString dd = QDir(dataDir_).absolutePath();
        candidates << QDir(dd).filePath(name);
        candidates << QDir(dd).filePath("data/" + name);
    }

    candidates << QDir(base).filePath("data/" + name)
               << QDir(base).filePath("../data/" + name)
               << QDir(base).filePath("../../data/" + name)
               << QDir(base).filePath("../../../data/" + name)
               << QDir(base).filePath(name);

    for (const QString& p : candidates) {
        const QString clean = QDir::cleanPath(p);
        if (QFile::exists(clean)) return clean;
    }
    return QDir::cleanPath(QDir(base).filePath("data/" + name));
}

bool DetailPage::readJson(const QString& name, QJsonDocument& out, QString* err) const
{
    const QString p = resolvePath(name);
    QFile f(p);
    if (!f.exists()) { if (err) *err = "not found: " + p; return false; }
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = "cannot open: " + p; return false; }
    const QByteArray b = f.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(b);
    if (doc.isNull()) { if (err) *err = "bad json: " + p; return false; }
    out = doc;
    return true;
}

QString DetailPage::fmtResult(const QString& r) const
{
    if (r == "ok") return QString::fromUtf8("✅ верно");
    if (r == "partial") return QString::fromUtf8("⚠️ частично");
    return QString::fromUtf8("❌ неверно");
}

QString DetailPage::safeVarName(int taskNo, const QString& varKey) const
{
    if (catalog_.contains(taskNo) && catalog_[taskNo].vars.contains(varKey))
        return catalog_[taskNo].vars[varKey].name;
    return varKey;
}

void DetailPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    auto* top = new QHBoxLayout();
    auto* title = new QLabel(QString::fromUtf8("Детально"), this);
    title->setStyleSheet("font-size:18px;font-weight:800;");
    top->addWidget(title);
    top->addStretch(1);
    root->addLayout(top);

    auto* split = new QSplitter(this);
    split->setOrientation(Qt::Horizontal);

    nav_ = new QListWidget(split);
    nav_->addItem(QString::fromUtf8("События"));
    nav_->addItem(QString::fromUtf8("День"));
    nav_->addItem(QString::fromUtf8("Номер"));
    nav_->addItem(QString::fromUtf8("Проверка"));
    nav_->setFixedWidth(160);

    stack_ = new QStackedWidget(split);

    QWidget* p0 = new QWidget(stack_);
    QWidget* p1 = new QWidget(stack_);
    QWidget* p2 = new QWidget(stack_);
    QWidget* p3 = new QWidget(stack_);

    buildEventsTab(p0);
    buildDayTab(p1);
    buildTaskTab(p2);
    buildCheckTab(p3);

    stack_->addWidget(p0);
    stack_->addWidget(p1);
    stack_->addWidget(p2);
    stack_->addWidget(p3);

    split->addWidget(nav_);
    split->addWidget(stack_);
    split->setStretchFactor(1, 1);

    root->addWidget(split, 1);

    connect(nav_, &QListWidget::currentRowChanged, this, &DetailPage::onNavChanged);

    nav_->setCurrentRow(0);
}

void DetailPage::buildEventsTab(QWidget* page)
{
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(8);

    // filters
    auto* filters = new QHBoxLayout();
    filters->setSpacing(8);

    searchEdit_ = new QLineEdit(page);
    searchEdit_->setPlaceholderText(QString::fromUtf8("Поиск: № / вариация / taskId"));
    searchEdit_->setMinimumWidth(220);

    periodBox_ = new QComboBox(page);
    periodBox_->addItem(QString::fromUtf8("7 дней"), 7);
    periodBox_->addItem(QString::fromUtf8("30 дней"), 30);
    periodBox_->addItem(QString::fromUtf8("90 дней"), 90);
    periodBox_->addItem(QString::fromUtf8("Всё"), 0);
    periodBox_->setCurrentIndex(1);

    sourceBox_ = new QComboBox(page);
    sourceBox_->addItem(QString::fromUtf8("Всё"), "all");
    sourceBox_->addItem(QString::fromUtf8("app"), "app");
    sourceBox_->addItem(QString::fromUtf8("bot"), "bot");

    sessTypeBox_ = new QComboBox(page);
    sessTypeBox_->addItem(QString::fromUtf8("Все типы"), "all");
    sessTypeBox_->addItem(QString::fromUtf8("Тренировки"), "training");
    sessTypeBox_->addItem(QString::fromUtf8("Пробники"), "mock");
    sessTypeBox_->addItem(QString::fromUtf8("Отдых"), "rest");
    sessTypeBox_->addItem(QString::fromUtf8("Неизвестно"), "unknown");

    onlyWrongBox_ = new QCheckBox(QString::fromUtf8("Только ошибки"), page);
    onlyPartialBox_ = new QCheckBox(QString::fromUtf8("Только частичные"), page);

    taskPickBox_ = new QComboBox(page);
    taskPickBox_->addItem(QString::fromUtf8("Все №"), 0);
    for (int i=1;i<=25;++i) taskPickBox_->addItem(QString("№%1").arg(i), i);

    filters->addWidget(searchEdit_);
    filters->addWidget(periodBox_);
    filters->addWidget(sourceBox_);
    filters->addWidget(sessTypeBox_);
    filters->addWidget(onlyWrongBox_);
    filters->addWidget(onlyPartialBox_);
    filters->addWidget(taskPickBox_);
    filters->addStretch(1);

    root->addLayout(filters);

    auto* split = new QSplitter(page);
    split->setOrientation(Qt::Horizontal);

    eventsView_ = new QTableView(split);
    eventsView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    eventsView_->setSelectionMode(QAbstractItemView::SingleSelection);
    eventsView_->setSortingEnabled(true);

    QWidget* right = new QWidget(split);
    auto* rlay = new QVBoxLayout(right);
    rlay->setContentsMargins(8,0,0,0);
    rlay->setSpacing(8);

    daySummary_ = new QTextEdit(right);
    daySummary_->setReadOnly(true);
    daySummary_->setMinimumHeight(120);

    dayByTask_ = new QTableWidget(right);
    dayByTask_->setColumnCount(3);
    dayByTask_->setHorizontalHeaderLabels({QString::fromUtf8("№"), QString::fromUtf8("Попытки"), QString::fromUtf8("Верно")});
    dayByTask_->horizontalHeader()->setStretchLastSection(true);
    dayByTask_->verticalHeader()->setVisible(false);
    dayByTask_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    dayByVar_ = new QTableWidget(right);
    dayByVar_->setColumnCount(4);
    dayByVar_->setHorizontalHeaderLabels({QString::fromUtf8("№"), QString::fromUtf8("Вариация"), QString::fromUtf8("Попытки"), QString::fromUtf8("Верно")});
    dayByVar_->horizontalHeader()->setStretchLastSection(true);
    dayByVar_->verticalHeader()->setVisible(false);
    dayByVar_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    rlay->addWidget(new QLabel(QString::fromUtf8("День подробно"), right));
    rlay->addWidget(daySummary_);
    rlay->addWidget(dayByTask_, 1);
    rlay->addWidget(dayByVar_, 1);

    split->addWidget(eventsView_);
    split->addWidget(right);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 1);

    root->addWidget(split, 1);

    eventsModel_ = new QStandardItemModel(this);
    eventsModel_->setColumnCount(C__COUNT);
    eventsModel_->setHorizontalHeaderLabels({
        QString::fromUtf8("Дата-время"),
        QString::fromUtf8("Источник"),
        QString::fromUtf8("№"),
        QString::fromUtf8("Вариация"),
        QString::fromUtf8("taskId"),
        QString::fromUtf8("Результат"),
        QString::fromUtf8("score"),
        QString::fromUtf8("Сессия")
    });

    eventsProxy_ = new EventsProxyFilter(this);
    eventsProxy_->setSourceModel(eventsModel_);
    eventsView_->setModel(eventsProxy_);
    eventsView_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    eventsView_->horizontalHeader()->setStretchLastSection(true);

    connect(searchEdit_, &QLineEdit::textChanged, this, &DetailPage::onFiltersChanged);
    connect(periodBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DetailPage::onFiltersChanged);
    connect(sourceBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DetailPage::onFiltersChanged);
    connect(sessTypeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DetailPage::onFiltersChanged);
    connect(onlyWrongBox_, &QCheckBox::toggled, this, &DetailPage::onFiltersChanged);
    connect(onlyPartialBox_, &QCheckBox::toggled, this, &DetailPage::onFiltersChanged);
    connect(taskPickBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DetailPage::onFiltersChanged);
    connect(eventsView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DetailPage::onEventSelectionChanged);
}

void DetailPage::buildDayTab(QWidget* page)
{
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(8);

    dayTitle_ = new QLabel(QString::fromUtf8("Выбери день"), page);
    dayTitle_->setStyleSheet("font-size:16px;font-weight:700;");
    dayText_ = new QTextEdit(page);
    dayText_->setReadOnly(true);

    dayTasksTable_ = new QTableWidget(page);
    dayTasksTable_->setColumnCount(3);
    dayTasksTable_->setHorizontalHeaderLabels({QString::fromUtf8("№"), QString::fromUtf8("Попытки"), QString::fromUtf8("Верно")});
    dayTasksTable_->horizontalHeader()->setStretchLastSection(true);
    dayTasksTable_->verticalHeader()->setVisible(false);
    dayTasksTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    root->addWidget(dayTitle_);
    root->addWidget(dayText_);
    root->addWidget(dayTasksTable_, 1);
}

void DetailPage::buildTaskTab(QWidget* page)
{
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(8);

    auto* top = new QHBoxLayout();
    taskBox_ = new QComboBox(page);
    taskBox_->addItem(QString::fromUtf8("Выбери №"), 0);
    for (int i=1;i<=25;++i) taskBox_->addItem(QString("№%1").arg(i), i);

    showTaskEventsBtn_ = new QPushButton(QString::fromUtf8("Показать события номера"), page);

    top->addWidget(taskBox_);
    top->addWidget(showTaskEventsBtn_);
    top->addStretch(1);

    taskText_ = new QTextEdit(page);
    taskText_->setReadOnly(true);

    taskVarsTable_ = new QTableWidget(page);
    taskVarsTable_->setColumnCount(4);
    taskVarsTable_->setHorizontalHeaderLabels({QString::fromUtf8("Вариация"), QString::fromUtf8("Всего"), QString::fromUtf8("Решено"), QString::fromUtf8("Верно")});
    taskVarsTable_->horizontalHeader()->setStretchLastSection(true);
    taskVarsTable_->verticalHeader()->setVisible(false);
    taskVarsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QWidget* charts = new QWidget(page);
    auto* ch = new QHBoxLayout(charts);
    ch->setContentsMargins(0,0,0,0);
    ch->setSpacing(10);

    taskAttemptsChart_ = new QChartView(new QChart(), charts);
    taskAttemptsChart_->setMinimumHeight(260);
    taskAccuracyChart_ = new QChartView(new QChart(), charts);
    taskAccuracyChart_->setMinimumHeight(260);
    ch->addWidget(taskAttemptsChart_, 1);
    ch->addWidget(taskAccuracyChart_, 1);

    root->addLayout(top);
    root->addWidget(taskText_);
    root->addWidget(taskVarsTable_, 1);
    root->addWidget(charts);

    connect(taskBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{
        rebuildTaskView(taskBox_->currentData().toInt());
    });
    connect(showTaskEventsBtn_, &QPushButton::clicked, this, &DetailPage::onShowTaskEventsClicked);
}

void DetailPage::buildCheckTab(QWidget* page)
{
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(8);

    checkText_ = new QTextEdit(page);
    checkText_->setReadOnly(true);
    root->addWidget(checkText_, 1);
}

void DetailPage::onNavChanged()
{
    if (!stack_) return;
    stack_->setCurrentIndex(nav_->currentRow());
}

void DetailPage::onFiltersChanged()
{
    if (!eventsProxy_) return;
    eventsProxy_->setTextQuery(searchEdit_ ? searchEdit_->text() : "");
    eventsProxy_->setPeriodDays(periodBox_ ? periodBox_->currentData().toInt() : 0);
    eventsProxy_->setSourceFilter(sourceBox_ ? sourceBox_->currentData().toString() : "all");
    eventsProxy_->setSessionTypeFilter(sessTypeBox_ ? sessTypeBox_->currentData().toString() : "all");
    eventsProxy_->setOnlyWrong(onlyWrongBox_ && onlyWrongBox_->isChecked());
    eventsProxy_->setOnlyPartial(onlyPartialBox_ && onlyPartialBox_->isChecked());
    eventsProxy_->setTaskFilter(taskPickBox_ ? taskPickBox_->currentData().toInt() : 0);
}

void DetailPage::onEventSelectionChanged()
{
    if (!eventsView_ || !eventsProxy_ || !eventsView_->selectionModel()) return;
    const auto rows = eventsView_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;

    const QModelIndex proxyIdx = rows.first();
    const QModelIndex srcIdx = eventsProxy_->mapToSource(proxyIdx);
    const QDate d = eventsModel_->data(eventsModel_->index(srcIdx.row(), C_SESS), R_SessDate).toDate();
    if (d.isValid()) rebuildDayView(d);
}

void DetailPage::onDayPickedFromCalendar(const QDate& d)
{
    if (!d.isValid()) return;
    selectedDate_ = d;
    rebuildDayView(d);
    nav_->setCurrentRow(1);
}

void DetailPage::onShowTaskEventsClicked()
{
    const int t = taskBox_ ? taskBox_->currentData().toInt() : 0;
    if (t <= 0) return;
    setQuickFilterTask(t);
}

void DetailPage::loadAll()
{
    // reset
    catalog_.clear();
    sessionsByDate_.clear();
    lastTouchByTask_.clear();
    events_.clear();

    // catalog.json (required)
    {
        QJsonDocument doc;
        QString err;
        if (readJson("catalog.json", doc, &err)) {
            const QJsonObject root = doc.object();
            const QJsonObject tasks = root.value("tasks").toObject();
            for (auto it = tasks.begin(); it != tasks.end(); ++it) {
                const int no = it.key().toInt();
                const QJsonObject to = it.value().toObject();
                TaskInfo ti;
                ti.title = to.value("title").toString();
                const QJsonObject vars = to.value("variations").toObject();
                for (auto vit = vars.begin(); vit != vars.end(); ++vit) {
                    VarInfo vi;
                    const QJsonObject vo = vit.value().toObject();
                    vi.name = vo.value("displayName").toString(vit.key());
                    vi.total = vo.value("items").toArray().size();
                    ti.vars.insert(vit.key(), vi);
                }
                catalog_.insert(no, ti);
            }
        }
    }

    // sessions.json
    {
        QJsonDocument doc;
        if (readJson("sessions.json", doc, nullptr)) {
            const QJsonObject root = doc.object();
            const QJsonArray days = root.value("days").toArray();
            for (const QJsonValue& dv : days) {
                const QJsonObject o = dv.toObject();
                const QDate d = QDate::fromString(o.value("date").toString(), Qt::ISODate);
                if (!d.isValid()) continue;
                DaySession ds;
                ds.date = d;
                ds.doneCount = o.value("doneCount").toInt();
                ds.correctCount = o.value("correctCount").toInt(ds.doneCount);
                ds.durationMin = o.value("durationMin").toInt(0);
                ds.type = o.value("type").toString("training");
                ds.mockScore = o.value("mockScore").toInt(-1);
                ds.mockMax = o.value("mockMax").toInt(32);

                const QJsonArray summ = o.value("taskSummary").toArray();
                for (const QJsonValue& tv : summ) {
                    const QJsonObject to = tv.toObject();
                    const int task = to.value("task").toInt();
                    const int att = to.value("attempts").toInt();
                    const int cor = to.value("correct").toInt();
                    if (task>=1 && task<=25) {
                        ds.byTask[task] = qMakePair(att, cor);
                        if (att > 0) lastTouchByTask_[task] = d;
                    }
                }
                sessionsByDate_.insert(d, ds);
            }
        }
    }

    // progress.json => app events (done pairs)
    {
        QJsonDocument doc;
        if (readJson("progress.json", doc, nullptr)) {
            const QJsonObject root = doc.object();
            const QJsonObject tasks = root.value("tasks").toObject();
            for (auto it = tasks.begin(); it != tasks.end(); ++it) {
                const int no = it.key().toInt();
                const QJsonObject to = it.value().toObject();
                const QJsonObject vars = to.value("variations").toObject();
                for (auto vit = vars.begin(); vit != vars.end(); ++vit) {
                    const QString varKey = vit.key();
                    const QJsonObject vo = vit.value().toObject();
                    const QJsonArray doneArr = vo.value("done").toArray();
                    for (const QJsonValue& dv : doneArr) {
                        const QJsonArray pair = dv.toArray();
                        if (pair.size() < 2) continue;
                        const int taskId = pair.at(0).toInt();
                        const bool ok = pair.at(1).toBool();

                        DetailEvent e;
                        e.source = "app";
                        e.taskNo = no;
                        e.varKey = varKey;
                        e.varName = safeVarName(no, varKey);
                        e.taskId = taskId;
                        e.result = ok ? "ok" : "wrong";
                        e.score = -1;

                        if (lastTouchByTask_.contains(no))
                            e.ts = QDateTime(lastTouchByTask_[no], QTime(12,0));

                        events_.push_back(e);
                    }
                }
            }
        }
    }

    // submissions.json => bot events (REVIEWED)
    {
        QJsonDocument doc;
        if (readJson("submissions.json", doc, nullptr)) {
            const QJsonObject root = doc.object();
            const QJsonArray subs = root.value("submissions").toArray();
            for (const QJsonValue& sv : subs) {
                const QJsonObject s = sv.toObject();
                if (s.value("status").toString() != "REVIEWED") continue;

                const QJsonObject tr = s.value("taskRef").toObject();
                const int no = tr.value("taskNo").toInt();
                const QString varKey = tr.value("variation").toString();
                const int taskId = tr.value("taskId").toInt();
                if (no < 1 || no > 25 || varKey.isEmpty() || taskId <= 0) continue;

                const QJsonObject rev = s.value("review").toObject();
                const int score = rev.value("score").toInt(-1);

                DetailEvent e;
                e.source = "bot";
                e.taskNo = no;
                e.varKey = varKey;
                e.varName = safeVarName(no, varKey);
                e.taskId = taskId;
                e.score = score;
                if (score >= 80) e.result = "ok";
                else if (score >= 30) e.result = "partial";
                else e.result = "wrong";

                e.ts = QDateTime::fromString(s.value("reviewedAt").toString(), Qt::ISODate);
                if (!e.ts.isValid()) e.ts = QDateTime::fromString(s.value("createdAt").toString(), Qt::ISODate);

                events_.push_back(e);
            }
        }
    }

    // attach session info to events by date
    for (DetailEvent& e : events_) {
        if (!e.ts.isValid()) continue;
        const QDate d = e.ts.date();
        if (sessionsByDate_.contains(d)) {
            e.sessionDate = d;
            e.sessionType = sessionsByDate_[d].type;
        } else {
            e.sessionDate = QDate();
            e.sessionType = "unknown";
        }
    }

    // build UI models
    rebuildEventsModel();
    runChecks();

    // default selections
    if (!selectedDate_.isValid()) selectedDate_ = QDate::currentDate();
    rebuildDayView(selectedDate_);
    rebuildTaskView(taskBox_ ? taskBox_->currentData().toInt() : 0);
}

void DetailPage::rebuildEventsModel()
{
    if (!eventsModel_) return;
    eventsModel_->removeRows(0, eventsModel_->rowCount());

    for (const DetailEvent& e : events_) {
        QList<QStandardItem*> row;
        row.reserve(C__COUNT);

        const QString tsText = e.ts.isValid() ? e.ts.toString("dd.MM.yyyy HH:mm") : QString::fromUtf8("—");
        auto* itTs = new QStandardItem(tsText);
        itTs->setData(e.ts, R_Ts);

        auto* itSrc = new QStandardItem(e.source);
        itSrc->setData(e.source, R_Source);

        auto* itNo = new QStandardItem(QString::number(e.taskNo));
        itNo->setData(e.taskNo, R_TaskNo);

        auto* itVar = new QStandardItem(e.varName);
        itVar->setData(e.varKey, R_VarKey);

        auto* itId = new QStandardItem(QString::number(e.taskId));

        auto* itRes = new QStandardItem(fmtResult(e.result));
        itRes->setData(e.result, R_Result);

        auto* itScore = new QStandardItem(e.score >= 0 ? QString::number(e.score) : QString::fromUtf8("—"));
        itScore->setData(e.score, R_Score);

        QString sess = QString::fromUtf8("—");
        if (e.sessionDate.isValid()) sess = e.sessionDate.toString("dd.MM") + " (" + e.sessionType + ")";
        auto* itSess = new QStandardItem(sess);
        itSess->setData(e.sessionType.isEmpty() ? "unknown" : e.sessionType, R_SessType);
        itSess->setData(e.sessionDate, R_SessDate);

        row << itTs << itSrc << itNo << itVar << itId << itRes << itScore << itSess;
        eventsModel_->appendRow(row);
    }

    if (eventsProxy_) eventsProxy_->refresh();
    if (eventsView_) eventsView_->sortByColumn(C_TS, Qt::DescendingOrder);
}

void DetailPage::rebuildDayView(const QDate& d)
{
    const bool has = sessionsByDate_.contains(d);
    if (!has) {
        if (daySummary_) daySummary_->setText(QString::fromUtf8("Нет записи в sessions.json на %1").arg(d.toString("dd.MM.yyyy")));
        if (dayByTask_) { dayByTask_->setRowCount(0); }
        if (dayByVar_) { dayByVar_->setRowCount(0); }
        if (dayTitle_) dayTitle_->setText(QString::fromUtf8("День: %1").arg(d.toString("dd.MM.yyyy")));
        if (dayText_) dayText_->setText(QString::fromUtf8("Нет данных"));
        if (dayTasksTable_) dayTasksTable_->setRowCount(0);
        return;
    }

    const DaySession& s = sessionsByDate_[d];
    const double acc = s.doneCount>0 ? (100.0 * (double)s.correctCount/(double)s.doneCount) : 0.0;

    const QString head = QString::fromUtf8(
        "Дата: %1\nТип: %2\nРешено: %3\nВерно: %4\nТочность: %5%\nДлительность: %6 мин.\nПробник: %7")
        .arg(d.toString("dd.MM.yyyy"))
        .arg(s.type)
        .arg(s.doneCount)
        .arg(s.correctCount)
        .arg(QString::number(acc,'f',1))
        .arg(s.durationMin)
        .arg(s.type=="mock" && s.mockScore>=0 ? QString("%1/%2").arg(s.mockScore).arg(s.mockMax) : QString::fromUtf8("—"));

    if (daySummary_) daySummary_->setText(head);
    if (dayTitle_) dayTitle_->setText(QString::fromUtf8("День: %1").arg(d.toString("dd.MM.yyyy")));
    if (dayText_) dayText_->setText(head);

    // by task
    if (dayByTask_) {
        dayByTask_->setRowCount(s.byTask.size());
        int r=0;
        for (auto it = s.byTask.begin(); it != s.byTask.end(); ++it, ++r) {
            dayByTask_->setItem(r,0,new QTableWidgetItem(QString::number(it.key())));
            dayByTask_->setItem(r,1,new QTableWidgetItem(QString::number(it.value().first)));
            dayByTask_->setItem(r,2,new QTableWidgetItem(QString::number(it.value().second)));
        }
        dayByTask_->resizeColumnsToContents();
    }
    if (dayTasksTable_) {
        dayTasksTable_->setRowCount(s.byTask.size());
        int r=0;
        for (auto it = s.byTask.begin(); it != s.byTask.end(); ++it, ++r) {
            dayTasksTable_->setItem(r,0,new QTableWidgetItem(QString::number(it.key())));
            dayTasksTable_->setItem(r,1,new QTableWidgetItem(QString::number(it.value().first)));
            dayTasksTable_->setItem(r,2,new QTableWidgetItem(QString::number(it.value().second)));
        }
        dayTasksTable_->resizeColumnsToContents();
    }

    // by variation: from events filtered by this date; if nothing, keep empty but explain
    QMap<QString, QPair<int,int>> byVar; // "no|varkey" -> (att, ok)
    for (const DetailEvent& e : events_) {
        if (e.sessionDate != d) continue;
        const QString k = QString("%1|%2").arg(e.taskNo).arg(e.varKey);
        byVar[k].first += 1;
        if (e.result == "ok") byVar[k].second += 1;
    }

    if (dayByVar_) {
        dayByVar_->setRowCount(byVar.size());
        int r=0;
        for (auto it = byVar.begin(); it != byVar.end(); ++it, ++r) {
            const QStringList parts = it.key().split('|');
            const int no = parts.value(0).toInt();
            const QString vk = parts.value(1);
            dayByVar_->setItem(r,0,new QTableWidgetItem(QString::number(no)));
            dayByVar_->setItem(r,1,new QTableWidgetItem(safeVarName(no, vk)));
            dayByVar_->setItem(r,2,new QTableWidgetItem(QString::number(it.value().first)));
            dayByVar_->setItem(r,3,new QTableWidgetItem(QString::number(it.value().second)));
        }
        dayByVar_->resizeColumnsToContents();
    }
}

void DetailPage::rebuildTaskView(int taskNo)
{
    if (taskNo <= 0 || !catalog_.contains(taskNo)) {
        if (taskText_) taskText_->setText(QString::fromUtf8("Выбери номер"));
        if (taskVarsTable_) taskVarsTable_->setRowCount(0);
        if (taskAttemptsChart_) taskAttemptsChart_->setChart(new QChart());
        if (taskAccuracyChart_) taskAccuracyChart_->setChart(new QChart());
        return;
    }
    selectedTaskNo_ = taskNo;

    // aggregate from events (unique by taskId for app, by taskId for bot too)
    QSet<int> attempted, correct;
    QMap<QString, QSet<int>> attByVar, okByVar;

    QDateTime lastTs;
    for (const DetailEvent& e : events_) {
        if (e.taskNo != taskNo) continue;
        attempted.insert(e.taskId);
        if (e.result == "ok") correct.insert(e.taskId);
        attByVar[e.varKey].insert(e.taskId);
        if (e.result == "ok") okByVar[e.varKey].insert(e.taskId);
        if (e.ts.isValid() && (!lastTs.isValid() || e.ts > lastTs)) lastTs = e.ts;
    }

    const int att = attempted.size();
    const int ok = correct.size();
    const double acc = att>0 ? (100.0*(double)ok/(double)att) : 0.0;
    const int mastery = clampInt((int)qRound(acc), 0, 100);
    const int risk = 100 - mastery;

    QString txt = QString::fromUtf8(
        "№%1 — %2\nПопытки (уник.): %3\nВерно (уник.): %4\nТочность: %5%\nМастерство: %6%\nРиск: %7%\nПоследнее: %8")
        .arg(taskNo)
        .arg(catalog_[taskNo].title)
        .arg(att)
        .arg(ok)
        .arg(QString::number(acc,'f',1))
        .arg(mastery)
        .arg(risk)
        .arg(lastTs.isValid()? lastTs.date().toString("dd.MM.yyyy") : QString::fromUtf8("—"));
    if (taskText_) taskText_->setText(txt);

    // vars table from catalog
    const auto& vars = catalog_[taskNo].vars;
    taskVarsTable_->setRowCount(vars.size());
    int r=0;
    for (auto it = vars.begin(); it != vars.end(); ++it, ++r) {
        const QString vk = it.key();
        const VarInfo& vi = it.value();
        const int a = attByVar.contains(vk) ? attByVar[vk].size() : 0;
        const int c = okByVar.contains(vk) ? okByVar[vk].size() : 0;

        taskVarsTable_->setItem(r,0,new QTableWidgetItem(vi.name));
        taskVarsTable_->setItem(r,1,new QTableWidgetItem(QString::number(vi.total)));
        taskVarsTable_->setItem(r,2,new QTableWidgetItem(QString::number(a)));
        taskVarsTable_->setItem(r,3,new QTableWidgetItem(QString::number(c)));
    }
    taskVarsTable_->resizeColumnsToContents();

    // charts from sessions (byTask)
    QVector<QPointF> ptsA, ptsAcc;
    QStringList cats;
    int x=0;
    QList<QDate> dates = sessionsByDate_.keys();
    std::sort(dates.begin(), dates.end());
    for (const QDate& d : dates) {
        const DaySession& s = sessionsByDate_[d];
        int a=0,c=0;
        if (s.byTask.contains(taskNo)) { a=s.byTask[taskNo].first; c=s.byTask[taskNo].second; }
        ptsA.push_back(QPointF(x, a));
        ptsAcc.push_back(QPointF(x, a>0? (100.0*(double)c/(double)a) : 0.0));
        cats << d.toString("dd.MM");
        x++;
    }

    // attempts chart
    {
        auto* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Попытки/день"));
        auto* s1 = new QLineSeries(chart);
        for (const QPointF& p : ptsA) s1->append(p);
        chart->addSeries(s1);

        auto* axX = new QBarCategoryAxis(chart);
        axX->append(cats);
        axX->setLabelsAngle(-90);
        auto* axY = new QValueAxis(chart);
        axY->setMin(0);

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        s1->attachAxis(axX); s1->attachAxis(axY);

        taskAttemptsChart_->setChart(chart);
    }
    // accuracy chart
    {
        auto* chart = new QChart();
        chart->setTitle(QString::fromUtf8("Точность/день"));
        auto* s1 = new QLineSeries(chart);
        for (const QPointF& p : ptsAcc) s1->append(p);
        chart->addSeries(s1);

        auto* axX = new QBarCategoryAxis(chart);
        axX->append(cats);
        axX->setLabelsAngle(-90);
        auto* axY = new QValueAxis(chart);
        axY->setRange(0, 100);

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        s1->attachAxis(axX); s1->attachAxis(axY);

        taskAccuracyChart_->setChart(chart);
    }
}

void DetailPage::runChecks()
{
    if (!checkText_) return;

    QStringList issues;

    // sessions sums checks
    for (auto it = sessionsByDate_.begin(); it != sessionsByDate_.end(); ++it) {
        const DaySession& s = it.value();
        int sumA=0, sumC=0;
        for (auto jt = s.byTask.begin(); jt != s.byTask.end(); ++jt) { sumA += jt.value().first; sumC += jt.value().second; }
        if (s.doneCount != sumA) issues << QString::fromUtf8("❌ %1: doneCount=%2, сумма attempts=%3").arg(s.date.toString("dd.MM.yyyy")).arg(s.doneCount).arg(sumA);
        if (s.correctCount != sumC) issues << QString::fromUtf8("❌ %1: correctCount=%2, сумма correct=%3").arg(s.date.toString("dd.MM.yyyy")).arg(s.correctCount).arg(sumC);
        if (s.doneCount==0 && !s.byTask.isEmpty()) issues << QString::fromUtf8("❌ %1: doneCount=0, но taskSummary не пуст").arg(s.date.toString("dd.MM.yyyy"));
        if (s.date > QDate::currentDate().addDays(1)) issues << QString::fromUtf8("❌ Будущая дата в sessions: %1").arg(s.date.toString("dd.MM.yyyy"));
    }

    // catalog mismatch
    for (const DetailEvent& e : events_) {
        if (!catalog_.contains(e.taskNo) || !catalog_[e.taskNo].vars.contains(e.varKey)) {
            issues << QString::fromUtf8("❌ Вариация не найдена в catalog: №%1 / %2").arg(e.taskNo).arg(e.varKey);
        }
        if (e.ts.isValid() && e.ts.date() > QDate::currentDate().addDays(1))
            issues << QString::fromUtf8("❌ Будущая дата события: №%1 %2").arg(e.taskNo).arg(e.ts.toString(Qt::ISODate));
    }

    QString out;
    out += QString::fromUtf8("Проверка данных\n\n");
    if (issues.isEmpty()) out += QString::fromUtf8("✅ Проблем не найдено.\n");
    else out += issues.join("\n") + "\n";

    checkText_->setText(out);
}
