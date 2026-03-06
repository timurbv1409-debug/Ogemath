#include "blockbuilderpage.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QComboBox>

namespace {
static QString tr8(const char* s) { return QString::fromUtf8(s); }

class AddBlockDialog final : public QDialog
{
public:
    AddBlockDialog(TrainingStateService* service, int currentTotal, QWidget* parent = nullptr)
        : QDialog(parent)
        , service_(service)
        , currentTotal_(currentTotal)
    {
        setWindowTitle(tr8("Добавить блок"));
        setModal(true);
        resize(460, 260);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(16, 16, 16, 16);
        lay->setSpacing(10);

        auto* title = new QLabel(tr8("Добавить блок"), this);
        title->setStyleSheet("font-size:18px; font-weight:800; color:#111827;");

        auto* subtitle = new QLabel(tr8("По умолчанию будут предложены ещё не решённые задания. Общий лимит на тренировку — 30 задач."), this);
        subtitle->setWordWrap(true);
        subtitle->setStyleSheet("font-size:12px; color:#4b5563;");

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);

        taskCombo_ = new QComboBox(this);
        taskCombo_->setMinimumHeight(34);
        for (int taskNo = 1; taskNo <= 25; ++taskNo) {
            const QString titleText = service_->taskTitle(taskNo).trimmed();
            const QString text = titleText.isEmpty()
                ? tr8("№%1").arg(taskNo)
                : tr8("№%1 — %2").arg(taskNo).arg(titleText);
            taskCombo_->addItem(text, taskNo);
        }

        variationCombo_ = new QComboBox(this);
        variationCombo_->setMinimumHeight(34);

        countSpin_ = new QSpinBox(this);
        countSpin_->setRange(0, 30);
        countSpin_->setMinimumHeight(34);

        statsLabel_ = new QLabel(this);
        statsLabel_->setWordWrap(true);
        statsLabel_->setStyleSheet("font-size:12px; color:#374151; background:#f9fafb; border:1px solid #e5e7eb; border-radius:10px; padding:8px;");

        limitLabel_ = new QLabel(this);
        limitLabel_->setWordWrap(true);
        limitLabel_->setStyleSheet("font-size:12px; color:#6b7280;");

        form->addRow(tr8("Номер"), taskCombo_);
        form->addRow(tr8("Вариация"), variationCombo_);
        form->addRow(tr8("Сколько задач взять"), countSpin_);

        lay->addWidget(title);
        lay->addWidget(subtitle);
        lay->addLayout(form);
        lay->addWidget(statsLabel_);
        lay->addWidget(limitLabel_);

        buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        okBtn_ = buttons_->button(QDialogButtonBox::Ok);
        okBtn_->setText(tr8("Добавить"));
        buttons_->button(QDialogButtonBox::Cancel)->setText(tr8("Отмена"));
        lay->addWidget(buttons_);

        connect(taskCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ rebuildVariations(); });
        connect(variationCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ updateState(); });
        connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

        rebuildVariations();
    }

    TrainingStateService::Block block() const
    {
        TrainingStateService::Block block;
        block.taskNo = taskCombo_->currentData().toInt();
        block.variation = variationCombo_->currentData().toString();
        block.count = countSpin_->value();
        return block;
    }

    int remaining() const { return remaining_; }
    int totalInVariation() const { return totalInVariation_; }
    int solvedInVariation() const { return solvedInVariation_; }
    bool allSolved() const { return remaining_ <= 0; }

private:
    void rebuildVariations()
    {
        variationCombo_->blockSignals(true);
        variationCombo_->clear();

        const int taskNo = taskCombo_->currentData().toInt();
        const auto variations = service_->variationsForTask(taskNo);
        for (const auto& info : variations)
            variationCombo_->addItem(info.name, info.name);

        variationCombo_->blockSignals(false);
        updateState();
    }

    void updateState()
    {
        const int taskNo = taskCombo_->currentData().toInt();
        const QString variation = variationCombo_->currentData().toString();
        const int allowed = qMax(0, 30 - currentTotal_);

        totalInVariation_ = service_->totalTasksInVariation(taskNo, variation);
        solvedInVariation_ = service_->solvedTasksInVariation(taskNo, variation);
        remaining_ = service_->remainingTasksInVariation(taskNo, variation);
        const int recommended = qMin(remaining_, allowed);

        countSpin_->setMaximum(qMax(0, allowed));
        countSpin_->setValue(recommended);

        if (variation.isEmpty()) {
            statsLabel_->setText(tr8("Нет доступных вариаций для выбранного номера."));
            okBtn_->setEnabled(false);
            limitLabel_->setText(tr8("Лимит 30 задач. Уже собрано: %1.").arg(currentTotal_));
            return;
        }

        if (remaining_ <= 0) {
            statsLabel_->setText(tr8("В этой вариации всё решено.\nВсего: %1 • Уже решено: %2 • Не решал: 0")
                                .arg(totalInVariation_)
                                .arg(solvedInVariation_));
            okBtn_->setEnabled(false);
        } else if (allowed <= 0) {
            statsLabel_->setText(tr8("Лимит 30 достигнут.\nВсего: %1 • Уже решено: %2 • Не решал: %3")
                                .arg(totalInVariation_)
                                .arg(solvedInVariation_)
                                .arg(remaining_));
            okBtn_->setEnabled(false);
        } else {
            statsLabel_->setText(tr8("Всего в вариации: %1 • Уже решено: %2 • Не решал: %3")
                                .arg(totalInVariation_)
                                .arg(solvedInVariation_)
                                .arg(remaining_));
            okBtn_->setEnabled(true);
        }

        if (recommended < remaining_ && allowed > 0) {
            limitLabel_->setText(tr8("Обрезано по лимиту: сейчас можно добавить не больше %1 задач.").arg(allowed));
        } else {
            limitLabel_->setText(tr8("Лимит 30 задач. Уже собрано: %1. Свободно: %2.").arg(currentTotal_).arg(allowed));
        }
    }

private:
    TrainingStateService* service_ = nullptr;
    int currentTotal_ = 0;

    QComboBox* taskCombo_ = nullptr;
    QComboBox* variationCombo_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    QLabel* limitLabel_ = nullptr;
    QDialogButtonBox* buttons_ = nullptr;
    QPushButton* okBtn_ = nullptr;

    int remaining_ = 0;
    int totalInVariation_ = 0;
    int solvedInVariation_ = 0;
};
}

BlockBuilderPage::BlockBuilderPage(TrainingStateService* service, QWidget* parent)
    : QWidget(parent)
    , service_(service)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    auto* top = new QHBoxLayout();
    top->setSpacing(10);

    auto* backBtn = new QPushButton(tr8("← Назад"), this);
    backBtn->setObjectName("backBtn");
    backBtn->setFixedHeight(34);
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, &BlockBuilderPage::backRequested);

    pageTitle_ = new QLabel(this);
    pageTitle_->setObjectName("pageTitle");

    top->addWidget(backBtn, 0);
    top->addWidget(pageTitle_, 1);
    top->addStretch(1);

    pageSubtitle_ = new QLabel(this);
    pageSubtitle_->setObjectName("pageSubtitle");
    pageSubtitle_->setWordWrap(true);

    auto* content = new QHBoxLayout();
    content->setSpacing(14);

    auto* leftCard = new QWidget(this);
    leftCard->setObjectName("card");
    auto* leftLay = new QVBoxLayout(leftCard);
    leftLay->setContentsMargins(16, 16, 16, 16);
    leftLay->setSpacing(10);

    auto* leftTitle = new QLabel(tr8("Блоки тренировки"), leftCard);
    leftTitle->setObjectName("sectionTitle");

    emptyLabel_ = new QLabel(tr8("Пока нет блоков. Нажми «＋ Добавить блок», чтобы собрать тренировку."), leftCard);
    emptyLabel_->setObjectName("emptyHint");
    emptyLabel_->setWordWrap(true);

    blocksList_ = new QListWidget(leftCard);
    blocksList_->setObjectName("blocksList");
    connect(blocksList_, &QListWidget::currentRowChanged, this, &BlockBuilderPage::onSelectionChanged);

    totalLabel_ = new QLabel(leftCard);
    totalLabel_->setObjectName("totalLabel");

    auto* leftButtons = new QHBoxLayout();
    addBtn_ = new QPushButton(tr8("＋ Добавить блок"), leftCard);
    addBtn_->setObjectName("secondaryBtn");
    addBtn_->setCursor(Qt::PointingHandCursor);
    connect(addBtn_, &QPushButton::clicked, this, &BlockBuilderPage::onAddBlock);

    removeBtn_ = new QPushButton(tr8("Удалить выбранный"), leftCard);
    removeBtn_->setObjectName("dangerBtn");
    removeBtn_->setCursor(Qt::PointingHandCursor);
    connect(removeBtn_, &QPushButton::clicked, this, &BlockBuilderPage::onRemoveSelected);

    leftButtons->addWidget(addBtn_);
    leftButtons->addWidget(removeBtn_);
    leftButtons->addStretch(1);

    startBtn_ = new QPushButton(leftCard);
    startBtn_->setObjectName("primaryBtn");
    startBtn_->setMinimumHeight(42);
    startBtn_->setCursor(Qt::PointingHandCursor);
    connect(startBtn_, &QPushButton::clicked, this, &BlockBuilderPage::onStartClicked);

    leftLay->addWidget(leftTitle);
    leftLay->addWidget(emptyLabel_);
    leftLay->addWidget(blocksList_, 1);
    leftLay->addWidget(totalLabel_);
    leftLay->addLayout(leftButtons);
    leftLay->addWidget(startBtn_);

    auto* rightCard = new QWidget(this);
    rightCard->setObjectName("card");
    auto* rightLay = new QVBoxLayout(rightCard);
    rightLay->setContentsMargins(16, 16, 16, 16);
    rightLay->setSpacing(10);

    infoTitle_ = new QLabel(tr8("Информация о блоке"), rightCard);
    infoTitle_->setObjectName("sectionTitle");

    infoTask_ = new QLabel(rightCard);
    infoTask_->setObjectName("infoLine");
    infoTask_->setWordWrap(true);

    infoVariation_ = new QLabel(rightCard);
    infoVariation_->setObjectName("infoLine");
    infoVariation_->setWordWrap(true);

    infoStats_ = new QLabel(rightCard);
    infoStats_->setObjectName("infoBox");
    infoStats_->setWordWrap(true);

    infoCount_ = new QLabel(rightCard);
    infoCount_->setObjectName("infoBox");
    infoCount_->setWordWrap(true);

    rightLay->addWidget(infoTitle_);
    rightLay->addWidget(infoTask_);
    rightLay->addWidget(infoVariation_);
    rightLay->addWidget(infoStats_);
    rightLay->addWidget(infoCount_);
    rightLay->addStretch(1);

    content->addWidget(leftCard, 7);
    content->addWidget(rightCard, 5);

    root->addLayout(top);
    root->addWidget(pageSubtitle_);
    root->addLayout(content, 1);

    setStyleSheet(R"(
        QWidget { background: #f5f6f8; color: #111827; }
        QWidget#card {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 16px;
        }
        QLabel#pageTitle { font-size: 20px; font-weight: 800; }
        QLabel#pageSubtitle { font-size: 13px; color: #4b5563; }
        QLabel#sectionTitle { font-size: 16px; font-weight: 800; color: #111827; }
        QLabel#emptyHint { font-size: 12px; color: #6b7280; }
        QLabel#totalLabel { font-size: 13px; font-weight: 700; color: #1f2937; }
        QLabel#infoLine { font-size: 13px; color: #374151; }
        QLabel#infoBox {
            font-size: 12px;
            color: #374151;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 10px;
        }
        QListWidget#blocksList {
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            padding: 6px;
        }
        QListWidget#blocksList::item {
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            background: #ffffff;
            margin: 4px;
            padding: 10px;
        }
        QListWidget#blocksList::item:selected {
            background: #dbeafe;
            border: 1px solid #93c5fd;
            color: #111827;
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
        }
        QPushButton#primaryBtn:hover { background: #1d4ed8; }
        QPushButton#primaryBtn:disabled { background: #cbd5e1; }
        QPushButton#secondaryBtn {
            padding: 9px 14px;
            border-radius: 12px;
            border: 1px solid #d1d5db;
            background: #ffffff;
            font-weight: 700;
        }
        QPushButton#secondaryBtn:hover { background: #f9fafb; }
        QPushButton#dangerBtn {
            padding: 9px 14px;
            border-radius: 12px;
            border: 1px solid #fecaca;
            background: #fff1f2;
            color: #b91c1c;
            font-weight: 700;
        }
        QPushButton#dangerBtn:hover { background: #ffe4e6; }
        QPushButton:disabled { color: #9ca3af; }
    )");

    refreshUi();
}

void BlockBuilderPage::openManual()
{
    setMode(Mode::ManualDraft, service_->loadManualDraftForToday());
}

void BlockBuilderPage::openContinue()
{
    setMode(Mode::ContinueDraft, service_->loadManualDraftForToday());
}

void BlockBuilderPage::openPlanned()
{
    setMode(Mode::PlannedPreview, service_->plannedInfoForToday().blocks);
}

void BlockBuilderPage::onAddBlock()
{
    const AddDialogResult result = runAddBlockDialog();
    if (!result.accepted) return;

    if (result.block.count <= 0) {
        QMessageBox::information(this, tr8("Добавление блока"), tr8("Лимит 30 достигнут или в вариации не осталось новых задач."));
        return;
    }

    if (service_->totalCount(blocks_) >= 30) {
        QMessageBox::information(this, tr8("Лимит"), tr8("Лимит 30 достигнут."));
        return;
    }

    const int allowed = qMax(0, 30 - service_->totalCount(blocks_));
    TrainingStateService::Block block = result.block;
    if (block.count > allowed) {
        block.count = allowed;
        QMessageBox::information(this, tr8("Лимит"), tr8("Обрезано до %1 (лимит 30).").arg(allowed));
    } else if (block.count > result.remaining && result.remaining > 0) {
        block.count = result.remaining;
    }

    if (block.count <= 0) {
        QMessageBox::information(this, tr8("Лимит"), tr8("Лимит 30 достигнут."));
        return;
    }

    blocks_.push_back(block);
    service_->saveManualDraftForToday(blocks_);
    refreshUi();
    blocksList_->setCurrentRow(blocksList_->count() - 1);
}

void BlockBuilderPage::onRemoveSelected()
{
    const int row = blocksList_->currentRow();
    if (row < 0 || row >= blocks_.size()) return;

    blocks_.removeAt(row);
    if (!isReadOnlyMode()) service_->saveManualDraftForToday(blocks_);
    refreshUi();

    if (!blocks_.isEmpty()) {
        const int nextRow = qMin(row, blocks_.size() - 1);
        blocksList_->setCurrentRow(nextRow);
    }
}

void BlockBuilderPage::onSelectionChanged()
{
    refreshDetails();
}

void BlockBuilderPage::onStartClicked()
{
    if (blocks_.isEmpty()) {
        QMessageBox::information(this, tr8("Пустая тренировка"), tr8("Сначала добавь хотя бы один блок."));
        return;
    }

    if (mode_ == Mode::PlannedPreview) {
        service_->markPlannedStartedToday();
        emit trainingStarted(blocks_, tr8("Плановая тренировка"), true);
    } else {
        service_->saveManualDraftForToday(blocks_);
        emit trainingStarted(blocks_, tr8("Ручная тренировка"), false);
    }
}

BlockBuilderPage::AddDialogResult BlockBuilderPage::runAddBlockDialog()
{
    AddDialogResult out;
    AddBlockDialog dialog(service_, service_->totalCount(blocks_), this);
    if (dialog.exec() != QDialog::Accepted) return out;

    out.accepted = true;
    out.block = dialog.block();
    out.remaining = dialog.remaining();
    out.totalInVariation = dialog.totalInVariation();
    out.solvedInVariation = dialog.solvedInVariation();
    out.allSolved = dialog.allSolved();
    return out;
}

void BlockBuilderPage::setMode(Mode mode, const QVector<TrainingStateService::Block>& blocks)
{
    mode_ = mode;
    blocks_ = blocks;
    refreshUi();
    if (!blocks_.isEmpty()) blocksList_->setCurrentRow(0);
}

void BlockBuilderPage::refreshUi()
{
    pageTitle_->setText(modeTitle());

    if (mode_ == Mode::PlannedPreview) {
        pageSubtitle_->setText(tr8("Блоки уже заполнены по сегодняшнему плану. Этот режим предназначен для просмотра перед стартом."));
        startBtn_->setText(tr8("Начать по плану"));
    } else {
        pageSubtitle_->setText(tr8("Собирай тренировку из нужных номеров и вариаций. При добавлении автоматически учитываются ещё не решённые задания и общий лимит 30 задач."));
        startBtn_->setText(tr8("Начать тренировку"));
    }

    addBtn_->setVisible(!isReadOnlyMode());
    removeBtn_->setVisible(!isReadOnlyMode());

    refreshList();
    refreshDetails();

    startBtn_->setEnabled(!blocks_.isEmpty());
    totalLabel_->setText(tr8("Всего: %1 / 30 задач").arg(service_->totalCount(blocks_)));
}

void BlockBuilderPage::refreshList()
{
    blocksList_->clear();
    for (const auto& block : blocks_) blocksList_->addItem(blockText(block));
    emptyLabel_->setVisible(blocks_.isEmpty());
    blocksList_->setVisible(!blocks_.isEmpty());
    removeBtn_->setEnabled(!isReadOnlyMode() && blocksList_->currentRow() >= 0);
}

void BlockBuilderPage::refreshDetails()
{
    const int row = blocksList_->currentRow();
    if (row < 0 || row >= blocks_.size()) {
        infoTask_->setText(tr8("Выбери блок слева, чтобы увидеть детали."));
        infoVariation_->setText(tr8(""));
        infoStats_->setText(tr8("Для ручной тренировки можно добавлять много блоков, пока суммарно не наберётся 30 задач."));
        infoCount_->setText(isReadOnlyMode()
                                ? tr8("В плановом режиме состав блоков не редактируется.")
                                : tr8("В ручном режиме черновик хранится только до конца текущего дня."));
        removeBtn_->setEnabled(false);
        return;
    }

    const auto& block = blocks_.at(row);
    const QString title = service_->taskTitle(block.taskNo);
    const int total = service_->totalTasksInVariation(block.taskNo, block.variation);
    const int solved = service_->solvedTasksInVariation(block.taskNo, block.variation);
    const int remaining = service_->remainingTasksInVariation(block.taskNo, block.variation);

    infoTask_->setText(title.isEmpty()
                           ? tr8("Номер: %1").arg(block.taskNo)
                           : tr8("Номер %1 — %2").arg(block.taskNo).arg(title));
    infoVariation_->setText(tr8("Вариация: %1").arg(block.variation));
    infoStats_->setText(tr8("Всего в вариации: %1\nУже решено: %2\nЕщё не решал: %3")
                        .arg(total)
                        .arg(solved)
                        .arg(remaining));
    infoCount_->setText(tr8("В тренировку будет взято: %1 задач").arg(block.count));
    removeBtn_->setEnabled(!isReadOnlyMode());
}

QString BlockBuilderPage::blockText(const TrainingStateService::Block& block) const
{
    const QString title = service_->taskTitle(block.taskNo);
    return title.isEmpty()
        ? tr8("№%1 — %2 • %3 задач").arg(block.taskNo).arg(block.variation).arg(block.count)
        : tr8("№%1 — %2 / %3 • %4 задач").arg(block.taskNo).arg(title).arg(block.variation).arg(block.count);
}

QString BlockBuilderPage::modeTitle() const
{
    switch (mode_) {
    case Mode::ManualDraft:
        return tr8("Сборщик блоков — ручная тренировка");
    case Mode::ContinueDraft:
        return tr8("Сборщик блоков — продолжить набор");
    case Mode::PlannedPreview:
        return tr8("Сборщик блоков — плановая тренировка");
    }
    return tr8("Сборщик блоков");
}

bool BlockBuilderPage::isReadOnlyMode() const
{
    return mode_ == Mode::PlannedPreview;
}
