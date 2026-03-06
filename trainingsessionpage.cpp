#include "trainingsessionpage.h"

#include <QApplication>
#include <QBuffer>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QUrl>
#include <QClipboard>

namespace {
static QString tr8(const char* s) { return QString::fromUtf8(s); }

class TrainingSessionPageImageWidget : public QWidget
{
public:
    explicit TrainingSessionPageImageWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);

        scroll_ = new QScrollArea(this);
        scroll_->setWidgetResizable(true);
        scroll_->setFrameShape(QFrame::NoFrame);

        auto* container = new QWidget(scroll_);
        auto* cLay = new QVBoxLayout(container);
        cLay->setContentsMargins(0, 0, 0, 0);
        cLay->setSpacing(8);

        statusLabel_ = new QLabel(tr8("Условие задачи загрузится здесь."), container);
        statusLabel_->setWordWrap(true);
        statusLabel_->setStyleSheet("font-size:12px; color:#6b7280;");

        imageLabel_ = new QLabel(container);
        imageLabel_->setAlignment(Qt::AlignCenter);
        imageLabel_->setMinimumHeight(360);
        imageLabel_->setText(tr8("Нет изображения"));
        imageLabel_->setStyleSheet("background:#f9fafb; border:1px solid #e5e7eb; border-radius:12px; padding:10px;");

        cLay->addWidget(statusLabel_);
        cLay->addWidget(imageLabel_, 1);
        cLay->addStretch(1);
        scroll_->setWidget(container);

        openBtn_ = new QPushButton(tr8("Открыть изображение в браузере"), this);
        openBtn_->setObjectName("secondaryBtn");
        openBtn_->setVisible(false);

        lay->addWidget(scroll_, 1);
        lay->addWidget(openBtn_, 0, Qt::AlignLeft);

        connect(openBtn_, &QPushButton::clicked, this, [this] {
            if (!currentUrl_.isEmpty()) QDesktopServices::openUrl(QUrl(currentUrl_));
        });
    }

    void loadUrl(const QString& url, QNetworkAccessManager* nam)
    {
        currentUrl_ = url;
        openBtn_->setVisible(!url.isEmpty());
        imageLabel_->setPixmap(QPixmap());
        imageLabel_->setText(tr8("Загрузка изображения..."));
        statusLabel_->setText(url.isEmpty() ? tr8("Ссылка на изображение не указана.") : tr8("Загружаем условие по ссылке из каталога."));

        if (url.isEmpty() || nam == nullptr) {
            imageLabel_->setText(tr8("Ссылка отсутствует"));
            return;
        }

        const QString requestedUrl = url;
        QNetworkReply* reply = nam->get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply, requestedUrl] {
            const QByteArray data = reply->readAll();
            const bool ok = (reply->error() == QNetworkReply::NoError);
            reply->deleteLater();

            if (requestedUrl != currentUrl_) return;

            if (!ok) {
                imageLabel_->setText(tr8("Не удалось загрузить изображение. Можно открыть ссылку в браузере."));
                statusLabel_->setText(tr8("Ошибка загрузки изображения."));
                return;
            }

            QPixmap pixmap;
            if (!pixmap.loadFromData(data)) {
                imageLabel_->setText(tr8("Файл получен, но изображение не распознано."));
                statusLabel_->setText(tr8("Проверь ссылку или формат файла."));
                return;
            }

            imageLabel_->setText(QString());
            imageLabel_->setPixmap(pixmap.scaledToWidth(760, Qt::SmoothTransformation));
            statusLabel_->setText(tr8("Условие задачи загружено."));
        });
    }

private:
    QScrollArea* scroll_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* imageLabel_ = nullptr;
    QPushButton* openBtn_ = nullptr;
    QString currentUrl_;
};
}

class TrainingSessionPage::RemoteImageWidget : public TrainingSessionPageImageWidget
{
public:
    using TrainingSessionPageImageWidget::TrainingSessionPageImageWidget;
};

TrainingSessionPage::TrainingSessionPage(TrainingStateService* service, QWidget* parent)
    : QWidget(parent)
    , service_(service)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    auto* top = new QHBoxLayout();
    top->setSpacing(10);

    auto* backBtn = new QPushButton(tr8("← К сборщику"), this);
    backBtn->setObjectName("backBtn");
    backBtn->setFixedHeight(34);
    connect(backBtn, &QPushButton::clicked, this, &TrainingSessionPage::backRequested);

    pageTitle_ = new QLabel(tr8("Тренировка"), this);
    pageTitle_->setObjectName("pageTitle");

    progressLabel_ = new QLabel(this);
    progressLabel_->setObjectName("pageSubtitle");

    auto* titleCol = new QVBoxLayout();
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(2);
    titleCol->addWidget(pageTitle_);
    titleCol->addWidget(progressLabel_);

    auto* finishBtn = new QPushButton(tr8("Завершить"), this);
    finishBtn->setObjectName("secondaryBtn");
    connect(finishBtn, &QPushButton::clicked, this, &TrainingSessionPage::backRequested);

    top->addWidget(backBtn, 0);
    top->addLayout(titleCol, 1);
    top->addWidget(finishBtn, 0);

    auto* content = new QHBoxLayout();
    content->setSpacing(14);

    auto* leftCard = new QWidget(this);
    leftCard->setObjectName("card");
    auto* leftLay = new QVBoxLayout(leftCard);
    leftLay->setContentsMargins(16, 16, 16, 16);
    leftLay->setSpacing(10);

    auto* leftTitle = new QLabel(tr8("Задачи тренировки"), leftCard);
    leftTitle->setObjectName("sectionTitle");

    emptyLabel_ = new QLabel(tr8("После старта здесь появится список задач."), leftCard);
    emptyLabel_->setObjectName("emptyHint");
    emptyLabel_->setWordWrap(true);

    tasksList_ = new QListWidget(leftCard);
    tasksList_->setObjectName("tasksList");
    connect(tasksList_, &QListWidget::currentRowChanged, this, [this](int row) {
        currentIndex_ = row;
        refreshTaskDetails();
    });

    leftLay->addWidget(leftTitle);
    leftLay->addWidget(emptyLabel_);
    leftLay->addWidget(tasksList_, 1);

    auto* rightCard = new QWidget(this);
    rightCard->setObjectName("card");
    auto* rightLay = new QVBoxLayout(rightCard);
    rightLay->setContentsMargins(16, 16, 16, 16);
    rightLay->setSpacing(10);

    taskTitleLabel_ = new QLabel(tr8("Выбери задачу"), rightCard);
    taskTitleLabel_->setObjectName("sectionTitle");

    taskInfoLabel_ = new QLabel(rightCard);
    taskInfoLabel_->setObjectName("infoLine");
    taskInfoLabel_->setWordWrap(true);

    taskStatusLabel_ = new QLabel(rightCard);
    taskStatusLabel_->setObjectName("infoBox");
    taskStatusLabel_->setWordWrap(true);

    imageWidget_ = new RemoteImageWidget(rightCard);

    answerStack_ = new QStackedWidget(rightCard);

    emptyAnswerPage_ = new QWidget(answerStack_);
    auto* emptyLay = new QVBoxLayout(emptyAnswerPage_);
    emptyLay->setContentsMargins(0, 0, 0, 0);
    auto* emptyAnswerLabel = new QLabel(tr8("Выбери задачу слева, чтобы увидеть действия."), emptyAnswerPage_);
    emptyAnswerLabel->setWordWrap(true);
    emptyAnswerLabel->setObjectName("infoBox");
    emptyLay->addWidget(emptyAnswerLabel);

    testAnswerPage_ = new QWidget(answerStack_);
    auto* testLay = new QVBoxLayout(testAnswerPage_);
    testLay->setContentsMargins(0, 0, 0, 0);
    testLay->setSpacing(8);
    auto* testTitle = new QLabel(tr8("Ответ для тестового задания"), testAnswerPage_);
    testTitle->setObjectName("sectionTitleSmall");
    auto* testRow = new QHBoxLayout();
    testAnswerEdit_ = new QLineEdit(testAnswerPage_);
    testAnswerEdit_->setPlaceholderText(tr8("Введи ответ и нажми «Сохранить ответ»"));
    auto* saveBtn = new QPushButton(tr8("Сохранить ответ"), testAnswerPage_);
    saveBtn->setObjectName("primaryBtn");
    connect(saveBtn, &QPushButton::clicked, this, &TrainingSessionPage::onSaveTestAnswer);
    testRow->addWidget(testAnswerEdit_, 1);
    testRow->addWidget(saveBtn, 0);
    testSavedLabel_ = new QLabel(testAnswerPage_);
    testSavedLabel_->setObjectName("infoLine");
    testSavedLabel_->setWordWrap(true);
    testLay->addWidget(testTitle);
    testLay->addLayout(testRow);
    testLay->addWidget(testSavedLabel_);

    writtenAnswerPage_ = new QWidget(answerStack_);
    auto* writtenLay = new QVBoxLayout(writtenAnswerPage_);
    writtenLay->setContentsMargins(0, 0, 0, 0);
    writtenLay->setSpacing(8);
    auto* writtenTitle = new QLabel(tr8("Отправка в Telegram-бота"), writtenAnswerPage_);
    writtenTitle->setObjectName("sectionTitleSmall");
    writtenCodeLabel_ = new QLabel(writtenAnswerPage_);
    writtenCodeLabel_->setObjectName("codeLabel");
    writtenHintLabel_ = new QLabel(writtenAnswerPage_);
    writtenHintLabel_->setWordWrap(true);
    writtenHintLabel_->setObjectName("infoBox");
    accountLabel_ = new QLabel(writtenAnswerPage_);
    accountLabel_->setWordWrap(true);
    accountLabel_->setObjectName("infoLine");

    auto* writtenButtons = new QHBoxLayout();
    auto* copyCodeBtn = new QPushButton(tr8("Скопировать код"), writtenAnswerPage_);
    copyCodeBtn->setObjectName("secondaryBtn");
    connect(copyCodeBtn, &QPushButton::clicked, this, &TrainingSessionPage::onCopyTaskCode);
    auto* copyMessageBtn = new QPushButton(tr8("Скопировать текст для бота"), writtenAnswerPage_);
    copyMessageBtn->setObjectName("secondaryBtn");
    connect(copyMessageBtn, &QPushButton::clicked, this, &TrainingSessionPage::onCopyTelegramMessage);
    auto* solvedBtn = new QPushButton(tr8("Отметить решённой"), writtenAnswerPage_);
    solvedBtn->setObjectName("primaryBtn");
    connect(solvedBtn, &QPushButton::clicked, this, &TrainingSessionPage::onMarkWrittenSolved);
    writtenButtons->addWidget(copyCodeBtn);
    writtenButtons->addWidget(copyMessageBtn);
    writtenButtons->addWidget(solvedBtn);
    writtenButtons->addStretch(1);

    writtenLay->addWidget(writtenTitle);
    writtenLay->addWidget(writtenCodeLabel_);
    writtenLay->addWidget(writtenHintLabel_);
    writtenLay->addWidget(accountLabel_);
    writtenLay->addLayout(writtenButtons);

    answerStack_->addWidget(emptyAnswerPage_);
    answerStack_->addWidget(testAnswerPage_);
    answerStack_->addWidget(writtenAnswerPage_);

    auto* navRow = new QHBoxLayout();
    prevBtn_ = new QPushButton(tr8("← Предыдущая"), rightCard);
    prevBtn_->setObjectName("secondaryBtn");
    connect(prevBtn_, &QPushButton::clicked, this, &TrainingSessionPage::onPrevClicked);
    nextBtn_ = new QPushButton(tr8("Следующая →"), rightCard);
    nextBtn_->setObjectName("secondaryBtn");
    connect(nextBtn_, &QPushButton::clicked, this, &TrainingSessionPage::onNextClicked);
    navRow->addWidget(prevBtn_);
    navRow->addWidget(nextBtn_);
    navRow->addStretch(1);

    rightLay->addWidget(taskTitleLabel_);
    rightLay->addWidget(taskInfoLabel_);
    rightLay->addWidget(taskStatusLabel_);
    rightLay->addWidget(imageWidget_, 1);
    rightLay->addWidget(answerStack_);
    rightLay->addLayout(navRow);

    content->addWidget(leftCard, 4);
    content->addWidget(rightCard, 8);

    root->addLayout(top);
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
        QLabel#sectionTitleSmall { font-size: 14px; font-weight: 800; color: #111827; }
        QLabel#emptyHint { font-size: 12px; color: #6b7280; }
        QLabel#infoLine { font-size: 12px; color: #374151; }
        QLabel#infoBox {
            font-size: 12px;
            color: #374151;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 10px;
        }
        QLabel#codeLabel {
            font-size: 18px;
            font-weight: 800;
            color: #1d4ed8;
            background: #eff6ff;
            border: 1px solid #bfdbfe;
            border-radius: 12px;
            padding: 10px;
        }
        QListWidget#tasksList {
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
            padding: 6px;
        }
        QListWidget#tasksList::item {
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            background: #ffffff;
            margin: 4px;
            padding: 10px;
        }
        QListWidget#tasksList::item:selected {
            background: #dbeafe;
            border: 1px solid #93c5fd;
            color: #111827;
        }
        QLineEdit {
            min-height: 38px;
            border: 1px solid #d1d5db;
            border-radius: 10px;
            padding: 0 10px;
            background: #ffffff;
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
        QPushButton#secondaryBtn {
            padding: 9px 14px;
            border-radius: 12px;
            border: 1px solid #d1d5db;
            background: #ffffff;
            font-weight: 700;
        }
        QPushButton#secondaryBtn:hover { background: #f9fafb; }
        QPushButton:disabled { color: #9ca3af; }
    )");

    refreshUi();
}

void TrainingSessionPage::openSession(const QVector<TrainingStateService::Block>& blocks,
                                      const QString& modeTitle,
                                      bool plannedMode)
{
    modeTitle_ = modeTitle;
    plannedMode_ = plannedMode;
    tasks_ = service_->buildSessionTasks(blocks);
    currentIndex_ = tasks_.isEmpty() ? -1 : 0;
    refreshUi();
    if (!tasks_.isEmpty()) tasksList_->setCurrentRow(0);
}

void TrainingSessionPage::onTaskSelectionChanged()
{
    refreshTaskDetails();
}

void TrainingSessionPage::onPrevClicked()
{
    setCurrentIndex(currentIndex_ - 1);
}

void TrainingSessionPage::onNextClicked()
{
    setCurrentIndex(currentIndex_ + 1);
}

void TrainingSessionPage::onSaveTestAnswer()
{
    if (currentIndex_ < 0 || currentIndex_ >= tasks_.size()) return;
    const auto& task = tasks_[currentIndex_];
    if (task.isWritten) return;

    const QString answer = testAnswerEdit_->text().trimmed();
    if (answer.isEmpty()) {
        QMessageBox::information(this, tr8("Сохранение ответа"), tr8("Сначала введи ответ."));
        return;
    }

    service_->saveTestAnswer(task.code, answer);
    testSavedLabel_->setText(tr8("Ответ сохранён: %1").arg(answer));
    refreshList();
}

void TrainingSessionPage::onCopyTaskCode()
{
    if (currentIndex_ < 0 || currentIndex_ >= tasks_.size()) return;
    QApplication::clipboard()->setText(tasks_[currentIndex_].code);
    QMessageBox::information(this, tr8("Код скопирован"), tr8("Код задачи скопирован в буфер обмена."));
}

void TrainingSessionPage::onCopyTelegramMessage()
{
    if (currentIndex_ < 0 || currentIndex_ >= tasks_.size()) return;
    const auto& task = tasks_[currentIndex_];
    const QString text = tr8("Введи код задачи из приложения (12 цифр), например: %1\n\n📌 Формат кода:\n• %2 - номер задания\n• %3 - вариант\n• %4 - номер в банке\n• %5 - ваш аккаунт")
        .arg(task.code)
        .arg(task.code.mid(0, 2))
        .arg(task.code.mid(2, 2))
        .arg(task.code.mid(4, 3))
        .arg(task.code.mid(7, 5));
    QApplication::clipboard()->setText(text);
    QMessageBox::information(this, tr8("Текст скопирован"), tr8("Текст для Telegram-бота скопирован в буфер обмена."));
}

void TrainingSessionPage::onMarkWrittenSolved()
{
    if (currentIndex_ < 0 || currentIndex_ >= tasks_.size()) return;
    auto& task = tasks_[currentIndex_];
    if (!task.isWritten) return;

    if (!service_->markWrittenTaskSolved(task)) {
        QMessageBox::warning(this, tr8("Ошибка"), tr8("Не удалось отметить задачу решённой."));
        return;
    }

    task.solved = true;
    task.hasAttempt = true;
    refreshUi();
    tasksList_->setCurrentRow(currentIndex_);
    QMessageBox::information(this, tr8("Готово"), tr8("Письменная задача отмечена как решённая."));
}

void TrainingSessionPage::refreshUi()
{
    pageTitle_->setText(modeTitle_.isEmpty() ? tr8("Тренировка") : modeTitle_);
    progressLabel_->setText(tasks_.isEmpty()
        ? tr8("В этой тренировке пока нет задач.")
        : tr8("Всего задач: %1").arg(tasks_.size()));

    emptyLabel_->setVisible(tasks_.isEmpty());
    tasksList_->setVisible(!tasks_.isEmpty());

    refreshList();
    refreshTaskDetails();
}

void TrainingSessionPage::refreshList()
{
    const int keepIndex = currentIndex_;
    tasksList_->clear();
    for (int i = 0; i < tasks_.size(); ++i) {
        tasksList_->addItem(itemTitle(i));
    }
    if (!tasks_.isEmpty() && keepIndex >= 0 && keepIndex < tasks_.size()) {
        tasksList_->setCurrentRow(keepIndex);
    }
}

void TrainingSessionPage::refreshTaskDetails()
{
    const bool valid = (currentIndex_ >= 0 && currentIndex_ < tasks_.size());
    prevBtn_->setEnabled(valid && currentIndex_ > 0);
    nextBtn_->setEnabled(valid && currentIndex_ >= 0 && currentIndex_ < tasks_.size() - 1);

    if (!valid) {
        taskTitleLabel_->setText(tr8("Выбери задачу"));
        taskInfoLabel_->setText(tr8("Слева появится список задач после старта тренировки."));
        taskStatusLabel_->setText(tr8("Статус задачи будет показан здесь."));
        answerStack_->setCurrentWidget(emptyAnswerPage_);
        imageWidget_->loadUrl(QString(), nullptr);
        return;
    }

    const auto& task = tasks_[currentIndex_];
    progressLabel_->setText(tr8("Задача %1 из %2").arg(currentIndex_ + 1).arg(tasks_.size()));
    taskTitleLabel_->setText(tr8("Задание №%1").arg(task.taskNo));
    taskInfoLabel_->setText(tr8("%1\nВариация: %2\nКод задачи: %3")
                            .arg(task.taskTitle)
                            .arg(task.variation)
                            .arg(task.code));
    taskStatusLabel_->setText(itemStatusText(task));

    static QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    imageWidget_->loadUrl(task.imageUrl, nam);

    if (task.isWritten) {
        answerStack_->setCurrentWidget(writtenAnswerPage_);
        writtenCodeLabel_->setText(task.code);
        writtenHintLabel_->setText(tr8("Введи код задачи из приложения (12 цифр), например: %1\n\n📌 Формат кода:\n• %2 - номер задания\n• %3 - вариант\n• %4 - номер в банке\n• %5 - ваш аккаунт")
                                 .arg(task.code)
                                 .arg(task.code.mid(0, 2))
                                 .arg(task.code.mid(2, 2))
                                 .arg(task.code.mid(4, 3))
                                 .arg(task.code.mid(7, 5)));
        const auto account = service_->accountInfo();
        accountLabel_->setText(account.isValid()
            ? tr8("Аккаунт для кода: %1\nЛогин: %2\nПароль: %3")
                .arg(account.accountNumber, account.login, account.password)
            : tr8("Файл account.json не найден или не заполнен. Для кода будет использован номер 01010."));
    } else {
        answerStack_->setCurrentWidget(testAnswerPage_);
        const QString saved = service_->loadSavedTestAnswer(task.code);
        testAnswerEdit_->setText(saved);
        testSavedLabel_->setText(saved.isEmpty()
            ? tr8("Ответ пока не сохранён.")
            : tr8("Сохранённый ответ: %1").arg(saved));
    }
}

void TrainingSessionPage::setCurrentIndex(int index)
{
    if (index < 0 || index >= tasks_.size()) return;
    currentIndex_ = index;
    tasksList_->setCurrentRow(index);
}

QString TrainingSessionPage::itemTitle(int index) const
{
    if (index < 0 || index >= tasks_.size()) return QString();
    const auto& task = tasks_[index];
    return tr8("%1. №%2 — %3\n%4")
        .arg(index + 1)
        .arg(task.taskNo)
        .arg(task.variation)
        .arg(itemStatusText(task));
}

QString TrainingSessionPage::itemStatusText(const TrainingStateService::SessionTask& task) const
{
    if (task.isWritten) {
        return task.solved
            ? tr8("Статус: письменная • решена")
            : (task.hasAttempt ? tr8("Статус: письменная • есть попытка") : tr8("Статус: письменная"));
    }

    const QString saved = service_->loadSavedTestAnswer(task.code);
    if (!saved.isEmpty()) return tr8("Статус: тестовая • ответ сохранён");
    if (task.solved) return tr8("Статус: тестовая • решена");
    return tr8("Статус: тестовая");
}
