#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "app/ui/base_page.h"
#include <QCheckBox>

/**
 * @brief 参数设置页面
 */
class SettingsPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit SettingsPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~SettingsPage();

signals:
    /**
     * @brief 帧率显示设置变更信号
     * @param show 是否显示帧率
     */
    void showFpsSettingChanged(bool show);

    /**
     * @brief 调试模式设置变更信号
     */
    void debugModeSettingChanged(bool enabled);

protected:
    /**
     * @brief 初始化内容区域
     */
    void initContent() override;

private slots:
    /**
     * @brief 处理帧率显示开关状态变更
     * @param checked 是否选中
     */
    void onShowFpsToggled(bool checked);

    /**
     * @brief 处理调试模式开关状态变更
     */
    void onDebugModeToggled(bool checked);

private:
    QCheckBox* m_showFpsCheckBox;  ///< 帧率显示开关
    QCheckBox* m_debugModeCheckBox; ///< 调试模式开关
};

#endif // SETTINGS_PAGE_H 