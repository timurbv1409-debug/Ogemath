#pragma once
#include <QWidget>
#include <QMap>

class QPushButton;
class QLabel;
class QComboBox;
class QStackedWidget;
class QScrollArea;

class ExamSelectWindow final : public QWidget
{
    Q_OBJECT
public:
    explicit ExamSelectWindow(QWidget* parent = nullptr);

signals:
    void backRequested();

    // Готовый вариант (1..36)
    void readyVariantChosen(int variantId);

    // Личный вариант: авто (подберём по слабым)
    void personalAutoChosen();

    // Личный вариант: вручную (выбор вариации для каждого номера)
    // map: taskNo -> variationKey (например "v1", "v2"...)
    void personalManualChosen(const QMap<int, QString>& selection);

private:
    void buildUi();
    void applyStyles();

    void showMainMenu();
    void showManualSelect();

    bool loadCatalogVariations(QString* err = nullptr);
    QString dataPath(const QString& fileName) const;

private:
    QStackedWidget* stack_ = nullptr;

    // page 0: main menu
    QWidget* pageMain_ = nullptr;
    QPushButton* backBtnMain_ = nullptr;

    QComboBox* readyCombo_ = nullptr;
    QPushButton* readyStartBtn_ = nullptr;

    QPushButton* personalAutoBtn_ = nullptr;
    QPushButton* personalManualBtn_ = nullptr;

    // page 1: manual select
    QWidget* pageManual_ = nullptr;
    QPushButton* backBtnManual_ = nullptr;
    QPushButton* startManualBtn_ = nullptr;

    QScrollArea* scroll_ = nullptr;

    // taskNo -> combo with variations
    QMap<int, QComboBox*> manualCombos_;

    // taskNo -> list of (variationKey, displayName)
    QMap<int, QList<QPair<QString, QString>>> catalogVars_;
};
