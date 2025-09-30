#include "app/ui/profile_chart_manager.h"
#include "infrastructure/logging/logger.h"
#include <limits>

namespace SmartScope {
namespace App {
namespace Ui {

ProfileChartManager::ProfileChartManager(QObject *parent)
    : QObject(parent),
      m_chartPlot(nullptr),
      m_initialized(false)
{
    LOG_INFO("创建剖面图表管理器");
}

ProfileChartManager::~ProfileChartManager()
{
    LOG_INFO("销毁剖面图表管理器");
    // 不需要删除m_chartPlot，因为它是父控件的子控件，会被自动删除
}

bool ProfileChartManager::initializeChart(QCustomPlot *chartPlot)
{
    if (!chartPlot) {
        LOG_ERROR("初始化图表失败: 无效的图表控件指针");
        return false;
    }
    
    m_chartPlot = chartPlot;
    
    // 应用深色主题
    applyDarkTheme();
    
    // 禁用全部交互，只保留拖拽和缩放
    m_chartPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    
    // 禁用所有可能导致闪烁竖线的追踪和选择功能
    m_chartPlot->setSelectionRectMode(QCP::srmNone); // 禁用选择矩形
    
    // 创建并配置主图形
    if (m_chartPlot->graphCount() == 0) {
        m_chartPlot->addGraph();
    }
    
    // 禁用图形选择
    m_chartPlot->graph(0)->setSelectable(QCP::stNone);
    
    // 禁用垂直和水平光标追踪
    m_chartPlot->setMultiSelectModifier(Qt::NoModifier);
    m_chartPlot->axisRect()->setRangeDragAxes(m_chartPlot->xAxis, m_chartPlot->yAxis);
    m_chartPlot->axisRect()->setRangeZoomAxes(m_chartPlot->xAxis, m_chartPlot->yAxis);
    
    // 禁用数据点追踪和选择
    m_chartPlot->graph(0)->setScatterStyle(QCPScatterStyle::ssNone);
    m_chartPlot->graph(0)->setAdaptiveSampling(true); // 启用自适应采样提高性能
    
    // 禁用全部轴交互
    m_chartPlot->xAxis->setSelectableParts(QCPAxis::spNone);
    m_chartPlot->yAxis->setSelectableParts(QCPAxis::spNone);
    
    // 禁用QCustomPlot的自动重绘功能
    m_chartPlot->setAutoAddPlottableToLegend(false);
    m_chartPlot->setPlottingHint(QCP::phFastPolylines, true);
    
    // 禁用垂直和水平线的追踪
    m_chartPlot->setSelectionRectMode(QCP::srmNone);
    
    // 禁用图例
    if (m_chartPlot->legend) {
        m_chartPlot->legend->setVisible(false);
    }
    
    // 初始隐藏
    m_chartPlot->setVisible(false);
    
    m_initialized = true;
    LOG_INFO("剖面图表初始化成功");
    return true;
}

void ProfileChartManager::applyDarkTheme()
{
    if (!m_chartPlot) {
        LOG_ERROR("应用深色主题失败: 图表控件未初始化");
        return;
    }
    
    // 背景色
    m_chartPlot->setBackground(QColor(40, 40, 40)); // 深灰色背景
    
    // 坐标轴设置
    QPen axisPen(QColor(200, 200, 200), 1); // 浅灰色轴线
    QFont labelFont("WenQuanYi Zen Hei", 24); // 增大轴标签字体
    QFont tickLabelFont("WenQuanYi Zen Hei", 20); // 增大刻度标签字体
    QColor textColor(220, 220, 220); // 浅灰色文本
    
    // X轴
    m_chartPlot->xAxis->setBasePen(axisPen);
    m_chartPlot->xAxis->setTickPen(axisPen);
    m_chartPlot->xAxis->setSubTickPen(axisPen);
    m_chartPlot->xAxis->setLabelFont(labelFont);
    m_chartPlot->xAxis->setTickLabelFont(tickLabelFont);
    m_chartPlot->xAxis->setLabelColor(textColor);
    m_chartPlot->xAxis->setTickLabelColor(textColor);
    m_chartPlot->xAxis->setLabel("Distance (mm)");
    
    // Y轴
    m_chartPlot->yAxis->setBasePen(axisPen);
    m_chartPlot->yAxis->setTickPen(axisPen);
    m_chartPlot->yAxis->setSubTickPen(axisPen);
    m_chartPlot->yAxis->setLabelFont(labelFont);
    m_chartPlot->yAxis->setTickLabelFont(tickLabelFont);
    m_chartPlot->yAxis->setLabelColor(textColor);
    m_chartPlot->yAxis->setTickLabelColor(textColor);
    m_chartPlot->yAxis->setLabel("Surface Elevation (mm)");
    
    // 图表曲线设置
    if (m_chartPlot->graphCount() == 0) {
        m_chartPlot->addGraph();
    }
    QPen graphPen(QColor(100, 150, 255), 4); // 蓝色曲线，线宽增大为4
    m_chartPlot->graph(0)->setPen(graphPen);
    
    LOG_INFO("已应用深色主题到剖面图表");
}

bool ProfileChartManager::updateChartData(const QVector<QPointF> &profileData, const QString &title)
{
    if (!m_initialized || !m_chartPlot) {
        LOG_ERROR("更新图表数据失败: 图表控件未初始化");
        return false;
    }
    
    // 暂时阻止自动重绘
    m_chartPlot->setNotAntialiasedElements(QCP::aeAll);
    m_chartPlot->setPlottingHint(QCP::phFastPolylines, true);
    
    // 清除旧数据
    m_chartPlot->graph(0)->data()->clear();
    
    // 如果剖面数据为空，显示错误信息
    if (profileData.isEmpty()) {
        LOG_WARNING("更新图表数据失败: 剖面数据为空");
        
        // 设置默认范围
        m_chartPlot->xAxis->setRange(0, 10);
        m_chartPlot->yAxis->setRange(0, 10);
        
        // 添加错误文本
        if (m_chartPlot->itemCount() > 0) {
            // 尝试找到并移除已有的文本项
            for (int i = 0; i < m_chartPlot->itemCount(); ++i) {
                if (qobject_cast<QCPItemText*>(m_chartPlot->item(i))) {
                    m_chartPlot->removeItem(i);
                    break;
                }
            }
        }
        
        QCPItemText* noDataText = new QCPItemText(m_chartPlot);
        noDataText->position->setCoords(5, 5);
        noDataText->setText("无剖面数据");
        noDataText->setFont(QFont("WenQuanYi Zen Hei", 24, QFont::Bold));
        noDataText->setColor(Qt::white);
        
        // 更新图表标题
        updateChartTitle(title);
        
        // 重绘图表
        m_chartPlot->replot(QCustomPlot::rpQueuedReplot);
        return false;
    }
    
    // 数据预处理：确保X轴数据单调递增，防止出现折返折线
    QVector<QPointF> processedData;
    processedData.reserve(profileData.size());
    
    // 如果原始数据足够（至少两个点）
    if (profileData.size() >= 2) {
        // 计算起点和终点
        QPointF startPoint = profileData.first();
        QPointF endPoint = profileData.last();
        
        // 检查是否需要调整X轴坐标（例如将距离作为X轴）
        bool useDistanceAsX = false;
        double totalLength = 0;
        
        // 检查X轴数据是否已经单调递增
        bool isMonotonic = true;
        for (int i = 1; i < profileData.size(); ++i) {
            if (profileData[i].x() <= profileData[i-1].x()) {
                isMonotonic = false;
                break;
            }
        }
        
        // 检查并处理X轴数据
        if (isMonotonic) {
            processedData = profileData;
            LOG_INFO("剖面数据X轴已单调递增，直接使用");
        } else {
            LOG_WARNING("剖面数据X轴非单调递增，需要重新处理");

            // 按X坐标排序数据点
            QVector<QPointF> sortedData = profileData;
            std::sort(sortedData.begin(), sortedData.end(),
                     [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });

            // 移除重复的X坐标点，保留Y值较大的点
            processedData.clear();
            if (!sortedData.isEmpty()) {
                processedData.append(sortedData.first());

                for (int i = 1; i < sortedData.size(); ++i) {
                    const QPointF& current = sortedData[i];
                    const QPointF& last = processedData.last();

                    // 如果X坐标相同，保留Y值较大的点
                    if (qAbs(current.x() - last.x()) < 0.001) {
                        if (qAbs(current.y()) > qAbs(last.y())) {
                            processedData.last() = current;
                        }
                    } else {
                        processedData.append(current);
                    }
                }
            }

            LOG_INFO(QString("剖面数据处理完成：原始%1个点 -> 处理后%2个点")
                    .arg(profileData.size()).arg(processedData.size()));
        }
    } else {
        // 如果数据点不足，直接使用原始数据
        processedData = profileData;
    }

    // 深度后处理：对剖面Y（深度/高程）取反，并将最小值归零
    if (!processedData.isEmpty()) {
        double minInverted = std::numeric_limits<double>::max();
        for (const QPointF &p : processedData) {
            double inv = -p.y();
            if (inv < minInverted) minInverted = inv;
        }
        for (QPointF &p : processedData) {
            double inv = -p.y();
            p.setY(inv - minInverted);
        }
    }

    // 设置新数据 (X: 距离, Y: 深度/高度)
    QVector<double> xData, yData;
    xData.reserve(processedData.size());
    yData.reserve(processedData.size());

    // 添加所有点
    for (const QPointF &point : processedData) {
        xData.append(point.x());
        yData.append(point.y());
    }

    // 设置图表数据
    m_chartPlot->graph(0)->setData(xData, yData);

    // 计算数据范围
    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();

    for (const QPointF &point : processedData) {
        if (point.x() < xMin) xMin = point.x();
        if (point.x() > xMax) xMax = point.x();
        if (point.y() < yMin) yMin = point.y();
        if (point.y() > yMax) yMax = point.y();
    }

    // 设置合理的边距
    double xMargin = (xMax - xMin) * 0.05;
    double yMargin = (yMax - yMin) * 0.1;
    if (xMargin < 0.1) xMargin = 0.1;
    if (yMargin < 0.1) yMargin = 0.1;

    // 设置图表范围
    m_chartPlot->xAxis->setRange(xMin - xMargin, xMax + xMargin);
    m_chartPlot->yAxis->setRange(yMin - yMargin, yMax + yMargin);

    LOG_INFO(QString("设置图表范围 X: [%1, %2] 边距: %3, Y: [%4, %5] 边距: %6")
            .arg(xMin).arg(xMax).arg(xMargin)
            .arg(yMin).arg(yMax).arg(yMargin));

    // 更新图表标题
    updateChartTitle(title);

    // 重绘图表 - 使用队列方式重绘减少闪烁
    m_chartPlot->replot(QCustomPlot::rpQueuedReplot);
    LOG_INFO(QString("已更新剖面图表数据，共 %1 个点").arg(processedData.size()));

    return true;
}

// 辅助方法：更新图表标题
void ProfileChartManager::updateChartTitle(const QString &title)
{
    // 检查是否需要添加标题行
    if (m_chartPlot->plotLayout()->rowCount() == 1) {
        m_chartPlot->plotLayout()->insertRow(0);
    }

    // 检查是否已有标题元素
    if (m_chartPlot->plotLayout()->elementCount() > 0) {
        // 移除现有标题
        QCPTextElement* existingTitle = qobject_cast<QCPTextElement*>(
            m_chartPlot->plotLayout()->element(0, 0));
        if (existingTitle) {
            m_chartPlot->plotLayout()->remove(existingTitle);
        }
    }

    // 添加新标题
    QCPTextElement* titleElement = new QCPTextElement(m_chartPlot, title);
    titleElement->setFont(QFont("Arial", 12, QFont::Bold));
    titleElement->setTextColor(QColor(50, 50, 50));
    m_chartPlot->plotLayout()->addElement(0, 0, titleElement);

    LOG_INFO(QString("已更新剖面图表标题: %1").arg(title));
}

// 切换图表可见性
bool ProfileChartManager::toggleChartVisibility()
{
    if (!m_initialized || !m_chartPlot) {
        LOG_ERROR("切换图表可见性失败: 图表控件未初始化");
        return false;
    }
    
    bool currentlyVisible = m_chartPlot->isVisible();
    m_chartPlot->setVisible(!currentlyVisible);
    
    LOG_INFO(currentlyVisible ? "隐藏剖面图表" : "显示剖面图表");
    return !currentlyVisible;
}

void ProfileChartManager::setChartVisible(bool visible)
{
    if (!m_initialized || !m_chartPlot) {
        LOG_ERROR("设置图表可见性失败: 图表控件未初始化");
        return;
    }
    
    m_chartPlot->setVisible(visible);
    if (visible) {
        LOG_INFO("显示剖面图表");
    } else {
        LOG_INFO("隐藏剖面图表");
    }
}

bool ProfileChartManager::isChartVisible() const
{
    if (!m_initialized || !m_chartPlot) {
        LOG_ERROR("获取图表可见性失败: 图表控件未初始化");
        return false;
    }
    
    return m_chartPlot->isVisible();
}

QVector<QPointF> ProfileChartManager::extractProfileData(const MeasurementObject* measurement)
{
    QVector<QPointF> profileData;
    
    if (!measurement || measurement->getType() != MeasurementType::Profile) {
        LOG_WARNING("提取剖面数据失败: 无效的测量对象或非剖面类型");
        return profileData;
    }
    
    // 先尝试从测量对象中获取已计算好的剖面数据
    profileData = measurement->getProfileData();
    if (!profileData.isEmpty()) {
        LOG_INFO(QString("直接使用已计算的剖面数据: %1个点").arg(profileData.size()));
        
        // 检查X轴数据是否单调递增
        bool isMonotonic = true;
        for (int i = 1; i < profileData.size(); ++i) {
            if (profileData[i].x() <= profileData[i-1].x()) {
                isMonotonic = false;
                break;
            }
        }
        
        // 如果数据已经单调递增，直接返回
        if (isMonotonic) {
            return profileData;
        }
        
        // 否则，进行处理以确保X轴单调递增
        LOG_WARNING("已计算的剖面数据X轴非单调递增，需要调整");
    } else {
        LOG_WARNING("测量对象没有预先计算的剖面数据，尝试生成简单剖面");
    }
    
    // 如果没有预先计算的剖面数据，或者数据需要调整，生成新的剖面数据
    const QVector<QVector3D>& points = measurement->getPoints();
    if (points.size() < 2) {
        LOG_WARNING("提取剖面数据失败: 点数量不足");
        return profileData;
    }
    
    // 重新计算剖面 - 使用起点到终点的直线采样
    QVector3D startPoint = points[0];
    QVector3D endPoint = points[1];
    
    // 计算线段向量
    QVector3D lineVector = endPoint - startPoint;
    float totalLength = lineVector.length();
    
    // 归一化方向向量
    if (totalLength < 0.001f) {
        LOG_WARNING("剖面线段长度几乎为零，无法生成剖面");
        return profileData;
    }
    
    // 确保生成足够多的点以获得平滑的剖面图
    const int numSamples = 100; // 生成100个点以确保平滑
    profileData.clear();
    profileData.reserve(numSamples);
    
    // 生成均匀分布的采样点
    for (int i = 0; i < numSamples; ++i) {
        float t = i / static_cast<float>(numSamples - 1);
        
        // 修复：使用正确的沿剖面线距离（与MeasurementCalculator一致）
        float distance = t * totalLength;
        
        // 线性插值得到当前点
        QVector3D currentPoint = startPoint + lineVector * t;
        
        // 计算表面起伏：相对于线性基准的高程偏差
        float baselineZ = startPoint.z() + t * (endPoint.z() - startPoint.z());
        float elevation = currentPoint.z() - baselineZ;
        
        profileData.append(QPointF(distance, elevation));
    }
    
    LOG_INFO(QString("生成了表面起伏剖面数据，总长度: %1 mm，共 %2 个点")
            .arg(totalLength).arg(profileData.size()));
    
    // 计算起伏统计信息
    if (!profileData.isEmpty()) {
        float minElevation = std::numeric_limits<float>::max();
        float maxElevation = std::numeric_limits<float>::lowest();
        
        for (const QPointF& point : profileData) {
            float elevation = point.y();
            if (elevation < minElevation) minElevation = elevation;
            if (elevation > maxElevation) maxElevation = elevation;
        }
        
        LOG_INFO(QString("ProfileChartManager：高程变化范围: [%1, %2]mm，最大起伏: %3mm")
                .arg(minElevation, 0, 'f', 2)
                .arg(maxElevation, 0, 'f', 2)
                .arg(maxElevation - minElevation, 0, 'f', 2));
    }
    
    // 检测和处理异常值 - 先找出Y值的合理范围
    if (profileData.size() > 5) {
        // 计算Y值的中位数和标准差
        QVector<double> yValues;
        yValues.reserve(profileData.size());
        
        for (const QPointF& point : profileData) {
            yValues.append(point.y());
        }
        
        // 排序以计算中位数
        std::sort(yValues.begin(), yValues.end());
        double median = yValues[yValues.size() / 2];
        
        // 计算绝对偏差的中位数(MAD)，这是一个稳健的离散度量
        QVector<double> absoluteDeviations;
        absoluteDeviations.reserve(yValues.size());
        
        for (double y : yValues) {
            absoluteDeviations.append(qAbs(y - median));
        }
        
        std::sort(absoluteDeviations.begin(), absoluteDeviations.end());
        double mad = absoluteDeviations[absoluteDeviations.size() / 2];
        
        // 设置阈值，通常使用MAD的几倍作为异常值判断标准
        // 对于正态分布，MAD * 1.4826 ≈ 标准差
        double threshold = mad * 3.0 * 1.4826; // 3倍标准差
        
        LOG_INFO(QString("剖面数据Y值中位数: %1, MAD: %2, 异常值阈值: %3")
                .arg(median).arg(mad).arg(threshold));
        
        // 检测和处理异常值
        for (int i = 0; i < profileData.size(); ++i) {
            double y = profileData[i].y();
            
            // 如果Y值偏离中位数太远，认为是异常值
            if (qAbs(y - median) > threshold) {
                LOG_INFO(QString("检测到异常值: 索引=%1, X=%2, Y=%3")
                        .arg(i).arg(profileData[i].x()).arg(y));
                
                // 使用线性插值替换异常值
                double interpolatedY = median; // 默认使用中位数
                
                // 尝试在前后找到有效值进行插值
                int prevValid = -1;
                int nextValid = -1;
                
                // 向前查找有效值
                for (int j = i - 1; j >= 0; --j) {
                    if (qAbs(profileData[j].y() - median) <= threshold) {
                        prevValid = j;
                        break;
                    }
                }
                
                // 向后查找有效值
                for (int j = i + 1; j < profileData.size(); ++j) {
                    if (qAbs(profileData[j].y() - median) <= threshold) {
                        nextValid = j;
                        break;
                    }
                }
                
                // 如果前后都有有效值，进行线性插值
                if (prevValid >= 0 && nextValid >= 0) {
                    double x1 = profileData[prevValid].x();
                    double y1 = profileData[prevValid].y();
                    double x2 = profileData[nextValid].x();
                    double y2 = profileData[nextValid].y();
                    double x = profileData[i].x();
                    
                    // 线性插值公式: y = y1 + (y2-y1)*(x-x1)/(x2-x1)
                    interpolatedY = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
                }
                // 如果只有前面有有效值，使用前面的值
                else if (prevValid >= 0) {
                    interpolatedY = profileData[prevValid].y();
                }
                // 如果只有后面有有效值，使用后面的值
                else if (nextValid >= 0) {
                    interpolatedY = profileData[nextValid].y();
                }
                
                // 替换异常值
                profileData[i].setY(interpolatedY);
                LOG_INFO(QString("已将异常值替换为插值: %1").arg(interpolatedY));
            }
        }
    }
    
    // 对数据进行平滑处理
    if (profileData.size() > 5) {
        // 保存原始数据副本
        QVector<QPointF> originalData = profileData;
        
        // 创建平滑后的数据数组
        QVector<QPointF> smoothedData;
        smoothedData.reserve(profileData.size());
        
        // 使用简单的移动平均法进行平滑处理
        const int windowSize = 5;  // 平滑窗口大小
        
        for (int i = 0; i < profileData.size(); ++i) {
            double sumY = 0.0;
            int count = 0;
            
            // 计算窗口内的平均值
            for (int j = qMax(0, i - windowSize/2); j <= qMin(profileData.size() - 1, i + windowSize/2); ++j) {
                sumY += profileData[j].y();
                count++;
            }
            
            if (count > 0) {
                // 保持X轴不变，只平滑Y轴
                smoothedData.append(QPointF(profileData[i].x(), sumY / count));
            } else {
                smoothedData.append(profileData[i]);
            }
        }
        
        // 替换为平滑后的数据
        profileData = smoothedData;
        
        // 检查平滑后与原始数据的差异
        int restoredCount = 0;
        
        // 计算差异的统计特性
        double sumDiff = 0.0;
        double sumDiffSquared = 0.0;
        
        for (int i = 0; i < originalData.size(); ++i) {
            double diff = qAbs(originalData[i].y() - profileData[i].y());
            sumDiff += diff;
            sumDiffSquared += diff * diff;
        }
        
        double avgDiff = sumDiff / originalData.size();
        double stdDevDiff = qSqrt(sumDiffSquared / originalData.size() - avgDiff * avgDiff);
        
        // 设置差异阈值为平均差异加上3倍标准差
        double diffThreshold = avgDiff + 3.0 * stdDevDiff;
        
        LOG_INFO(QString("剖面数据平滑统计: 平均差异=%1, 标准差=%2, 阈值=%3")
                .arg(avgDiff).arg(stdDevDiff).arg(diffThreshold));
        
        // 恢复差异过大的点
        for (int i = 0; i < originalData.size(); ++i) {
            double diff = qAbs(originalData[i].y() - profileData[i].y());
            
            if (diff > diffThreshold) {
                profileData[i].setY(originalData[i].y());
                restoredCount++;
            }
        }
        
        if (restoredCount > 0) {
            LOG_INFO(QString("已恢复 %1 个与原始数据差异过大的点").arg(restoredCount));
        }
        
        LOG_INFO("已对剖面数据进行平滑处理");
    }
    
    return profileData;
}

bool ProfileChartManager::updateControlsVisibility(const QVector<MeasurementObject*>& measurements)
{
    bool profileMeasurementExists = false;
    
    // 检查是否存在剖面测量对象
    for (const MeasurementObject* measurement : measurements) {
        if (measurement && measurement->getType() == MeasurementType::Profile) {
            profileMeasurementExists = true;
            break;
        }
    }
    
    // 如果没有剖面测量，强制隐藏图表
    if (!profileMeasurementExists && m_chartPlot) {
        m_chartPlot->setVisible(false);
        LOG_INFO("无剖面测量，隐藏剖面图表");
    }
    
    return profileMeasurementExists;
}

/**
 * @brief 更新起伏统计信息
 * @param minElevation 最小高程
 * @param maxElevation 最大高程
 * @param elevationRange 起伏范围
 */
void ProfileChartManager::updateElevationStats(float minElevation, float maxElevation, float elevationRange)
{
    LOG_INFO(QString("ProfileChartManager：更新起伏统计: 最小高程=%1mm, 最大高程=%2mm, 起伏范围=%3mm")
             .arg(minElevation, 0, 'f', 2)
             .arg(maxElevation, 0, 'f', 2)
             .arg(elevationRange, 0, 'f', 2));

    // 这里可以添加额外的图表更新逻辑，比如更新图表标题或添加统计信息标注
    // 例如在图表上显示起伏范围信息

    if (m_chartPlot) {
        // 确保图表显示正确的统计信息
        // 这里可以添加显示起伏范围的文本标签或其他可视化元素
        m_chartPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

} // namespace Ui
} // namespace App
} // namespace SmartScope 