#include "calendarpage.h"

#include <QCalendarWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTextStream>
#include <QToolTip>
#include <QCursor>
#include <QJsonArray>
#include <QJsonValue>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

// ---- Custom calendar with per-cell bars and borders ----
class PlanCalendarWidget final : public QCalendarWidget {
public:
    struct Viz {
        QColor bg;
        bool mockBorder = false;
        double barFill = 0.0;   // 0..1
        double barHeight = 0.0; // 0..1
        int streakBadge = 0;
        bool critical = false;
    };

    explicit PlanCalendarWidget(QWidget* parent=nullptr) : QCalendarWidget(parent) {}

    void setViz(const QMap<QDate, Viz>& v) { viz_ = v; refreshCells(); }

    void refreshCells() { updateCells(); }

protected:
    void paintCell(QPainter* painter, const QRect& rect, QDate date) const override {
        QCalendarWidget::paintCell(painter, rect, date);

        if (!viz_.contains(date)) return;
        const Viz& v = viz_[date];

        // background overlay (light)
        if (v.bg.isValid()) {
            painter->save();
            QColor c = v.bg;
            c.setAlpha(140);
            painter->fillRect(rect.adjusted(1,1,-1,-1), c);
            painter->restore();
        }

        // bottom bar (volume + accuracy)
        if (v.barFill > 0.0 || v.barHeight > 0.0) {
            painter->save();
            const int barMaxH = qMax(2, rect.height()/6);
            const int barH = qBound(2, (int)qRound(barMaxH * v.barHeight), barMaxH);
            const int barW = qBound(0, (int)qRound((rect.width()-4) * v.barFill), rect.width()-4);
            const QRect bar(rect.left()+2, rect.bottom()-barH-2, barW, barH);
            painter->fillRect(bar, QColor(0,0,0,60));
            painter->restore();
        }

        // mock border
        if (v.mockBorder) {
            painter->save();
            QPen pen(QColor("#1d4ed8"));
            pen.setWidth(2);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect.adjusted(1,1,-2,-2));
            painter->restore();
        }

        // critical day highlight (where streak may break)
        if (v.critical) {
            painter->save();
            QPen pen(QColor("#f97316"));
            pen.setWidth(2);
            pen.setStyle(Qt::DashLine);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect.adjusted(3,3,-4,-4), 6, 6);
            painter->restore();
        }

        // streak badge (usually on today)
        if (v.streakBadge > 0) {
            painter->save();
            const int r = qMax(16, rect.width()/3);
            QRect badge(rect.right()-r-4, rect.top()+4, r, 18);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(17, 24, 39, 200));
            painter->drawRoundedRect(badge, 9, 9);
            painter->setPen(QColor("#ffffff"));
            QFont f = painter->font();
            f.setPointSize(qMax(8, f.pointSize()-1));
            f.setBold(true);
            painter->setFont(f);
            painter->drawText(badge, Qt::AlignCenter, QString::fromUtf8("🔥 %1").arg(v.streakBadge));
            painter->restore();
        }
    }

private:
    QMap<QDate, Viz> viz_;
};



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
    
    // Recompute streak based on sessions (ending at last active day)
    {
        QSet<QDate> active;
        QDate lastActive;
        for (const DaySession& s : sessions_) {
            if (s.doneCount > 0) {
                active.insert(s.date);
                if (!lastActive.isValid() || s.date > lastActive) lastActive = s.date;
            }
        }
        int streak = 0;
        if (lastActive.isValid()) {
            QDate d = lastActive;
            while (active.contains(d)) { streak++; d = d.addDays(-1); }
        }
        if (legendStreak_) {
            if (lastActive.isValid() && lastActive != QDate::currentDate())
                legendStreak_->setText(QString::fromUtf8("🔥 %1 до %2").arg(streak).arg(lastActive.toString("dd.MM")));
            else
                legendStreak_->setText(QString::fromUtf8("🔥 %1").arg(streak));
        }
    }
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

    cal_ = new PlanCalendarWidget(this);
    cal_->setObjectName("calendar");
    cal_->setGridVisible(true);
    cal_->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    cal_->setNavigationBarVisible(false); // свой хедер
    connect(cal_, &QCalendarWidget::clicked, this, &CalendarPage::onDayClicked);
    connect(cal_, &QCalendarWidget::currentPageChanged, this, &CalendarPage::onMonthChanged);

    // Наведение: тултип по дню. Внутри QCalendarWidget сетка — это QTableView.
    calView_ = cal_->findChild<QTableView*>("qt_calendar_calendarview");
    if (calView_ && calView_->viewport()) {
        calView_->viewport()->setMouseTracking(true);
        calView_->viewport()->installEventFilter(this);
    }

    auto* right = new QFrame(this);
    right->setObjectName("card");
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(12, 12, 12, 12);
    rv->setSpacing(10);

    // Legend (chips)
    auto* legendTitle = new QLabel(this);
    legendTitle->setObjectName("legendTitle");
    legendTitle->setText(QString::fromUtf8("Легенда"));

    auto* legendRow = new QWidget(this);
    legendRow->setObjectName("legendRow");
    auto* legLay = new QHBoxLayout(legendRow);
    legLay->setContentsMargins(0,0,0,0);
    legLay->setSpacing(10);

    auto makeChip = [this](const QString& text, const QString& bg, const QString& fg = "#111827") {
        QLabel* l = new QLabel(text, this);
        l->setStyleSheet(QString("QLabel{padding:4px 10px;border-radius:10px;background:%1;color:%2;}").arg(bg, fg));
        return l;
    };

    legendPlanned_ = makeChip(QString::fromUtf8("План"), "#fde68a");
    legendDone_    = makeChip(QString::fromUtf8("Занимался"), "#86efac");
    legendMissed_  = makeChip(QString::fromUtf8("Пропуск"), "#fca5a5");
    legendFree_    = makeChip(QString::fromUtf8("Нет плана"), "#e5e7eb");
    legendMock_    = makeChip(QString::fromUtf8("Пробник"), "#dbeafe");
    legendStreak_  = makeChip(QString::fromUtf8("🔥 0"), "#fff7ed");

    legLay->addWidget(legendPlanned_);
    legLay->addWidget(legendDone_);
    legLay->addWidget(legendMissed_);
    legLay->addWidget(legendFree_);
    legLay->addWidget(legendMock_);
    legLay->addStretch(1);
    legLay->addWidget(legendStreak_);
legendPlanned_->setTextFormat(Qt::RichText);
    legendDone_->setTextFormat(Qt::RichText);
    legendMissed_->setTextFormat(Qt::RichText);
    legendMock_->setTextFormat(Qt::RichText);

    details_ = new QLabel(QString::fromUtf8("Нажми на день — покажу детали."), this);
    details_->setObjectName("details");
    details_->setWordWrap(true);
    details_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    rv->addWidget(legendTitle);
    rv->addWidget(legendRow);    rv->addSpacing(10);
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

bool CalendarPage::eventFilter(QObject* watched, QEvent* event)
{
    if (calView_ && watched == calView_->viewport()) {
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::ToolTip) {
            QPoint pos;
            QPoint global;
            if (event->type() == QEvent::ToolTip) {
                const auto* he = static_cast<QHelpEvent*>(event);
                pos = he->pos();
                global = he->globalPos();
            } else {
                // MouseMove
                const auto* me = static_cast<QMouseEvent*>(event);
                pos = me->pos();
                global = calView_->viewport()->mapToGlobal(pos);
            }

            const QDate d = dateAtCalendarPos(pos);
            if (d.isValid()) {
                if (d != lastHoverDate_) {
                    lastHoverDate_ = d;
                    QToolTip::showText(global, tooltipForDate(d), calView_);
                }
                return true;
            } else {
                lastHoverDate_ = QDate();
                QToolTip::hideText();
            }
        } else if (event->type() == QEvent::Leave) {
            lastHoverDate_ = QDate();
            QToolTip::hideText();
        }
    }
    return QWidget::eventFilter(watched, event);
}

QDate CalendarPage::dateAtCalendarPos(const QPoint& pos) const
{
    if (!calView_ || !cal_) return QDate();
    const QModelIndex idx = calView_->indexAt(pos);
    if (!idx.isValid() || !calView_->model()) return QDate();

    // На большинстве Qt сборок дата лежит в Qt::UserRole.
    QVariant v = calView_->model()->data(idx, Qt::UserRole);
    if (v.canConvert<QDate>()) {
        const QDate d = v.toDate();
        if (d.isValid()) return d;
    }

    // Фолбэк: если там только число дня.
    const QString dayStr = calView_->model()->data(idx, Qt::DisplayRole).toString();
    bool ok = false;
    const int day = dayStr.toInt(&ok);
    if (!ok || day < 1 || day > 31) return QDate();

    const int y = cal_->yearShown();
    const int m = cal_->monthShown();
    QDate guess(y, m, day);
    if (guess.isValid()) return guess;
    return QDate();
}

QString CalendarPage::tooltipForDate(const QDate& d) const
{
    // короткая сводка для наведения (факт или план)
    int done = 0, correct = 0, dur = 0, distinct = 0;
    bool isMock = false;
    int mockScore = -1, mockMax = 0;

    for (const auto& s : sessions_) {
        if (s.date != d) continue;
        done += s.doneCount;
        correct += s.correctCount;
        dur += s.durationMin;
        distinct += s.byTask.size();
        if (s.type == "mock") { isMock = true; mockScore = s.mockScore; mockMax = s.mockMax; }
    }

    QString line = d.toString("dd.MM.yyyy");

    if (done > 0) {
        const int acc = (int)qRound(100.0 * (double)correct / (double)done);
        line += QString::fromUtf8(" — %1 задач, %2% точность, %3 мин").arg(done).arg(acc).arg(dur);
        if (distinct > 0) line += QString::fromUtf8(", %1 номеров").arg(distinct);
        if (isMock && mockScore >= 0) line += QString::fromUtf8(" • пробник %1/%2").arg(mockScore).arg(mockMax);
        return line;
    }

    // plan fallback
    if (!plan_.isEmpty()) {
        const QJsonArray days = plan_.value("days").toArray();
        for (const auto& dv : days) {
            const QJsonObject o = dv.toObject();
            const QDate pd = QDate::fromString(o.value("date").toString(), Qt::ISODate);
            if (pd != d) continue;
            if (!o.value("planned").toBool(true)) break;
            const int total = o.value("totalTasks").toInt(0);
            const QString type = o.value("type").toString("training");
            line += QString::fromUtf8(" — план: %1 задач").arg(total);
            if (type == "mock") line += QString::fromUtf8(" • пробник");
            return line;
        }
    }

    line += QString::fromUtf8(" — нет занятий");
    return line;
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
    // не сбрасываем выбор пользователя на "сегодня" при обновлениях данных
    if (!selectedDate_.isValid()) selectedDate_ = QDate::currentDate();
    showDayDetails(selectedDate_);
}

int CalendarPage::riskFromMastery(int mastery)
{
    // простая и стабильная оценка риска из мастерства, 0..100
    return qBound(0, 100 - qBound(0, mastery, 100), 100);
}

CalendarPage::QualityParts CalendarPage::computeQualityParts(int done, int correct, int distinct, double focusWeak)
{
    QualityParts qp;
    if (done <= 0) {
        qp.score = 0;
        qp.explain = QString::fromUtf8("Нет данных — не было занятий.");
        return qp;
    }

    // target knobs (can be moved to settings later)
    const double targetSolved = 20.0;
    const double targetDistinct = 6.0;

    qp.volume = qBound(0.0, done / targetSolved, 1.0);
    qp.accuracy = done > 0 ? qBound(0.0, (double)correct / (double)done, 1.0) : 0.0;
    qp.diversity = qBound(0.0, distinct / targetDistinct, 1.0);
    qp.focusWeak = qBound(0.0, focusWeak, 1.0);

    const double score = 100.0 * (0.25*qp.volume + 0.35*qp.accuracy + 0.20*qp.diversity + 0.20*qp.focusWeak);
    qp.score = qBound(0, (int)qRound(score), 100);

    QStringList notes;
    if (qp.accuracy < 0.55) notes << QString::fromUtf8("низкая точность");
    if (qp.diversity < 0.45) notes << QString::fromUtf8("мало разнообразия");
    if (qp.volume < 0.55) notes << QString::fromUtf8("малый объём");
    if (qp.focusWeak > 0.70) notes << QString::fromUtf8("хороший фокус на слабых темах");

    if (notes.isEmpty()) {
        qp.explain = QString::fromUtf8("Сбалансированный день: и объём, и точность, и разнообразие на хорошем уровне.");
    } else {
        qp.explain = notes.join(QString::fromUtf8(", ")) + ".";
    }

    return qp;
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

    const int y = cal_->yearShown();
    const int m = cal_->monthShown();
    monthLabel_->setText(QString("%1 %2").arg(monthNameRu(m)).arg(y));

    rebuildVizMap();

    // apply tooltips (QCalendarWidget supports toolTip only via QTextCharFormat)
    QDate first(y, m, 1);
    QDate start = first.addDays(- (first.dayOfWeek() - 1)); // monday
    for (int i=0;i<42;++i) {
        const QDate d = start.addDays(i);
        QTextCharFormat fmt;
        if (viz_.contains(d) && !viz_[d].tooltip.isEmpty()) {
            fmt.setToolTip(viz_[d].tooltip);
        }
        cal_->setDateTextFormat(d, fmt);
    }

    // push viz to custom calendar
    if (auto* pc = static_cast<PlanCalendarWidget*>(cal_)) {
        QMap<QDate, PlanCalendarWidget::Viz> v;
        for (auto it = viz_.begin(); it != viz_.end(); ++it) {
            PlanCalendarWidget::Viz z;
            z.bg = it.value().bg;
            z.mockBorder = it.value().mockBorder;
            z.barFill = it.value().barFill;
            z.barHeight = it.value().barHeight;
            z.streakBadge = it.value().streakBadge;
            z.critical = it.value().critical;
            v.insert(it.key(), z);
        }
        pc->setViz(v);
    } else {
        cal_->update();
}
}




void CalendarPage::rebuildVizMap()
{
    viz_.clear();
    if (!cal_) return;

    const QDate today = QDate::currentDate();

    // index sessions by date
    QMap<QDate, DaySession> byDate;
    for (const auto& s : sessions_) byDate[s.date] = s;

    // index plan by date
    QMap<QDate, QJsonObject> planByDate;
    if (!plan_.isEmpty()) {
        const QJsonArray days = plan_.value("days").toArray();
        for (const auto& dv : days) {
            const QJsonObject o = dv.toObject();
            const QDate d = QDate::fromString(o.value("date").toString(), Qt::ISODate);
            if (d.isValid()) planByDate[d] = o;
        }
    }

    const int y = cal_->yearShown();
    const int m = cal_->monthShown();
    QDate first(y, m, 1);
    QDate start = first.addDays(- (first.dayOfWeek() - 1)); // monday

    const int targetSolved = 20; // можно вынести в settings/plan meta позже

    // streak badge: show on today
    const int streak = computeStreakDays(sessions_);

    // critical day: tomorrow planned while streak active and today done (so it can break)
    const QDate tomorrow = today.addDays(1);
    const bool tomorrowPlanned = planByDate.contains(tomorrow) && planByDate[tomorrow].value("planned").toBool(true);
    const bool todayDone = byDate.contains(today) && byDate[today].doneCount > 0;

    for (int i=0;i<42;++i) {
        const QDate d = start.addDays(i);

        const bool hasSession = byDate.contains(d) && byDate[d].doneCount > 0;
        const bool hasPlan = planByDate.contains(d) && planByDate[d].value("planned").toBool(true);

        CellViz cv;
        cv.bg = QColor(); // invalid by default
        cv.mockBorder = false;
        cv.streakBadge = 0;
        cv.critical = false;

        // status coloring
        if (hasPlan && d > today) {
            cv.bg = QColor("#fde68a"); // planned yellow
        } else if (hasSession) {
            cv.bg = QColor("#86efac"); // done green
        } else if (hasPlan && d <= today) {
            cv.bg = QColor("#fca5a5"); // missed red
        } else {
            // free / no plan
            cv.bg = QColor("#e5e7eb"); // light gray
            cv.bg.setAlpha(70);
        }

        // mock border
        if (hasSession && byDate[d].type == "mock") cv.mockBorder = true;
        if (!cv.mockBorder && hasPlan && planByDate[d].value("type").toString() == "mock") cv.mockBorder = true;

        // streak badge + critical day
        if (d == today && streak > 0) cv.streakBadge = streak;
        if (d == tomorrow && streak > 0 && tomorrowPlanned && todayDone) cv.critical = true;

        // bars + tooltip
        if (hasSession) {
            const auto& s = byDate[d];
            const double vol = qBound(0.0, (double)s.doneCount / (double)targetSolved, 1.0);
            const double acc = s.doneCount > 0 ? qBound(0.0, (double)s.correctCount / (double)s.doneCount, 1.0) : 0.0;
            cv.barFill = vol;
            cv.barHeight = acc;

            QSet<int> distinct;
            for (auto it = s.byTask.begin(); it != s.byTask.end(); ++it) if (it.value().first>0) distinct.insert(it.key());
            // focusWeak: average risk of touched topics (risk = 100 - mastery)
            double avgRisk = 0.0;
            int riskCnt = 0;
            for (auto it = s.byTask.begin(); it != s.byTask.end(); ++it) {
                if (it.value().first <= 0) continue;
                const int taskNo = it.key();
                if (!tasks_.contains(taskNo)) continue;
                const int mastery = tasks_[taskNo].mastery;
                avgRisk += riskFromMastery(mastery);
                ++riskCnt;
            }
            const double focusWeak = riskCnt > 0 ? qBound(0.0, (avgRisk / (double)riskCnt) / 80.0, 1.0) : 0.4;
            const auto qp = computeQualityParts(s.doneCount, s.correctCount, distinct.size(), focusWeak);

            cv.tooltip = QString::fromUtf8("%1\nЗадач: %2, верно: %3\nТочность: %4%\nРазных номеров: %5\nКачество дня: %6/100")
                    .arg(d.toString("dd.MM.yyyy"))
                    .arg(s.doneCount).arg(s.correctCount)
                    .arg((int)qRound(100.0*acc))
                    .arg(distinct.size())
                    .arg(qp.score);
            cv.tooltip += QString::fromUtf8("\n(объём %1%, разнообразие %2%, фокус %3%)")
                    .arg((int)qRound(qp.volume*100.0))
                    .arg((int)qRound(qp.diversity*100.0))
                    .arg((int)qRound(qp.focusWeak*100.0));
            if (s.type == "mock" && s.mockScore >= 0) {
                cv.tooltip += QString::fromUtf8("\nПробник: %1/%2").arg(s.mockScore).arg(s.mockMax);
            }
        } else if (hasPlan) {
            const QJsonObject po = planByDate[d];
            const int total = po.value("totalTasks").toInt(0);
            cv.barFill = qBound(0.0, (double)total / (double)targetSolved, 1.0);

            // expected accuracy as average of items if exists
            double expAcc = 0.0;
            int cnt = 0;
            const QJsonArray items = po.value("items").toArray();
            for (const auto& iv : items) {
                const QJsonObject io = iv.toObject();
                if (io.contains("expectedAccuracy")) { expAcc += io.value("expectedAccuracy").toDouble(); cnt++; }
            }
            cv.barHeight = cnt>0 ? qBound(0.0, expAcc/(double)cnt, 1.0) : 0.45;

            cv.tooltip = QString::fromUtf8("%1\nПлан: %2 задач").arg(d.toString("dd.MM.yyyy")).arg(total);
            int shown = 0;
            for (const auto& iv : items) {
                const QJsonObject io = iv.toObject();
                const int no = io.value("taskNo").toInt();
                const QString name = io.value("displayName").toString();
                const int c = io.value("count").toInt();
                if (no>0 && !name.isEmpty() && c>0) {
                    cv.tooltip += QString::fromUtf8("\n• №%1 %2 ×%3").arg(no).arg(name).arg(c);
                    if (++shown >= 4) break;
                }
            }
        }

        viz_.insert(d, cv);
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
        // focusWeak: average risk from mastery of touched topics
        double avgRisk = 0.0;
        int riskCnt = 0;
        for (auto it = s->byTask.begin(); it != s->byTask.end(); ++it) {
            if (it.value().first <= 0) continue;
            const int taskNo = it.key();
            if (!tasks_.contains(taskNo)) continue;
            avgRisk += riskFromMastery(tasks_[taskNo].mastery);
            ++riskCnt;
        }
        const double focusWeak = riskCnt > 0 ? qBound(0.0, (avgRisk/(double)riskCnt)/80.0, 1.0) : 0.4;
        const auto qp = computeQualityParts(s->doneCount, s->correctCount, distinct.size(), focusWeak);

        text += QString::fromUtf8("Тип: <b>%1</b><br>").arg(s->type == "mock" ? "Пробник" : "Тренировка");
        if (s->type == "mock" && s->mockScore >= 0) {
            text += QString::fromUtf8("Пробник: <b>%1/%2</b><br>").arg(s->mockScore).arg(s->mockMax);
        }
        text += QString::fromUtf8("Решено: <b>%1</b>, верно: <b>%2</b><br>")
                    .arg(s->doneCount).arg(s->correctCount);
        text += QString::fromUtf8("Разных номеров: <b>%1</b><br>").arg(distinct.size());
        text += QString::fromUtf8("Индекс качества дня: <b>%1/100</b><br>").arg(qp.score);
        text += QString::fromUtf8("<span style='color:#6b7280'>объём %1% • точность %2% • разнообразие %3% • фокус %4%</span><br>")
                    .arg((int)qRound(qp.volume*100.0))
                    .arg((int)qRound(qp.accuracy*100.0))
                    .arg((int)qRound(qp.diversity*100.0))
                    .arg((int)qRound(qp.focusWeak*100.0));
        text += QString::fromUtf8("<span style='color:#374151'>%1</span><br>").arg(qp.explain);

        // why counts may differ:
        text += QString::fromUtf8("<br><i>Почему «решено» и «верно» могут не совпадать?</i><br>"
                                  "— «решено» = попытки (включая ошибки и частичные).<br>"
                                  "— «верно» = только полностью верные ответы.<br>");

        // short list of tasks (sorted by worst accuracy for mock recommendations)
        struct Line { int no; int solved; int correct; double acc; };
        QVector<Line> lines;
        for (auto it = s->byTask.begin(); it != s->byTask.end(); ++it) {
            if (it.value().first <= 0) continue;
            Line l; l.no = it.key(); l.solved = it.value().first; l.correct = it.value().second;
            l.acc = l.solved > 0 ? (double)l.correct/(double)l.solved : 0.0;
            lines.push_back(l);
        }
        std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b){ return a.acc < b.acc; });

        text += QString::fromUtf8("<br><b>По номерам:</b><br>");
        int shown = 0;
        for (const auto& l : lines) {
            if (shown >= 10) break;
            QString title = tasks_.contains(l.no) ? tasks_[l.no].title : QString::fromUtf8("—");
            text += QString::fromUtf8("• №%1 %2 — %3 задач, %4 верно (<b>%5%</b>)<br>")
                    .arg(l.no).arg(title).arg(l.solved).arg(l.correct).arg((int)qRound(l.acc*100.0));
            ++shown;
        }

        // mock: show weaknesses + what next
        if (s->type == "mock") {
            text += QString::fromUtf8("<br><b>Слабые места пробника:</b><br>");
            int cnt = 0;
            for (const auto& l : lines) {
                if (cnt >= 3) break;
                QString title = tasks_.contains(l.no) ? tasks_[l.no].title : QString::fromUtf8("—");
                text += QString::fromUtf8("• №%1 %2 — точность %3%<br>")
                        .arg(l.no).arg(title).arg((int)qRound(l.acc*100.0));
                ++cnt;
            }
            text += QString::fromUtf8("<br><b>Что делать дальше:</b><br>");
            text += QString::fromUtf8("1) Возьми 2–3 самые слабые темы и реши по 3–5 задач на каждую.<br>");
            text += QString::fromUtf8("2) На следующий день повтори 1 слабую тему + 1 тему «давно не повторял».<br>");
            text += QString::fromUtf8("3) Через 5–7 дней сделай ещё один пробник и сравни слабые места.<br>");
        }
    } else {
        // planned (or missed) info
        QJsonObject po;
        if (!plan_.isEmpty()) {
            const QJsonArray days = plan_.value("days").toArray();
            for (const auto& dv : days) {
                const QJsonObject o = dv.toObject();
                const QDate pd = QDate::fromString(o.value("date").toString(), Qt::ISODate);
                if (pd == d) { po = o; break; }
            }
        }

        const bool hasPlan = !po.isEmpty() && po.value("planned").toBool(true);

        if (hasPlan) {
            const bool isPastOrToday = d <= QDate::currentDate();
            const QString type = po.value("type").toString("training");
            const int total = po.value("totalTasks").toInt(0);

            if (isPastOrToday) {
                text += QString::fromUtf8("План был, но занятий нет (пропуск).<br>");
            } else {
                text += QString::fromUtf8("План на день:<br>");
            }

            text += QString::fromUtf8("Тип: <b>%1</b><br>").arg(type == "mock" ? "Пробник" : "Тренировка");
            if (total > 0) text += QString::fromUtf8("Всего: <b>%1</b> задач<br><br>").arg(total);

            const QJsonArray items = po.value("items").toArray();
            if (!items.isEmpty()) {
                text += QString::fromUtf8("<b>Темы:</b><br>");
                for (const auto& iv : items) {
                    const QJsonObject io = iv.toObject();
                    const int no = io.value("taskNo").toInt();
                    const QString name = io.value("displayName").toString();
                    const int c = io.value("count").toInt();
                    const int risk = io.value("risk").toInt(-1);
                    const int mastery = io.value("mastery").toInt(-1);
                    const int daysSince = io.value("daysSince").toInt(-1);

                    QString why;
                    const QJsonArray rs = io.value("reasons").toArray();
                    for (const auto& rv : rs) {
                        const QJsonObject ro = rv.toObject();
                        const QString t = ro.value("text").toString();
                        if (!t.isEmpty()) why += (why.isEmpty() ? "" : "; ") + t;
                    }

                    QString line = QString::fromUtf8("• №%1 %2 — <b>%3</b> задач")
                            .arg(no)
                            .arg(name.isEmpty() ? QString::fromUtf8("—") : name)
                            .arg(c);

                    QStringList extra;
                    if (risk >= 0) extra << QString::fromUtf8("риск %1").arg(risk);
                    if (mastery >= 0) extra << QString::fromUtf8("мастерство %1").arg(mastery);
                    if (daysSince >= 0) extra << QString::fromUtf8("не повторял %1 дн.").arg(daysSince);
                    if (!extra.isEmpty()) line += QString::fromUtf8(" <span style='color:#6b7280'>(%1)</span>").arg(extra.join(", "));

                    text += line + "<br>";
                    if (!why.isEmpty()) {
                        text += QString::fromUtf8("<span style='color:#374151'>— потому что: %1</span><br>").arg(why);
                    }
                }
            } else {
                text += QString::fromUtf8("Темы будут уточнены автоматически по слабым местам.<br>");
            }

            // прогноз качества по плану (если поле есть) или считаем на лету
            int q = po.value("dayQuality").toInt(-1);
            QString ex = po.value("qualityExplain").toString();
            if (q < 0) {
                // estimate: diversity = count of items, expectedAcc = avg expectedAccuracy
                const QJsonArray items = po.value("items").toArray();
                int distinct = 0;
                double expAcc = 0.0;
                double avgRisk = 0.0;
                int cnt = 0;
                for (const auto& iv : items) {
                    const QJsonObject io = iv.toObject();
                    if (io.value("count").toInt(0) <= 0) continue;
                    ++distinct;
                    expAcc += io.value("expectedAccuracy").toDouble(0.45);
                    avgRisk += io.value("risk").toInt(60);
                    ++cnt;
                }
                const double focusWeak = cnt>0 ? qBound(0.0, (avgRisk/(double)cnt)/80.0, 1.0) : 0.4;
                const auto qp = computeQualityParts(total, (int)qRound(total * (cnt>0 ? expAcc/(double)cnt : 0.45)), distinct, focusWeak);
                q = qp.score;
                ex = qp.explain;
                text += QString::fromUtf8("<br>Индекс качества (прогноз): <b>%1/100</b><br>").arg(q);
                text += QString::fromUtf8("<span style='color:#6b7280'>объём %1% • точность ~%2% • разнообразие %3% • фокус %4%</span><br>")
                        .arg((int)qRound(qp.volume*100.0))
                        .arg((int)qRound(qp.accuracy*100.0))
                        .arg((int)qRound(qp.diversity*100.0))
                        .arg((int)qRound(qp.focusWeak*100.0));
                if (!ex.isEmpty()) text += QString::fromUtf8("<span style='color:#374151'>%1</span><br>").arg(ex);
            } else {
                text += QString::fromUtf8("<br>Индекс качества (прогноз): <b>%1/100</b><br>").arg(q);
                if (!ex.isEmpty()) text += QString::fromUtf8("<span style='color:#374151'>%1</span><br>").arg(ex);
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
    selectedDate_ = d;
    showDayDetails(d);
}
