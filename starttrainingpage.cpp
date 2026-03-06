#include "starttrainingpage.h"
#include "trainingstateservice.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
static QString tr8(const char* s) { return QString::fromUtf8(s); }
}

StartTrainingPage::StartTrainingPage(TrainingStateService* service, QWidget* parent)
    : QWidget(parent)
    , service_(service)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(26, 20, 26, 20);
    root->setSpacing(14);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    auto* backBtn = new QPushButton(tr8("← Назад"), this);
    backBtn->setObjectName("backBtn");
    backBtn->setFixedHeight(34);
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, &StartTrainingPage::backRequested);

    auto* title = new QLabel(tr8("Начать тренировку"), this);
    title->setObjectName("pageTitle");

    topRow->addWidget(backBtn, 0);
    topRow->addWidget(title, 1);
    topRow->addStretch(1);

    auto* subtitle = new QLabel(
        tr8("Выбери, как собрать сегодняшнюю тренировку. "
            "Плановая тренировка берётся из расписания, ручная — собирается по нужным номерам и вариациям."),
        this);
    subtitle->setObjectName("pageSubtitle");
    subtitle->setWordWrap(true);

    root->addLayout(topRow);
    root->addWidget(subtitle);

    root->addWidget(makeCard(
        tr8("Продолжить"),
        tr8("Открыть сохранённый набор блоков за сегодня. Черновик автоматически очищается в новый день."),
        continueBtn_, continueStatus_, tr8("Открыть")));

    root->addWidget(makeCard(
        tr8("Плановая тренировка"),
        tr8("Открыть блоки, уже собранные по сегодняшнему плану. "
            "Режим только для просмотра перед стартом: добавление новых блоков скрыто."),
        plannedBtn_, plannedStatus_, tr8("Открыть план")));

    root->addWidget(makeCard(
        tr8("Прорешать задания"),
        tr8("Собрать тренировку вручную. По умолчанию будут предлагаться ещё не решённые задания, общий лимит — 30 задач."),
        manualBtn_, manualStatus_, tr8("Собрать вручную")));

    auto* bottom = new QHBoxLayout();
    auto* hint = new QLabel(tr8("Плановая тренировка — фиксированный набор, ручная — свободная сборка до 30 задач."), this);
    hint->setObjectName("footerHint");
    hint->setWordWrap(true);
    bottom->addWidget(hint);
    bottom->addStretch(1);
    root->addLayout(bottom);

    connect(continueBtn_, &QPushButton::clicked, this, &StartTrainingPage::continueRequested);
    connect(plannedBtn_, &QPushButton::clicked, this, &StartTrainingPage::plannedRequested);
    connect(manualBtn_, &QPushButton::clicked, this, &StartTrainingPage::manualRequested);

    applyStyles();
    refreshState();
}

void StartTrainingPage::refreshState()
{
    continueStatus_->setText(service_->manualDraftStatusText());

    const TrainingStateService::PlannedInfo planned = service_->plannedInfoForToday();
    plannedStatus_->setText(service_->plannedStatusText());
    plannedBtn_->setEnabled(planned.available && !planned.alreadyStartedToday);

    manualStatus_->setText(tr8("Ручная сборка до 30 задач • Берутся ещё не решённые задания"));
}

QWidget* StartTrainingPage::makeCard(const QString& title,
                                     const QString& description,
                                     QPushButton*& buttonOut,
                                     QLabel*& statusOut,
                                     const QString& buttonText)
{
    auto* card = new QWidget(this);
    card->setObjectName("modeCard");

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(18, 18, 18, 18);
    lay->setSpacing(8);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("cardTitle");

    auto* descLabel = new QLabel(description, card);
    descLabel->setObjectName("cardText");
    descLabel->setWordWrap(true);

    statusOut = new QLabel(card);
    statusOut->setObjectName("cardStatus");
    statusOut->setWordWrap(true);

    buttonOut = new QPushButton(buttonText, card);
    buttonOut->setObjectName("primaryBtn");
    buttonOut->setMinimumHeight(40);
    buttonOut->setCursor(Qt::PointingHandCursor);

    lay->addWidget(titleLabel);
    lay->addWidget(descLabel);
    lay->addWidget(statusOut);
    lay->addSpacing(4);
    lay->addWidget(buttonOut, 0, Qt::AlignLeft);

    return card;
}

void StartTrainingPage::applyStyles()
{
    setStyleSheet(R"(
        QWidget { background: #f5f6f8; color: #111827; }
        QLabel#pageTitle { font-size: 20px; font-weight: 800; color: #111827; }
        QLabel#pageSubtitle { font-size: 13px; color: #4b5563; }
        QLabel#footerHint { font-size: 12px; color: #6b7280; }

        QWidget#modeCard {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 16px;
        }
        QLabel#cardTitle { font-size: 17px; font-weight: 800; color: #111827; }
        QLabel#cardText { font-size: 13px; color: #374151; }
        QLabel#cardStatus {
            font-size: 12px;
            color: #1f2937;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 10px;
        }

        QPushButton#backBtn {
            padding: 6px 10px;
            border-radius: 10px;
            border: 1px solid #e5e7eb;
            background: #ffffff;
        }
        QPushButton#backBtn:hover { background: #f9fafb; }

        QPushButton#primaryBtn {
            padding: 10px 16px;
            border-radius: 12px;
            border: none;
            background: #2563eb;
            color: white;
            font-size: 13px;
            font-weight: 700;
            min-width: 160px;
        }
        QPushButton#primaryBtn:hover { background: #1d4ed8; }
        QPushButton#primaryBtn:pressed { background: #1e40af; }
        QPushButton#primaryBtn:disabled {
            background: #cbd5e1;
            color: #ffffff;
        }
    )");
}
