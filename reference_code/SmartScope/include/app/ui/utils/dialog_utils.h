#ifndef DIALOG_UTILS_H
#define DIALOG_UTILS_H

#include <QtWidgets/QMessageBox> // Include necessary headers
#include <QString>

class QWidget; // Forward declaration

namespace SmartScope::App::Ui {

/**
 * @brief UI相关的工具类
 */
class DialogUtils 
{
public:
    /**
     * @brief 显示带统一样式的确认对话框
     * @param parent 父窗口指针
     * @param title 对话框标题
     * @param text 对话框提示文本
     * @param yesText 确认按钮文本
     * @param noText 取消按钮文本
     * @return 用户点击的标准按钮 (QMessageBox::Yes 或 QMessageBox::No/NoButton)
     */
    static QMessageBox::StandardButton showStyledConfirmationDialog(
        QWidget *parent, 
        const QString &title, 
        const QString &text, 
        const QString &yesText, 
        const QString &noText
    );
};

} // namespace SmartScope::App::Ui

#endif // DIALOG_UTILS_H 