#pragma once

#include <QVector>
#include <QWidget>

#include "trainingstateservice.h"

class QLabel;
class QListWidget;
class QPushButton;
class TrainingStateService;

class BlockBuilderPage final : public QWidget
{
    Q_OBJECT
public:
    enum class Mode {
        ManualDraft,
        ContinueDraft,
        PlannedPreview
    };

    explicit BlockBuilderPage(TrainingStateService* service, QWidget* parent = nullptr);

    void openManual();
    void openContinue();
    void openPlanned();

signals:
    void backRequested();
    void trainingStarted(const QString& message);

private slots:
    void onAddBlock();
    void onRemoveSelected();
    void onSelectionChanged();
    void onStartClicked();

private:
    struct AddDialogResult {
        bool accepted = false;
        TrainingStateService::Block block;
        int remaining = 0;
        int totalInVariation = 0;
        int solvedInVariation = 0;
        bool allSolved = false;
    };

    AddDialogResult runAddBlockDialog();

    void setMode(Mode mode, const QVector<TrainingStateService::Block>& blocks);
    void refreshUi();
    void refreshList();
    void refreshDetails();
    QString blockText(const TrainingStateService::Block& block) const;
    QString modeTitle() const;
    bool isReadOnlyMode() const;

private:
    TrainingStateService* service_ = nullptr;
    Mode mode_ = Mode::ManualDraft;
    QVector<TrainingStateService::Block> blocks_;

    QLabel* pageTitle_ = nullptr;
    QLabel* pageSubtitle_ = nullptr;
    QListWidget* blocksList_ = nullptr;
    QLabel* totalLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* removeBtn_ = nullptr;
    QPushButton* startBtn_ = nullptr;

    QLabel* infoTitle_ = nullptr;
    QLabel* infoTask_ = nullptr;
    QLabel* infoVariation_ = nullptr;
    QLabel* infoStats_ = nullptr;
    QLabel* infoCount_ = nullptr;
};
