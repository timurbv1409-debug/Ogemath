#pragma once

#include <QVector>
#include <QWidget>

#include "trainingstateservice.h"

class QLabel;
class QListWidget;
class QPushButton;
class QLineEdit;
class QTextEdit;
class QStackedWidget;
class TrainingStateService;
class QNetworkAccessManager;

class TrainingSessionPage final : public QWidget
{
    Q_OBJECT
public:
    explicit TrainingSessionPage(TrainingStateService* service, QWidget* parent = nullptr);

    void openSession(const QVector<TrainingStateService::Block>& blocks,
                     const QString& modeTitle,
                     bool plannedMode);

signals:
    void backRequested();

private slots:
    void onTaskSelectionChanged();
    void onPrevClicked();
    void onNextClicked();
    void onSaveTestAnswer();
    void onCopyTaskCode();
    void onCopyTelegramMessage();
    void onMarkWrittenSolved();

private:
    class RemoteImageWidget;

    void refreshUi();
    void refreshList();
    void refreshTaskDetails();
    void setCurrentIndex(int index);
    QString itemTitle(int index) const;
    QString itemStatusText(const TrainingStateService::SessionTask& task) const;

private:
    TrainingStateService* service_ = nullptr;
    QVector<TrainingStateService::SessionTask> tasks_;
    QString modeTitle_;
    bool plannedMode_ = false;
    int currentIndex_ = -1;

    QLabel* pageTitle_ = nullptr;
    QLabel* progressLabel_ = nullptr;
    QLabel* taskMetaLabel_ = nullptr;
    QListWidget* tasksList_ = nullptr;
    QLabel* emptyLabel_ = nullptr;

    QLabel* taskTitleLabel_ = nullptr;
    QLabel* taskInfoLabel_ = nullptr;
    QLabel* taskStatusLabel_ = nullptr;
    RemoteImageWidget* imageWidget_ = nullptr;

    QStackedWidget* answerStack_ = nullptr;
    QWidget* emptyAnswerPage_ = nullptr;
    QWidget* testAnswerPage_ = nullptr;
    QWidget* writtenAnswerPage_ = nullptr;

    QLineEdit* testAnswerEdit_ = nullptr;
    QLabel* testSavedLabel_ = nullptr;

    QLabel* writtenCodeLabel_ = nullptr;
    QLabel* writtenHintLabel_ = nullptr;
    QLabel* accountLabel_ = nullptr;

    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
};
