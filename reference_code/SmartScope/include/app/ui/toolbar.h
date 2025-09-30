#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <QWidget>
#include <QPushButton>
#include <QMap>
#include <QString>
#include <QLabel>

namespace SmartScope {

/**
 * @brief 工具栏组件，用于管理右侧工具按钮
 */
class ToolBar : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit ToolBar(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ToolBar();
    
    /**
     * @brief 添加工具按钮
     * @param id 按钮ID
     * @param iconPath 图标路径
     * @param tooltip 提示文本
     * @param position 位置索引（从上到下）
     * @return 按钮指针
     */
    QPushButton* addButton(const QString& id, const QString& iconPath, const QString& tooltip, int position);

    /**
     * @brief 添加底部固定按钮（固定在工具栏最下方）
     */
    QPushButton* addBottomButton(const QString& id, const QString& iconPath, const QString& tooltip);
    
    /**
     * @brief 获取按钮
     * @param id 按钮ID
     * @return 按钮指针，如果不存在则返回nullptr
     */
    QPushButton* getButton(const QString& id);
    
    /**
     * @brief 移除按钮
     * @param id 按钮ID
     * @return 是否成功移除
     */
    bool removeButton(const QString& id);
    
    /**
     * @brief 显示指定按钮
     * @param id 按钮ID
     */
    void showButton(const QString& id);
    
    /**
     * @brief 隐藏指定按钮
     * @param id 按钮ID
     */
    void hideButton(const QString& id);
    
    /**
     * @brief 更新工具栏位置
     */
    void updatePosition();

    /**
     * @brief 设置底部信息文本可见性（用于录制时间）
     */
    void setBottomInfoVisible(bool visible);

    /**
     * @brief 更新底部信息文本内容
     */
    void setBottomInfoText(const QString& text);

protected:
    /**
     * @brief 调整大小事件
     * @param event 事件
     */
    void resizeEvent(QResizeEvent *event) override;
    
    /**
     * @brief 显示事件
     * @param event 事件
     */
    void showEvent(QShowEvent *event) override;

private:
    /**
     * @brief 创建白色图标
     * @param iconPath 图标路径
     * @return 白色图标
     */
    QIcon createWhiteIcon(const QString& iconPath);

    /**
     * @brief 创建现代化图标
     * @param iconType 图标类型
     * @return 现代化图标
     */
    QIcon createModernIcon(const QString& iconType);
    
    /**
     * @brief 重新排列按钮
     */
    void rearrangeButtons();
    
    QMap<QString, QPushButton*> m_buttons;  ///< 按钮映射
    QMap<QString, int> m_buttonPositions;   ///< 按钮位置映射
    int m_buttonSize = 80;                  ///< 按钮大小
    int m_buttonSpacing = 20;               ///< 按钮间距
    int m_rightMargin = 100;                ///< 右边距

    // 底部固定按钮与信息
    QString m_bottomButtonId;               ///< 底部按钮ID
    QLabel* m_bottomInfoLabel = nullptr;    ///< 底部文本（录制计时）
};

} // namespace SmartScope

#endif // TOOLBAR_H 