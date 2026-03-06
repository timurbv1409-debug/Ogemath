#pragma once

#include <QMainWindow>

#include "examselectwindow.h"
#include "progresspage.h"
#include "starttrainingpage.h"
#include "blockbuilderpage.h"
#include "trainingsessionpage.h"
#include "trainingstateservice.h"

class QStackedWidget;
class QWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildUi();
    QWidget* buildHomePage();

    void showHome();
    void showExamSelect();
    void showProgress();
    void showStartTraining();
    void showBlockBuilder();
    void showTrainingSession();

private slots:
    void onReadyVariantChosen(int variantId);
    void onPersonalVariantChosen();

private:
    QStackedWidget* stack_ = nullptr;

    QWidget* homePage_ = nullptr;
    ExamSelectWindow* examSelectPage_ = nullptr;
    ProgressPage* progressPage_ = nullptr;
    StartTrainingPage* startTrainingPage_ = nullptr;
    BlockBuilderPage* blockBuilderPage_ = nullptr;
    TrainingSessionPage* trainingSessionPage_ = nullptr;
    TrainingStateService* trainingStateService_ = nullptr;
};
