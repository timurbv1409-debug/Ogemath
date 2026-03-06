#pragma once

#include <QMainWindow>

class QStackedWidget;
class QWidget;

class ExamSelectWindow;
class ProgressPage;
class StartTrainingPage;
class BlockBuilderPage;
class TrainingStateService;

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
    TrainingStateService* trainingStateService_ = nullptr;
};
