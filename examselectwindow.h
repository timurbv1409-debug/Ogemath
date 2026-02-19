#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include <QStringList>

class QTabWidget;
class QListWidget;
class QPushButton;
class QLabel;
class QRadioButton;
class QStackedWidget;
class QScrollArea;
class QComboBox;

class ExamSelectWindow final : public QWidget {
    Q_OBJECT
public:
    explicit ExamSelectWindow(QWidget* parent = nullptr);

signals:
    void backRequested();
    void readyVariantChosen(int id);
    void personalVariantChosen();

private slots:
    void onStartReady();
    void onBuildPersonal();     // авто
    void onPersonalModeChanged();
    void onBuildManual();       // ручной

private:
    void buildUi();
    void applyStyles();

    // ===== catalog loader =====
    void loadCatalog();
    QStringList variationsForTask(int taskNo) const;

    // Header
    QPushButton* backBtn_ = nullptr;

    // Tabs
    QTabWidget* tabs_ = nullptr;

    // Ready
    QListWidget* variantsList_ = nullptr;
    QLabel* readyInfo_ = nullptr;
    QPushButton* startReadyBtn_ = nullptr;

    // Personal mode
    QRadioButton* rbAuto_ = nullptr;
    QRadioButton* rbManual_ = nullptr;
    QStackedWidget* personalStack_ = nullptr;

    QPushButton* buildAutoBtn_ = nullptr;

    // Manual builder
    QScrollArea* manualScroll_ = nullptr;
    QWidget* manualForm_ = nullptr;
    QVector<QComboBox*> manualCombos_;  // индекс = taskNo-1
    QPushButton* buildManualBtn_ = nullptr;

    // из catalog.json: taskNo -> список вариаций (строки)
    QMap<int, QStringList> catalogVariations_;
};
