#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class TrainingStateService;

class StartTrainingPage final : public QWidget
{
    Q_OBJECT
public:
    explicit StartTrainingPage(TrainingStateService* service, QWidget* parent = nullptr);

    void refreshState();

signals:
    void backRequested();
    void continueRequested();
    void plannedRequested();
    void manualRequested();

private:
    QWidget* makeCard(const QString& title,
                      const QString& description,
                      QPushButton*& buttonOut,
                      QLabel*& statusOut,
                      const QString& buttonText);
    void applyStyles();

private:
    TrainingStateService* service_ = nullptr;

    QPushButton* continueBtn_ = nullptr;
    QLabel* continueStatus_ = nullptr;

    QPushButton* plannedBtn_ = nullptr;
    QLabel* plannedStatus_ = nullptr;

    QPushButton* manualBtn_ = nullptr;
    QLabel* manualStatus_ = nullptr;
};
