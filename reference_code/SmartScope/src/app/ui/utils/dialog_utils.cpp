#include "app/ui/utils/dialog_utils.h"
#include <QtWidgets> // Include necessary headers for implementation

namespace SmartScope::App::Ui {

QMessageBox::StandardButton DialogUtils::showStyledConfirmationDialog(
    QWidget *parent, 
    const QString &title, 
    const QString &text, 
    const QString &yesText, 
    const QString &noText
) {
    // 1. 定义 QMessageBox 基础样式 (背景、边框、提示文字)
    QString msgBoxBaseStyle = 
        "QMessageBox { "
        "    background-color: #252526; "
        "    border-radius: 12px; " 
        "    border: 1px solid #444; " 
        "    padding: 25px; "           
        "}"        
        "QMessageBox QLabel { "         /* 提示文字 */
        "    color: #E0E0E0; "
        "    background-color: transparent; "
        "    min-width: 700px; "        
        "    padding: 40px; "           
        "    font-size: 24pt; "         
        "}";

    // 2. 定义确认按钮 (Yes) 的样式 - 按钮缩小
    QString yesButtonStyle = 
        "QPushButton { "
        "    background-color: #D9534F; " /* 红色 */
        "    color: white; "
        "    padding: 10px 25px; "      
        "    border-radius: 8px; "      
        "    border: none; "
        "    min-height: 45px; "        
        "    min-width: 160px; "        
        "    font-size: 18pt; "         
        "    margin: 10px 15px; "       
        "}"        
        "QPushButton:hover { background-color: #C9302C; }"        
        "QPushButton:pressed { background-color: #AC2925; }";

    QMessageBox msgBox(parent);
    msgBox.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint); 
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setStyleSheet(msgBoxBaseStyle); 
    msgBox.setMinimumWidth(900); 

    QPushButton *yesButton = msgBox.addButton(yesText, QMessageBox::YesRole); 
    yesButton->setStyleSheet(yesButtonStyle); 
    
    // 只在noText非空时添加No按钮
    QPushButton *noButton = nullptr;
    if (!noText.isEmpty()) {
    // 3. 定义取消按钮 (No) 的样式 - 按钮缩小
    QString noButtonStyle = 
        "QPushButton { "
        "    background-color: #555555; " /* 灰色 */
        "    color: white; "
        "    padding: 10px 25px; "      
        "    border-radius: 8px; "      
        "    border: none; "
        "    min-height: 45px; "        
        "    min-width: 160px; "        
        "    font-size: 18pt; "         
        "    margin: 10px 15px; "       
        "}"        
        "QPushButton:hover { background-color: #666666; }"        
        "QPushButton:pressed { background-color: #444444; }";

        noButton = msgBox.addButton(noText, QMessageBox::NoRole);
    noButton->setStyleSheet(noButtonStyle);
    msgBox.setDefaultButton(noButton); 
    } else {
        msgBox.setDefaultButton(yesButton);
    }

    msgBox.exec(); 

    // 返回对应的标准按钮
    if (msgBox.clickedButton() == yesButton) {
        return QMessageBox::Yes;
    } else {
        return QMessageBox::No; 
    }
}

} // namespace SmartScope::App::Ui 