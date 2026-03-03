#include "examselectwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QFrame>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QString trUtf8(const char* s) { return QString::fromUtf8(s); }

ExamSelectWindow::ExamSelectWindow(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    applyStyles();
    showMainMenu();
}

QString ExamSelectWindow::dataPath(const QString& fileName) const
{
    // Без QCoreApplication/QApplication — только currentPath + подъёмы.
    const QString base = QDir::currentPath();
    const QStringList candidates = {
        base + "/data/" + fileName,
        base + "/../data/" + fileName,
        base + "/../../data/" + fileName,
        base + "/../../../data/" + fileName,
        base + "/" + fileName
    };
    for (const auto& p : candidates) {
        const QString clean = QDir::cleanPath(p);
        if (QFile::exists(clean)) return clean;
    }
    return QDir::cleanPath(base + "/data/" + fileName);
}

bool ExamSelectWindow::loadCatalogVariations(QString* err)
{
    catalogVars_.clear();

    const QString path = dataPath("catalog.json");
    QFile f(path);
    if (!f.exists()) {
        if (err) *err = "catalog.json not found: " + path;
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "cannot open: " + path;
        return false;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isNull() || !doc.isObject()) {
        if (err) *err = "invalid JSON: " + path;
        return false;
    }

    const auto root = doc.object();
    const auto tasksObj = root.value("tasks").toObject();

    for (auto it = tasksObj.begin(); it != tasksObj.end(); ++it) {
        const int taskNo = it.key().toInt();
        const auto tObj = it.value().toObject();
        const auto varsObj = tObj.value("variations").toObject();

        QList<QPair<QString, QString>> list;
        for (auto vit = varsObj.begin(); vit != varsObj.end(); ++vit) {
            const QString varKey = vit.key(); // v1, v2, ...
            const auto vObj = vit.value().toObject();
            const QString display = vObj.value("displayName").toString(varKey);
            list.push_back({varKey, display});
        }
        // Сортируем по varKey v1..vN (чтобы было стабильно)
        std::sort(list.begin(), list.end(), [](auto a, auto b){ return a.first < b.first; });

        if (taskNo >= 1 && taskNo <= 25) catalogVars_[taskNo] = list;
    }

    // если вдруг нет — всё равно создадим пустые списки, чтобы UI не падал
    for (int t = 1; t <= 25; ++t)
        if (!catalogVars_.contains(t)) catalogVars_[t] = {};

    return true;
}

void ExamSelectWindow::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);
    root->addWidget(stack_, 1);

    // ---------------- pageMain_ ----------------
    pageMain_ = new QWidget(stack_);
    auto* mainLay = new QVBoxLayout(pageMain_);
    mainLay->setContentsMargins(24, 18, 24, 18);
    mainLay->setSpacing(14);

    auto* top = new QHBoxLayout();
    top->setSpacing(10);

    backBtnMain_ = new QPushButton(trUtf8("← Назад"), pageMain_);
    backBtnMain_->setObjectName("backBtn");
    backBtnMain_->setFixedHeight(34);
    backBtnMain_->setCursor(Qt::PointingHandCursor);
    connect(backBtnMain_, &QPushButton::clicked, this, &ExamSelectWindow::backRequested);

    auto* header = new QLabel(trUtf8("Пробный вариант ОГЭ"), pageMain_);
    header->setObjectName("pageTitle");

    top->addWidget(backBtnMain_, 0);
    top->addWidget(header, 1);
    top->addStretch(1);

    auto* sub = new QLabel(trUtf8("Выбери готовый вариант (1–36) или личный вариант."), pageMain_);
    sub->setObjectName("pageSubtitle");
    sub->setWordWrap(true);

    auto* gridWrap = new QWidget(pageMain_);
    auto* grid = new QGridLayout(gridWrap);
    grid->setContentsMargins(0,0,0,0);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);

    // Ready block
    auto* readyBlock = new QFrame(gridWrap);
    readyBlock->setObjectName("block");
    auto* rLay = new QVBoxLayout(readyBlock);
    rLay->setContentsMargins(18, 16, 18, 16);
    rLay->setSpacing(12);

    auto* readyTitle = new QLabel(trUtf8("Готовый вариант"), readyBlock);
    readyTitle->setObjectName("blockTitle");

    readyCombo_ = new QComboBox(readyBlock);
    readyCombo_->setObjectName("combo");
    for (int i = 1; i <= 36; ++i)
        readyCombo_->addItem(trUtf8("Вариант %1").arg(i), i);

    readyStartBtn_ = new QPushButton(trUtf8("Начать"), readyBlock);
    readyStartBtn_->setObjectName("primaryBtn");
    readyStartBtn_->setMinimumHeight(40);
    readyStartBtn_->setCursor(Qt::PointingHandCursor);
    connect(readyStartBtn_, &QPushButton::clicked, this, [this]{
        emit readyVariantChosen(readyCombo_->currentData().toInt());
    });

    rLay->addWidget(readyTitle);
    rLay->addWidget(readyCombo_);
    rLay->addStretch(1);
    rLay->addWidget(readyStartBtn_);

    // Personal block
    auto* personalBlock = new QFrame(gridWrap);
    personalBlock->setObjectName("block");
    auto* pLay = new QVBoxLayout(personalBlock);
    pLay->setContentsMargins(18, 16, 18, 16);
    pLay->setSpacing(12);

    auto* pTitle = new QLabel(trUtf8("Личный вариант"), personalBlock);
    pTitle->setObjectName("blockTitle");

    auto* hint = new QLabel(trUtf8(
                                "Авто — приложение выбирает слабые вариации.\n"
                                "Вручную — выбери вариацию для каждого номера (1–25)."
                                ), personalBlock);
    hint->setObjectName("hint");
    hint->setWordWrap(true);

    personalAutoBtn_ = new QPushButton(trUtf8("Составить автоматически"), personalBlock);
    personalAutoBtn_->setObjectName("primaryBtn");
    personalAutoBtn_->setMinimumHeight(40);
    personalAutoBtn_->setCursor(Qt::PointingHandCursor);
    connect(personalAutoBtn_, &QPushButton::clicked, this, &ExamSelectWindow::personalAutoChosen);

    personalManualBtn_ = new QPushButton(trUtf8("Выбрать вручную по вариациям"), personalBlock);
    personalManualBtn_->setObjectName("secondaryBtn");
    personalManualBtn_->setMinimumHeight(40);
    personalManualBtn_->setCursor(Qt::PointingHandCursor);
    connect(personalManualBtn_, &QPushButton::clicked, this, [this]{
        showManualSelect();
    });

    pLay->addWidget(pTitle);
    pLay->addWidget(hint);
    pLay->addStretch(1);
    pLay->addWidget(personalAutoBtn_);
    pLay->addWidget(personalManualBtn_);

    grid->addWidget(readyBlock, 0, 0);
    grid->addWidget(personalBlock, 0, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    mainLay->addLayout(top);
    mainLay->addWidget(sub);
    mainLay->addWidget(gridWrap, 1);

    stack_->addWidget(pageMain_);

    // ---------------- pageManual_ ----------------
    pageManual_ = new QWidget(stack_);
    auto* manLay = new QVBoxLayout(pageManual_);
    manLay->setContentsMargins(24, 18, 24, 18);
    manLay->setSpacing(12);

    auto* top2 = new QHBoxLayout();
    top2->setSpacing(10);

    backBtnManual_ = new QPushButton(trUtf8("← Назад"), pageManual_);
    backBtnManual_->setObjectName("backBtn");
    backBtnManual_->setFixedHeight(34);
    backBtnManual_->setCursor(Qt::PointingHandCursor);
    connect(backBtnManual_, &QPushButton::clicked, this, [this]{ showMainMenu(); });

    auto* header2 = new QLabel(trUtf8("Личный вариант — выбор вариаций"), pageManual_);
    header2->setObjectName("pageTitle");

    top2->addWidget(backBtnManual_, 0);
    top2->addWidget(header2, 1);
    top2->addStretch(1);

    auto* sub2 = new QLabel(trUtf8(
                                "Для каждого номера выбери вариацию. "
                                "Вариации берутся из data/catalog.json (displayName)."
                                ), pageManual_);
    sub2->setObjectName("pageSubtitle");
    sub2->setWordWrap(true);

    scroll_ = new QScrollArea(pageManual_);
    scroll_->setWidgetResizable(true);
    scroll_->setObjectName("scroll");
auto* scrollContent = new QWidget(scroll_);
    auto* form = new QFormLayout(scrollContent);
    form->setContentsMargins(14, 14, 14, 14);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);

    // Заполним комбобоксы позже (в showManualSelect), но создадим каркас сразу
    for (int t = 1; t <= 25; ++t) {
        auto* label = new QLabel(trUtf8("%1").arg(t), scrollContent);
        label->setObjectName("taskNo");

        auto* combo = new QComboBox(scrollContent);
        combo->setObjectName("combo");
        combo->setMinimumHeight(34);

        manualCombos_[t] = combo;

        auto* rowWrap = new QWidget(scrollContent);
        auto* rowLay = new QHBoxLayout(rowWrap);
        rowLay->setContentsMargins(0,0,0,0);
        rowLay->setSpacing(10);

        auto* name = new QLabel(trUtf8("—"), rowWrap);
        name->setObjectName("taskTitle");
        name->setMinimumWidth(320);

        rowLay->addWidget(name, 1);
        rowLay->addWidget(combo, 0);

        // важный трюк: хранить label темы в свойстве combo, чтобы обновлять текст
        combo->setProperty("titleLabel", QVariant::fromValue<void*>(name));

        form->addRow(label, rowWrap);
    }

    scroll_->setWidget(scrollContent);

    startManualBtn_ = new QPushButton(trUtf8("Собрать личный вариант"), pageManual_);
    startManualBtn_->setObjectName("primaryBtn");
    startManualBtn_->setMinimumHeight(44);
    startManualBtn_->setCursor(Qt::PointingHandCursor);

    connect(startManualBtn_, &QPushButton::clicked, this, [this]{
        QMap<int, QString> selection;
        for (int t = 1; t <= 25; ++t) {
            auto* c = manualCombos_.value(t, nullptr);
            if (!c) continue;
            selection[t] = c->currentData().toString(); // varKey
        }
        emit personalManualChosen(selection);
    });

    manLay->addLayout(top2);
    manLay->addWidget(sub2);
    manLay->addWidget(scroll_, 1);
    manLay->addWidget(startManualBtn_);

    stack_->addWidget(pageManual_);
}

void ExamSelectWindow::showMainMenu()
{
    stack_->setCurrentWidget(pageMain_);
}

void ExamSelectWindow::showManualSelect()
{
    // Подгружаем вариации из catalog.json
    QString err;
    loadCatalogVariations(&err);

    // Заполняем комбобоксы
    for (int t = 1; t <= 25; ++t) {
        auto* combo = manualCombos_.value(t, nullptr);
        if (!combo) continue;

        combo->blockSignals(true);
        combo->clear();

        const auto vars = catalogVars_.value(t);
        // если пусто — всё равно добавим заглушку
        if (vars.isEmpty()) {
            combo->addItem(trUtf8("v1"), "v1");
        } else {
            for (const auto& pr : vars) {
                combo->addItem(pr.second, pr.first); // display, key
            }
        }
        combo->blockSignals(false);

        // обновим текст темы (если есть в catalog)
        auto* titleLabel = reinterpret_cast<QLabel*>(combo->property("titleLabel").value<void*>());
        if (titleLabel) {
            // В твоём catalog.json title хранится в tasks[номер].title
            // Мы можем вытащить title ещё раз быстро:
            // (упрощённо: если не нашли — оставим "Номер t")
            titleLabel->setText(trUtf8("Номер %1").arg(t));
        }
    }

    stack_->setCurrentWidget(pageManual_);
}

void ExamSelectWindow::applyStyles()
{
    setStyleSheet(R"(
        QWidget { background:#f5f6f8; color:#111827; }

        QLabel#pageTitle { font-size:18px; font-weight:800; color:#111827; }
        QLabel#pageSubtitle { font-size:12px; color:#374151; }

        QPushButton#backBtn {
            padding: 6px 10px;
            border-radius: 10px;
            border: 1px solid #e5e7eb;
            background: #ffffff;
            color:#111827;
        }
        QPushButton#backBtn:hover { background:#f3f4f6; }

        QFrame#block {
            background:#ffffff;
            border:1px solid #e5e7eb;
            border-radius: 14px;
        }
        QLabel#blockTitle { font-size:14px; font-weight:800; color:#111827; }
        QLabel#hint { font-size:12px; color:#374151; }
        QLabel#taskNo { font-size:13px; font-weight:800; }
        QLabel#taskTitle { font-size:12px; color:#111827; }

        QComboBox#combo {
            padding: 6px 10px;
            border-radius: 10px;
            border: 1px solid #e5e7eb;
            background: #ffffff;
            color:#111827;
            min-width: 260px;
        }

        QScrollArea#scroll {
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            background: #ffffff;
        }

        QPushButton#primaryBtn {
            padding:10px 12px;
            border-radius: 12px;
            background:#111827;
            color:#ffffff;
            border:1px solid #111827;
        }
        QPushButton#primaryBtn:hover { background:#0b1220; }

        QPushButton#secondaryBtn {
            padding:10px 12px;
            border-radius: 12px;
            background:#ffffff;
            color:#111827;
            border:1px solid #e5e7eb;
        }
        QPushButton#secondaryBtn:hover { background:#f3f4f6; }
    )");
}
