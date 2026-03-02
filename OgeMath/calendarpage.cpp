#include "calendarpage.h"

#include <QCalendarWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTextStream>
#include <QToolTip>
#include <QCursor>
#include <QJsonArray>
#include <QJsonValue>
#include <QtMath>

static QString monthNameRu(int m) {
    static const char* names[] = {"",
        "Январь","Февраль","Март","Апрель","Май","Июнь",
        "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"};
    if (m < 1 || m > 12) return QString::fromUtf8("—");
    return QString::fromUtf8(names[m]);
}

static QString colorDot(const QString& hex) {
    // маленький квадрат 12x12 чтобы точно было видно цвет
    return QString("<span style='display:inline-block;width:12px;height:12px;"
                   "border-radius:3px;background:%1;border:1px solid #cbd5e1;"
                   "margin-right:6px;'></span>").arg(hex);
}

CalendarPage::CalendarPage(QWidget* parent) : QWidget(parent) {
    buildUi();
    applyStyles();
}

void CalendarPage::setData(const QVector<DaySession>& sessions,
                           const QMap<int, TaskAggLite>& tasks,
                           const OverviewLite& overview,
                           const QJsonObject& planJson,
                           int trainingsPerWeek,
                           const QSet<int>& plannedWeekdays)
{
    sessions_ = sessions;
    tasks_ = tasks;
    overview_ = overview;
    plan_ = planJson;
    trainingsPerWeek_ = trainingsPerWeek > 0 ? trainingsPerWeek : 3;
    plannedWeekdays_ = plannedWeekdays;
    refreshAll();
}

void CalendarPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // top row
    auto* top = new QHBoxLayout();
    top->setSpacing(10);

    backBtn_ = new QPushButton(QString::fromUtf8("← Назад"), this);
    backBtn_->setObjectName("backBtn");
    backBtn_->setFixedHeight(34);
    connect(backBtn_, &QPushButton::clicked, this, &CalendarPage::backRequested);

    title_ = new QLabel(QString::fromUtf8("Календарь занятий"), this);
    title_->setObjectName("title");

    prevBtn_ = new QPushButton(QString::fromUtf8("◀"), this);
    prevBtn_->setFixedSize(34, 34);
    prevBtn_->setObjectName("navBtn");
    nextBtn_ = new QPushButton(QString::fromUtf8("▶"), this);
    nextBtn_->setFixedSize(34, 34);
    nextBtn_->setObjectName("navBtn");

    monthLabel_ = new QLabel(QString::fromUtf8("—"), this);
    monthLabel_->setObjectName("monthLabel");

    top->addWidget(backBtn_);
    top->addWidget(title_, 1);
    top->addWidget(prevBtn_);
    top->addWidget(monthLabel_);
    top->addWidget(nextBtn_);
    root->addLayout(top);

    // calendar + details
    auto* mid = new QHBoxLayout();
    mid->setSpacing(12);

    cal_ = new QCalendarWidget(this);
    cal_->setObjectName("calendar");
    cal_->setGridVisible(true);
    cal_->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    cal_->setNavigationBarVisible(false); // свой хедер
    connect(cal_, &QCalendarWidget::clicked, this, &CalendarPage::onDayClicked);
    connect(cal_, &QCalendarWidget::currentPageChanged, this, &CalendarPage::onMonthChanged);

    auto* right = new QFrame(this);
    right->setObjectName("card");
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(12, 12, 12, 12);
    rv->setSpacing(10);

    auto* legend = new QLabel(this);
    legend->setObjectName("legendTitle");
    legend->setText(QString::fromUtf8("Легенда"));

    legendPlanned_ = new QLabel(this);
    legendDone_ = new QLabel(this);
    legendMissed_ = new QLabel(this);
    legendMock_ = new QLabel(this);

    legendPlanned_->setText(colorDot("#fde68a") + QString::fromUtf8("План"));
    legendDone_->setText(colorDot("#86efac") + QString::fromUtf8("Занимался"));
    legendMissed_->setText(colorDot("#fca5a5") + QString::fromUtf8("Пропуск (был план)"));
    legendMock_->setText(colorDot("#93c5fd") + QString::fromUtf8("Пробник (рамка)"));

    legendPlanned_->setTextFormat(Qt::RichText);
    legendDone_->setTextFormat(Qt::RichText);
    legendMissed_->setTextFormat(Qt::RichText);
    legendMock_->setTextFormat(Qt::RichText);

    details_ = new QLabel(QString::fromUtf8("Нажми на день — покажу детали."), this);
    details_->setObjectName("details");
    details_->setWordWrap(true);
    details_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    rv->addWidget(legend);
    rv->addWidget(legendPlanned_);
    rv->addWidget(legendDone_);
    rv->addWidget(legendMissed_);
    rv->addWidget(legendMock_);
    rv->addSpacing(10);
    rv->addWidget(details_, 1);

    mid->addWidget(cal_, 2);
    mid->addWidget(right, 1);

    root->addLayout(mid, 1);

    connect(prevBtn_, &QPushButton::clicked, this, &CalendarPage::onPrevMonth);
    connect(nextBtn_, &QPushButton::clicked, this, &CalendarPage::onNextMonth);

    // initial label
    const auto today = QDate::currentDate();
    monthLabel_->setText(QString("%1 %2").arg(monthNameRu(today.month())).arg(today.year()));
}

void CalendarPage::applyStyles()
{
    setStyleSheet(R"(
        QWidget { background: #f5f6f8; color: #111827; }
        QPushButton#backBtn {
            padding: 6px 10px;
            border-radius: 10px;
            border: 1px solid #e5e7eb;
            background: #ffffff;
        }
        QPushButton#backBtn:hover { background:#f3f4f6; }

        QLabel#title { font-size: 18px; font-weight: 800; }
        QLabel#monthLabel { font-size: 14px; font-weight: 700; padding: 0 8px; }

        QPushButton#navBtn {
            border-radius: 10px;
            border: 1px solid #e5e7eb;
            background: #ffffff;
        }
        QPushButton#navBtn:hover { background:#f3f4f6; }

        QCalendarWidget#calendar {
            background:#ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
        }

        QFrame#card {
            background:#ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
        }
        QLabel#legendTitle { font-weight: 800; }
        QLabel#details { color:#111827; }
    )");
}

void CalendarPage::refreshAll()
{
    paintCalendar();
    // show today details by default
    showDayDetails(QDate::currentDate());
}

int CalendarPage::computeQualityScore(int done, int correct, int distinct)
{
    // 0..100: объём + точность + разнообразие
    if (done <= 0) return 0;
    const double acc = done > 0 ? (double)correct / (double)done : 0.0;
    const double vol = qBound(0.0, (double)done / 20.0, 1.0);      // 20 задач = "полный" объём
    const double div = qBound(0.0, (double)distinct / 8.0, 1.0);   // 8 номеров = хорошее разнообразие
    const double score = 100.0 * (0.45*acc + 0.35*vol + 0.20*div);
    return qBound(0, (int)qRound(score), 100);
}

int CalendarPage::computeStreakDays(const QVector<DaySession>& sessions)
{
    QSet<QDate> active;
    for (const auto& s : sessions) if (s.doneCount > 0) active.insert(s.date);
    int streak = 0;
    QDate d = QDate::currentDate();
    while (active.contains(d)) { ++streak; d = d.addDays(-1); }
    return streak;
}

void CalendarPage::paintCalendar()
{
    if (!cal_) return;

    // update month label
    const int y = cal_->yearShown();
    const int m = cal_->monthShown();
    monthLabel_->setText(QString("%1 %2").arg(monthNameRu(m)).arg(y));

    // clear formats for current page range (42 cells)
    QDate first(y, m, 1);
    QDate start = first.addDays(- (first.dayOfWeek() - 1)); // monday
    for (int i=0;i<42;++i) {
        cal_->setDateTextFormat(start.addDays(i), QTextCharFormat());
    }

    // build quick maps
    QMap<QDate, DaySession> byDate;
    for (const auto& s : sessions_) byDate[s.date] = s;

    // planned days from plan.json (if exists)
    QSet<QDate> planned;
    if (!plan_.isEmpty()) {
        const QJsonArray days = plan_.value("days").toArray();
        for (const auto& dv : days) {
            const QJsonObject o = dv.toObject();
            const QDate d = QDate::fromString(o.value("date").toString(), Qt::ISODate);
            if (d.isValid() && o.value("planned").toBool(true)) planned.insert(d);
        }
    } else if (!plannedWeekdays_.isEmpty()) {
        // fallback: plan by weekdays for next 60 days
        QDate d = QDate::currentDate().addDays(-30);
        for (int i=0;i<90;++i) {
            if (plannedWeekdays_.contains(d.dayOfWeek())) planned.insert(d);
            d = d.addDays(1);
        }
    }

    // apply formats
    for (int i=0;i<42;++i) {
        const QDate d = start.addDays(i);
        QTextCharFormat fmt;

        const bool isPlanned = planned.contains(d);
        const bool hasSession = byDate.contains(d) && byDate[d].doneCount > 0;
        const bool isPastOrToday = d <= QDate::currentDate();

        if (isPlanned) {
            fmt.setBackground(QColor("#fde68a")); // planned yellow
        }
        if (hasSession) {
            fmt.setBackground(QColor("#86efac")); // green
        }
        if (isPlanned && isPastOrToday && (!hasSession)) {
            fmt.setBackground(QColor("#fca5a5")); // red missed
        }

        // mock highlight with border
        if (byDate.contains(d) && byDate[d].type == "mock") {
            fmt.setFontWeight(QFont::Bold);
            fmt.setForeground(QBrush(QColor("#1d4ed8")));
            fmt.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        }

        // mini-strip idea: add dot via foreground? QCalendarWidget doesn't support per-cell custom painting easily
        // We'll emulate by making font slightly bolder for high quality days
        if (byDate.contains(d) && byDate[d].doneCount > 0) {
            const auto& s = byDate[d];
            QSet<int> distinct;
            for (auto it = s.byTask.begin(); it != s.byTask.end(); ++it) if (it.value().first>0) distinct.insert(it.key());
            const int q = computeQualityScore(s.doneCount, s.correctCount, distinct.size());
            if (q >= 70) fmt.setFontWeight(QFont::Bold);
        }

        // tooltip
        if (byDate.contains(d) || planned.contains(d)) {
            QString tt;
            if (byDate.contains(d) && byDate[d].doneCount > 0) {
                const auto& s = byDate[d];
                QSet<int> distinct;
                for (auto it = s.byTask.begin(); it != s.byTask.end(); ++it) if (it.value().first>0) distinct.insert(it.key());
                const int q = computeQualityScore(s.doneCount, s.correctCount, distinct.size());
                tt = QString::fromUtf8("%1\nЗадач: %2, верно: %3\nРазных номеров: %4\nКачество дня: %5/100")
                        .arg(d.toString("dd.MM.yyyy"))
                        .arg(s.doneCount).arg(s.correctCount).arg(distinct.size()).arg(q);
                if (s.type == "mock" && s.mockScore >= 0) {
                    tt += QString::fromUtf8("\nПробник: %1/%2").arg(s.mockScore).arg(s.mockMax);
                }
            } else if (planned.contains(d)) {
                tt = QString::fromUtf8("%1\nПлан: занятие").arg(d.toString("dd.MM.yyyy"));
            }
            fmt.setToolTip(tt);
        }

        cal_->setDateTextFormat(d, fmt);
    }
}

void CalendarPage::showDayDetails(const QDate& d)
{
    if (!details_) return;

    // find session for this day
    const DaySession* s = nullptr;
    for (const auto& x : sessions_) if (x.date == d) { s = &x; break; }

    const int streak = computeStreakDays(sessions_);

    QString text;
    text += QString::fromUtf8("<b>%1</b><br>").arg(d.toString("dd.MM.yyyy"));
    text += QString::fromUtf8("🔥 Серия: <b>%1</b> дн.<br><br>").arg(streak);

    if (s && s->doneCount > 0) {
        QSet<int> distinct;
        for (auto it = s->byTask.begin(); it != s->byTask.end(); ++it) if (it.value().first>0) distinct.insert(it.key());
        const int quality = computeQualityScore(s->doneCount, s->correctCount, distinct.size());

        text += QString::fromUtf8("Тип: <b>%1</b><br>").arg(s->type == "mock" ? "Пробник" : "Тренировка");
        if (s->type == "mock" && s->mockScore >= 0) {
            text += QString::fromUtf8("Пробник: <b>%1/%2</b><br>").arg(s->mockScore).arg(s->mockMax);
        }
        text += QString::fromUtf8("Решено: <b>%1</b>, верно: <b>%2</b><br>")
                    .arg(s->doneCount).arg(s->correctCount);
        text += QString::fromUtf8("Разных номеров: <b>%1</b><br>").arg(distinct.size());
        text += QString::fromUtf8("Индекс качества дня: <b>%1/100</b><br>").arg(quality);

        // why counts may differ:
        text += QString::fromUtf8("<br><i>Почему «решено» и «верно» могут не совпадать?</i><br>"
                                  "— «решено» = попытки (включая ошибки и частичные).<br>"
                                  "— «верно» = только полностью верные ответы.<br>");

        // short list of top tasks
        text += QString::fromUtf8("<br><b>Кратко по номерам:</b><br>");
        int shown = 0;
        for (auto it = s->byTask.begin(); it != s->byTask.end() && shown < 8; ++it) {
            if (it.value().first <= 0) continue;
            const int taskNo = it.key();
            QString title = tasks_.contains(taskNo) ? tasks_[taskNo].title : QString::fromUtf8("—");
            text += QString::fromUtf8("• №%1 %2 — %3 задач, %4 верно<br>")
                        .arg(taskNo)
                        .arg(title)
                        .arg(it.value().first)
                        .arg(it.value().second);
            ++shown;
        }
    } else {
        // planned info
        bool hasPlan = false;
        QStringList topics;
        if (!plan_.isEmpty()) {
            const QJsonArray days = plan_.value("days").toArray();
            for (const auto& dv : days) {
                const QJsonObject o = dv.toObject();
                const QDate pd = QDate::fromString(o.value("date").toString(), Qt::ISODate);
                if (pd != d) continue;
                hasPlan = o.value("planned").toBool(true);
                const QJsonArray t = o.value("topics").toArray();
                for (const auto& tv : t) topics << tv.toString();
                break;
            }
        }
        if (hasPlan) {
            text += QString::fromUtf8("План: <b>занятие</b><br>");
            if (!topics.isEmpty()) {
                text += QString::fromUtf8("Темы (вариации):<br>");
                for (const auto& t : topics) text += QString::fromUtf8("• %1<br>").arg(t);
            } else {
                text += QString::fromUtf8("Темы будут уточнены автоматически по слабым местам.<br>");
            }
        } else {
            text += QString::fromUtf8("Нет данных за этот день.<br>");
        }
    }

    details_->setText(text);
}

void CalendarPage::onPrevMonth()
{
    if (!cal_) return;
    cal_->showPreviousMonth();
    paintCalendar();
}
void CalendarPage::onNextMonth()
{
    if (!cal_) return;
    cal_->showNextMonth();
    paintCalendar();
}
void CalendarPage::onMonthChanged(int /*year*/, int /*month*/)
{
    paintCalendar();
}
void CalendarPage::onDayClicked(const QDate& d)
{
    showDayDetails(d);
}
