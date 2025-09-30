#ifndef PROFILE_CHART_MANAGER_H
#define PROFILE_CHART_MANAGER_H

#include <QObject>
#include <QWidget>
#include <QVector>
#include <QVector3D>
#include <QPointF>
#include <QString>
#include "app/ui/measurement_object.h"
#include "qcustomplot.h"

namespace SmartScope {
namespace App {
namespace Ui {

/**
 * @brief 剖面图表管理器，负责处理测量页面中的剖面图表显示和交互
 */
class ProfileChartManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit ProfileChartManager(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ProfileChartManager();
    
    /**
     * @brief 初始化图表控件
     * @param chartPlot 图表控件指针
     * @return 是否初始化成功
     */
    bool initializeChart(QCustomPlot *chartPlot);
    
    /**
     * @brief 更新图表数据
     * @param profileData 剖面数据点
     * @param title 图表标题
     * @return 是否更新成功
     */
    bool updateChartData(const QVector<QPointF> &profileData, const QString &title = "剖面图");
    
    /**
     * @brief 切换图表可见性
     * @return 切换后的可见性状态
     */
    bool toggleChartVisibility();
    
    /**
     * @brief 设置图表可见性
     * @param visible 是否可见
     */
    void setChartVisible(bool visible);
    
    /**
     * @brief 获取图表可见性
     * @return 图表是否可见
     */
    bool isChartVisible() const;
    
    /**
     * @brief 从测量对象中提取剖面数据
     * @param measurement 剖面测量对象
     * @return 剖面数据点集合
     */
    QVector<QPointF> extractProfileData(const MeasurementObject* measurement);
    
    /**
     * @brief 更新控件可见性
     * @param measurements 测量对象列表
     * @return 是否存在剖面测量
     */
    bool updateControlsVisibility(const QVector<MeasurementObject*>& measurements);
    
    /**
     * @brief 应用深色主题到图表
     */
    void applyDarkTheme();

    /**
     * @brief 更新起伏统计信息
     * @param minElevation 最小高程
     * @param maxElevation 最大高程
     * @param elevationRange 起伏范围
     */
    void updateElevationStats(float minElevation, float maxElevation, float elevationRange);

private:
    /**
     * @brief 更新图表标题
     * @param title 图表标题
     */
    void updateChartTitle(const QString &title);

private:
    QCustomPlot *m_chartPlot;  ///< 图表控件指针
    bool m_initialized;         ///< 初始化标志
};

} // namespace Ui
} // namespace App
} // namespace SmartScope

#endif // PROFILE_CHART_MANAGER_H 