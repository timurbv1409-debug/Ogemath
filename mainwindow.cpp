#include "mainwindow.h"
#include "examselectwindow.h"
#include "progresspage.h"
#include "starttrainingpage.h"
#include "blockbuilderpage.h"
#include "trainingstateservice.h"

#include <QStackedWidget>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSizePolicy>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QString::fromUtf8("ОГЭ Математика — адаптивный тренажёр"));
    resize(1100, 650);

    buildUi();
    showHome();
}

void MainWindow::buildUi()
{
    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);

    trainingStateService_ = new TrainingStateService(this);

    homePage_ = buildHomePage();

    examSelectPage_ = new ExamSelectWindow(stack_);
    progressPage_   = new ProgressPage(stack_);
    startTrainingPage_ = new StartTrainingPage(trainingStateService_, stack_);
    blockBuilderPage_ = new BlockBuilderPage(trainingStateService_, stack_);

    connect(examSelectPage_, &ExamSelectWindow::backRequested, this, [this]{ showHome(); });
    connect(examSelectPage_, &ExamSelectWindow::readyVariantChosen, this, [this](int id){ onReadyVariantChosen(id); });
    connect(examSelectPage_, &ExamSelectWindow::backRequested, this, [this]{ showHome(); });

    connect(examSelectPage_, &ExamSelectWindow::readyVariantChosen, this,
            [this](int id){ onReadyVariantChosen(id); });

    connect(examSelectPage_, &ExamSelectWindow::personalAutoChosen, this,
            [this]{ onPersonalVariantChosen(); });

    connect(examSelectPage_, &ExamSelectWindow::personalManualChosen, this,
            [this](const QMap<int, QString>& selection){
                QStringList parts;
                for (auto it = selection.begin(); it != selection.end(); ++it)
                    parts << QString("%1:%2").arg(it.key()).arg(it.value());
                QMessageBox::information(this, "Личный вариант (ручной)", parts.join(", "));
                onPersonalVariantChosen();
            });
    connect(progressPage_, &ProgressPage::backRequested, this, [this]{ showHome(); });

    connect(startTrainingPage_, &StartTrainingPage::backRequested, this, [this]{ showHome(); });
    connect(startTrainingPage_, &StartTrainingPage::continueRequested, this, [this]{
        blockBuilderPage_->openContinue();
        showBlockBuilder();
    });
    connect(startTrainingPage_, &StartTrainingPage::plannedRequested, this, [this]{
        blockBuilderPage_->openPlanned();
        showBlockBuilder();
    });
    connect(startTrainingPage_, &StartTrainingPage::manualRequested, this, [this]{
        blockBuilderPage_->openManual();
        showBlockBuilder();
    });

    connect(blockBuilderPage_, &BlockBuilderPage::backRequested, this, [this]{
        startTrainingPage_->refreshState();
        stack_->setCurrentWidget(startTrainingPage_);
    });
    connect(blockBuilderPage_, &BlockBuilderPage::trainingStarted, this, [this](const QString& message){
        QMessageBox::information(this, QString::fromUtf8("Тренировка"), message);
        startTrainingPage_->refreshState();
        stack_->setCurrentWidget(startTrainingPage_);
    });

    stack_->addWidget(homePage_);
    stack_->addWidget(examSelectPage_);
    stack_->addWidget(progressPage_);
    stack_->addWidget(startTrainingPage_);
    stack_->addWidget(blockBuilderPage_);
}

QWidget* MainWindow::buildHomePage()
{
    auto* page = new QWidget(stack_);

    auto* title = new QLabel(QString::fromUtf8("ОГЭ по математике — адаптивный тренажёр"), page);
    title->setObjectName("title");

    auto* subtitle = new QLabel(QString::fromUtf8("Построй свою подготовку под себя"), page);
    subtitle->setObjectName("subtitle");

    auto* welcome = new QLabel(QString::fromUtf8("Привет!"), page);
    welcome->setObjectName("welcome");

    auto* plan = new QLabel(
        QString::fromUtf8("Сегодня по плану: Тренировка (10 задач)\n")
            + QString::fromUtf8("Последнее занятие: вчера\n")
            + QString::fromUtf8("Следующее напоминание: завтра в 19:00"),
        page
    );
    plan->setObjectName("plan");

    auto* card = new QWidget(page);
    card->setObjectName("card");

    auto* grid = new QGridLayout(card);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);
    grid->setContentsMargins(20, 20, 20, 20);

    auto makeTile = [&](const QString& t, const QString& d) {
        auto* b = new QPushButton(card);
        b->setObjectName("tileButton");
        b->setMinimumHeight(120);
        b->setCursor(Qt::PointingHandCursor);
        b->setText(t + "\n" + d);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        return b;
    };

    auto* btnTraining = makeTile(
        QString::fromUtf8("Начать тренировку"),
        QString::fromUtf8("Ежедневная подборка задач с учётом твоего уровня.")
    );
    auto* btnMockExam = makeTile(
        QString::fromUtf8("Пробный вариант ОГЭ"),
        QString::fromUtf8("Готовый (1–36) или личный вариант.")
    );
    auto* btnProgress = makeTile(
        QString::fromUtf8("Мой прогресс"),
        QString::fromUtf8("Статистика по номерам и вариациям.")
    );
    auto* btnSettings = makeTile(
        QString::fromUtf8("Настройки и график занятий"),
        QString::fromUtf8("Сколько раз в неделю и время уведомлений.")
    );

    grid->addWidget(btnTraining, 0, 0);
    grid->addWidget(btnMockExam, 0, 1);
    grid->addWidget(btnProgress, 1, 0);
    grid->addWidget(btnSettings, 1, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    connect(btnTraining, &QPushButton::clicked, this, [this]{ showStartTraining(); });
    connect(btnMockExam, &QPushButton::clicked, this, [this]{ showExamSelect(); });
    connect(btnProgress, &QPushButton::clicked, this, [this]{ showProgress(); });
    connect(btnSettings, &QPushButton::clicked, this, [this]{
        QMessageBox::information(this, "Настройки", "Позже сделаем страницу настроек.");
    });

    auto* footerLeft = new QLabel(QString::fromUtf8("© Физтех-лицей, 2026"), page);
    footerLeft->setObjectName("footer");
    auto* version = new QLabel(QString::fromUtf8("Версия 0.1 (демо)"), page);
    version->setObjectName("footer");

    auto* footerRow = new QHBoxLayout();
    footerRow->addWidget(footerLeft);
    footerRow->addStretch(1);
    footerRow->addWidget(version);

    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(30, 25, 30, 20);
    root->setSpacing(14);

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addSpacing(6);
    root->addWidget(welcome);
    root->addWidget(plan);
    root->addSpacing(10);
    root->addWidget(card, 1);
    root->addLayout(footerRow);

    page->setStyleSheet(R"(
        QWidget { background: #f5f6f8; }
        QLabel#title { font-size: 22px; font-weight: 700; color: #111827; }
        QLabel#subtitle { font-size: 13px; color: #4b5563; }
        QLabel#welcome { font-size: 16px; font-weight: 700; color: #111827; }
        QLabel#plan {
            font-size: 13px; color: #374151;
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            padding: 12px;
        }
        QWidget#card {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 16px;
        }
        QPushButton#tileButton {
            text-align: left;
            padding: 14px;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 14px;
            font-size: 14px;
            color: #111827;
        }
        QPushButton#tileButton:hover { background: #f3f4f6; }
        QPushButton#tileButton:pressed { background: #e5e7eb; }
        QLabel#footer { font-size: 12px; color: #6b7280; }
    )");

    return page;
}

void MainWindow::showHome()
{
    stack_->setCurrentWidget(homePage_);
}

void MainWindow::showExamSelect()
{
    stack_->setCurrentWidget(examSelectPage_);
}

void MainWindow::showProgress()
{
    progressPage_->reloadAllData();
    stack_->setCurrentWidget(progressPage_);
}

void MainWindow::showStartTraining()
{
    startTrainingPage_->refreshState();
    stack_->setCurrentWidget(startTrainingPage_);
}

void MainWindow::showBlockBuilder()
{
    stack_->setCurrentWidget(blockBuilderPage_);
}

void MainWindow::onReadyVariantChosen(int variantId)
{
    QMessageBox::information(this, "Выбор",
                             QString("Выбран готовый вариант %1.\nСледующий шаг: страница экзамена.").arg(variantId));
    showHome();
}

void MainWindow::onPersonalVariantChosen()
{
    QMessageBox::information(this, "Выбор",
                             "Выбран личный вариант.\nСледующий шаг: собрать вариант и открыть страницу экзамена.");
    showHome();
}
