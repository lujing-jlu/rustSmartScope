#ifndef MEASUREMENT_STATE_MANAGER_H
#define MEASUREMENT_STATE_MANAGER_H

#include <QObject>
#include "app/ui/measurement_object.h"

// 测量操作模式
enum class MeasurementMode {
    View,           // 查看模式，用于浏览点云
    Add,            // 添加测量模式，用于添加新的测量
    Edit,           // 编辑测量模式，用于编辑现有的测量
    Delete          // 删除测量模式，用于删除测量
};

// 测量状态管理器
class MeasurementStateManager : public QObject
{
    Q_OBJECT

public:
    explicit MeasurementStateManager(QObject* parent = nullptr);
    ~MeasurementStateManager();

    // 获取当前测量模式
    MeasurementMode getMeasurementMode() const;
    
    // 设置当前测量模式
    void setMeasurementMode(MeasurementMode mode);
    
    // 获取当前活动的测量类型
    MeasurementType getActiveMeasurementType() const;
    
    // 设置当前活动的测量类型
    void setActiveMeasurementType(MeasurementType type);
    
    // 是否处于测量添加状态
    bool isAddingMeasurement() const;
    
    // 是否处于测量编辑状态
    bool isEditingMeasurement() const;
    
    // 开始添加测量
    void startAddMeasurement(MeasurementType type);
    
    // 开始编辑测量
    void startEditMeasurement(MeasurementObject* measurement);
    
    // 取消当前操作
    void cancelOperation();
    
    // 完成当前操作
    void completeOperation();

signals:
    // 测量模式改变信号
    void measurementModeChanged(MeasurementMode mode);
    
    // 活动测量类型改变信号
    void activeMeasurementTypeChanged(MeasurementType type);
    
    // 操作开始信号
    void operationStarted();
    
    // 操作取消信号
    void operationCancelled();
    
    // 操作完成信号
    void operationCompleted();

private:
    MeasurementMode m_currentMode;           // 当前测量模式
    MeasurementType m_activeMeasurementType; // 当前活动的测量类型
};

#endif // MEASUREMENT_STATE_MANAGER_H 