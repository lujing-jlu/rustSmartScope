#ifndef MEASUREMENT_DELETE_DIALOG_H
#define MEASUREMENT_DELETE_DIALOG_H

#include <QDialog>
#include <QVector>
#include <QString>
#include "app/ui/measurement_object.h"

// Forward declarations
namespace Ui { class MeasurementDeleteDialog; }
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QWidget;
class QLabel;
class QPushButton;
class QVariant;
class MeasurementObject;

namespace SmartScope::App::Ui {

class MeasurementDeleteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MeasurementDeleteDialog(QWidget *parent = nullptr);
    ~MeasurementDeleteDialog() override;

    // 用测量对象列表填充对话框内容
    void populateList(const QVector<MeasurementObject*>& measurements);

signals:
    // 当用户点击某个测量项的删除按钮时发出
    void measurementToDelete(MeasurementObject* measurementObject);

private slots:
    // 内部槽，用于处理某个删除按钮的点击，并发出 measurementToDelete 信号
    void onDeleteButtonClicked();

private:
    // 清空列表内容
    void clearList();
    // 将 MeasurementType 转换为用户友好的字符串
    QString measurementTypeToString(MeasurementType type);
    // 将 MeasurementType 转换为图标路径 (可选)
    QString measurementTypeToIconPath(MeasurementType type);

    QVBoxLayout* m_mainLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollWidget = nullptr; // 滚动区域的内容窗口
    QVBoxLayout* m_listLayout = nullptr; // 放置测量项的布局
    
    // 添加一个关闭按钮
    QPushButton* m_closeButton = nullptr;
};

} // namespace SmartScope::App::Ui

#endif // MEASUREMENT_DELETE_DIALOG_H 