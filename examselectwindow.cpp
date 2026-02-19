#include "examselectwindow.h"

#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QRadioButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QFormLayout>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>

static QString catalogPath()
{
    // <exe>/data/catalog.json
    const QString base = QCoreApplication::applicationDirPath();
    return base + "/data/catalog.json";
}

ExamSelectWindow::ExamSelectWindow(QWidget* parent)
    : QWidget(parent)
{
    loadCatalog();
    buildUi();
    applyStyles();
    onPersonalModeChanged();
}

void ExamSelectWindow::loadCatalog()
{
    catalogVariations_.clear();

    QFile f(catalogPath());
    if (!f.open(QIODevice::ReadOnly)) {
        // если catalog.json не найден — оставим заглушки
        return;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;

    const QJsonObject root = doc.object();
    const QJsonObject tasks = root.value("tasks").toObject();

    for (int taskNo = 1; taskNo <= 25; ++taskNo) {
        const QString key = QString::number(taskNo);
        const QJsonObject taskObj = tasks.value(key).toObject();
        const QJsonObject varsObj = taskObj.value("variations").toObject();

        QStringList vars;
        for (auto it = varsObj.begin(); it != varsObj.end(); ++it) {
            vars << it.key(); // название вариации (например "Неравенства")
        }

        vars.sort(Qt::CaseInsensitive);

        if (!vars.isEmpty())
            catalogVariations_[taskNo] = vars;
    }
}

QStringList ExamSelectWindow::variationsForTask(int taskNo) const
{
    if (catalogVariations_.contains(taskNo))
        return catalogVariations_.value(taskNo);

    // fallback, если нет catalog.json или нет записи
    return {"A", "B", "C"};
}

void ExamSelectWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    // Header
    auto* header = new QHBoxLayout();
    header->setSpacing(10);

    backBtn_ = new QPushButton(QString::fromUtf8("←"), this);
    backBtn_->setObjectName("backBtn");
    backBtn_->setCursor(Qt::PointingHandCursor);
    backBtn_->setFixedSize(36, 36);
    backBtn_->setToolTip(QString::fromUtf8("Назад"));
    connect(backBtn_, &QPushButton::clicked, this, &ExamSelectWindow::backRequested);

    auto* title = new QLabel(QString::fromUtf8("Пробный вариант ОГЭ"), this);
    title->setObjectName("pageTitle");

    header->addWidget(backBtn_, 0);
    header->addWidget(title, 1);
    header->addStretch(1);

    root->addLayout(header);

    tabs_ = new QTabWidget(this);

    // ===== READY TAB =====
    auto* readyTab = new QWidget(this);
    auto* readyLayout = new QHBoxLayout(readyTab);
    readyLayout->setSpacing(14);

    variantsList_ = new QListWidget(readyTab);
    variantsList_->setMinimumWidth(260);
    for (int i = 1; i <= 36; ++i)
        variantsList_->addItem(QString::fromUtf8("Вариант %1").arg(i));
    variantsList_->setCurrentRow(0);

    auto* rightBlock = new QWidget(readyTab);
    auto* rightLayout = new QVBoxLayout(rightBlock);
    rightLayout->setSpacing(10);

    readyInfo_ = new QLabel(QString::fromUtf8(
                                "Выберите готовый вариант (1–36).\n\n"
                                "Формат:\n"
                                "• тестовая часть\n"
                                "• 12 заданий\n"
                                "• проверка по ответам\n"
                                ), rightBlock);

    startReadyBtn_ = new QPushButton(QString::fromUtf8("Начать выбранный вариант"), rightBlock);
    startReadyBtn_->setMinimumHeight(42);
    startReadyBtn_->setCursor(Qt::PointingHandCursor);
    connect(startReadyBtn_, &QPushButton::clicked, this, &ExamSelectWindow::onStartReady);

    rightLayout->addWidget(readyInfo_);
    rightLayout->addStretch(1);
    rightLayout->addWidget(startReadyBtn_);

    readyLayout->addWidget(variantsList_);
    readyLayout->addWidget(rightBlock, 1);

    // ===== PERSONAL TAB =====
    auto* personalTab = new QWidget(this);
    auto* personalLayout = new QVBoxLayout(personalTab);
    personalLayout->setSpacing(12);

    auto* modeRow = new QHBoxLayout();
    modeRow->setSpacing(14);

    rbAuto_ = new QRadioButton(QString::fromUtf8("Вариант составлен компьютером"), personalTab);
    rbManual_ = new QRadioButton(QString::fromUtf8("Собрать вручную (вариации 1–25)"), personalTab);
    rbAuto_->setChecked(true);

    modeRow->addWidget(rbAuto_);
    modeRow->addWidget(rbManual_);
    modeRow->addStretch(1);

    personalLayout->addLayout(modeRow);

    auto* modeGroup = new QButtonGroup(personalTab);
    modeGroup->addButton(rbAuto_);
    modeGroup->addButton(rbManual_);
    connect(rbAuto_, &QRadioButton::toggled, this, &ExamSelectWindow::onPersonalModeChanged);
    connect(rbManual_, &QRadioButton::toggled, this, &ExamSelectWindow::onPersonalModeChanged);

    personalStack_ = new QStackedWidget(personalTab);

    // --- Auto page ---
    auto* autoPage = new QWidget(personalStack_);
    auto* autoLayout = new QVBoxLayout(autoPage);
    autoLayout->setSpacing(12);

    auto* autoInfo = new QLabel(QString::fromUtf8(
                                    "Авто-вариант формируется по прогрессу:\n"
                                    "• больше задач по слабым темам;\n"
                                    "• часть задач для закрепления;\n"
                                    "• итог — личный вариант.\n\n"
                                    "Нажми кнопку, чтобы собрать."
                                    ), autoPage);

    buildAutoBtn_ = new QPushButton(QString::fromUtf8("Собрать авто-вариант"), autoPage);
    buildAutoBtn_->setMinimumHeight(42);
    buildAutoBtn_->setCursor(Qt::PointingHandCursor);
    connect(buildAutoBtn_, &QPushButton::clicked, this, &ExamSelectWindow::onBuildPersonal);

    autoLayout->addWidget(autoInfo);
    autoLayout->addStretch(1);
    autoLayout->addWidget(buildAutoBtn_);

    // --- Manual page ---
    auto* manualPage = new QWidget(personalStack_);
    auto* manualPageLayout = new QVBoxLayout(manualPage);
    manualPageLayout->setSpacing(10);

    auto* manualInfo = new QLabel(QString::fromUtf8(
                                      "Ручная сборка: выбери вариацию (подвид) для каждого номера 1–25.\n"
                                      "Список берётся из data/catalog.json."
                                      ), manualPage);

    manualScroll_ = new QScrollArea(manualPage);
    manualScroll_->setWidgetResizable(true);
    manualScroll_->setObjectName("manualScroll");

    manualForm_ = new QWidget(manualScroll_);
    auto* form = new QFormLayout(manualForm_);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    manualCombos_.clear();
    manualCombos_.reserve(25);

    for (int taskNo = 1; taskNo <= 25; ++taskNo) {
        auto* cb = new QComboBox(manualForm_);

        const QStringList vars = variationsForTask(taskNo);
        for (const QString& v : vars) cb->addItem(v);

        cb->setCurrentIndex(0);

        manualCombos_.push_back(cb);
        form->addRow(QString::fromUtf8("Задание %1:").arg(taskNo), cb);
    }

    manualScroll_->setWidget(manualForm_);

    buildManualBtn_ = new QPushButton(QString::fromUtf8("Собрать ручной вариант"), manualPage);
    buildManualBtn_->setMinimumHeight(42);
    buildManualBtn_->setCursor(Qt::PointingHandCursor);
    connect(buildManualBtn_, &QPushButton::clicked, this, &ExamSelectWindow::onBuildManual);

    manualPageLayout->addWidget(manualInfo);
    manualPageLayout->addWidget(manualScroll_, 1);
    manualPageLayout->addWidget(buildManualBtn_);

    personalStack_->addWidget(autoPage);
    personalStack_->addWidget(manualPage);

    personalLayout->addWidget(personalStack_, 1);

    tabs_->addTab(readyTab, QString::fromUtf8("Готовые варианты"));
    tabs_->addTab(personalTab, QString::fromUtf8("Личный вариант"));

    root->addWidget(tabs_, 1);
}

void ExamSelectWindow::applyStyles()
{
    setStyleSheet(R"(
        QWidget { background: #f5f6f8; color: #111827; font-size: 13px; }

        QLabel#pageTitle { font-size: 18px; font-weight: 700; color: #111827; }

        QPushButton#backBtn {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 18px;
            color: #111827;
            font-size: 16px;
        }
        QPushButton#backBtn:hover { background: #f3f4f6; }
        QPushButton#backBtn:pressed { background: #e5e7eb; }

        QTabWidget::pane {
            border: 1px solid #e5e7eb;
            background: #ffffff;
            border-radius: 12px;
            top: -1px;
        }

        QTabBar::tab {
            background: #f3f4f6;
            border: 1px solid #e5e7eb;
            padding: 10px 14px;
            margin-right: 6px;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            color: #111827;
            font-weight: 600;
        }
        QTabBar::tab:selected { background: #ffffff; border-bottom-color: #ffffff; }
        QTabBar::tab:hover { background: #eaeef6; }

        QListWidget {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 6px;
            color: #111827;
        }
        QListWidget::item { padding: 8px 10px; border-radius: 8px; }
        QListWidget::item:selected { background: #2563eb; color: white; }

        QScrollArea#manualScroll {
            background: transparent;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
        }

        QComboBox {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 6px 10px;
            min-height: 26px;
            color: #111827;
        }
        QComboBox::drop-down { border: none; width: 24px; }

        QPushButton {
            background: #2563eb;
            color: white;
            border: none;
            border-radius: 10px;
            padding: 10px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover { background: #1d4ed8; }
        QPushButton:pressed { background: #1e40af; }
    )");
}

void ExamSelectWindow::onStartReady()
{
    int row = variantsList_->currentRow();
    if (row < 0) return;
    emit readyVariantChosen(row + 1);
}

void ExamSelectWindow::onBuildPersonal()
{
    emit personalVariantChosen();
}

void ExamSelectWindow::onPersonalModeChanged()
{
    if (!personalStack_) return;
    personalStack_->setCurrentIndex((rbAuto_ && rbAuto_->isChecked()) ? 0 : 1);
}

void ExamSelectWindow::onBuildManual()
{
    // пока просто сигнал; дальше сделаем передачу выбранных вариаций
    emit personalVariantChosen();
}
