#ifndef MEASUREMENT_TYPE_SELECTION_PAGE_H
#define MEASUREMENT_TYPE_SELECTION_PAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include "app/ui/measurement_object.h"
#include "app/ui/base_page.h"

// 测量类型卡片
class MeasurementTypeCard : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementTypeCard(MeasurementType type, const QString& title, const QString& iconPath, QWidget* parent = nullptr);
    ~MeasurementTypeCard();

    MeasurementType getType() const;

signals:
    void cardClicked(MeasurementType type);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    MeasurementType m_type;
    QString m_title;
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QVBoxLayout* m_layout;
};

// 测量类型选择页面
class MeasurementTypeSelectionPage : public BasePage
{
    Q_OBJECT

public:
    explicit MeasurementTypeSelectionPage(QWidget* parent = nullptr);
    ~MeasurementTypeSelectionPage();

signals:
    void measurementTypeSelected(MeasurementType type);
    void cancelSelection();

protected:
    void initContent() override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onCardClicked(MeasurementType type);
    void onCancelClicked();

private:
    void createMeasurementTypeCards();

    QGridLayout* m_cardsLayout;
    QPushButton* m_cancelButton;

    QVector<MeasurementTypeCard*> m_typeCards;
};

#endif // MEASUREMENT_TYPE_SELECTION_PAGE_H 