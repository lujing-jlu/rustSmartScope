#pragma once

#include <QPushButton>
#include <QWidget>
#include <QHBoxLayout>

// 测量菜单按钮类
class MeasurementMenuButton : public QPushButton {
    Q_OBJECT
    
public:
    MeasurementMenuButton(const QString &iconPath, const QString &text, QWidget *parent = nullptr);
    
private:
    void initialize();
};

// 测量菜单栏类
class MeasurementMenuBar : public QWidget {
    Q_OBJECT
    
public:
    explicit MeasurementMenuBar(QWidget *parent = nullptr);
    
    MeasurementMenuButton* addButton(const QString &iconPath, const QString &text);
    QWidget* backgroundPanel() const { return m_backgroundPanel; }
    
private:
    void setupUI();
    
    QHBoxLayout *m_layout;
    QWidget *m_backgroundPanel;
    QHBoxLayout *m_buttonLayout;
}; 