#include "app/ui/measurement_state_manager.h"
#include "infrastructure/logging/logger.h"

MeasurementStateManager::MeasurementStateManager(QObject* parent)
    : QObject(parent)
    , m_currentMode(MeasurementMode::View)
    , m_activeMeasurementType(MeasurementType::Length)
{
    LOG_INFO("创建测量状态管理器");
}

MeasurementStateManager::~MeasurementStateManager()
{
    LOG_INFO("销毁测量状态管理器");
}

MeasurementMode MeasurementStateManager::getMeasurementMode() const
{
    return m_currentMode;
}

void MeasurementStateManager::setMeasurementMode(MeasurementMode mode)
{
    if (m_currentMode != mode) {
        m_currentMode = mode;
        LOG_INFO(QString("设置测量模式: %1").arg(static_cast<int>(mode)));
        emit measurementModeChanged(mode);
    }
}

MeasurementType MeasurementStateManager::getActiveMeasurementType() const
{
    return m_activeMeasurementType;
}

void MeasurementStateManager::setActiveMeasurementType(MeasurementType type)
{
    if (m_activeMeasurementType != type) {
        m_activeMeasurementType = type;
        LOG_INFO(QString("设置活动测量类型: %1").arg(static_cast<int>(type)));
        emit activeMeasurementTypeChanged(type);
    }
}

bool MeasurementStateManager::isAddingMeasurement() const
{
    return m_currentMode == MeasurementMode::Add;
}

bool MeasurementStateManager::isEditingMeasurement() const
{
    return m_currentMode == MeasurementMode::Edit;
}

void MeasurementStateManager::startAddMeasurement(MeasurementType type)
{
    setActiveMeasurementType(type);
    setMeasurementMode(MeasurementMode::Add);
    LOG_INFO(QString("开始添加测量，类型: %1").arg(static_cast<int>(type)));
    emit operationStarted();
}

void MeasurementStateManager::startEditMeasurement(MeasurementObject* measurement)
{
    if (measurement) {
        setActiveMeasurementType(measurement->getType());
        setMeasurementMode(MeasurementMode::Edit);
        LOG_INFO("开始编辑测量");
        emit operationStarted();
    }
}

void MeasurementStateManager::cancelOperation()
{
    LOG_INFO("取消当前操作");
    setMeasurementMode(MeasurementMode::View);
    emit operationCancelled();
}

void MeasurementStateManager::completeOperation()
{
    LOG_INFO("完成当前操作");
    setMeasurementMode(MeasurementMode::View);
    emit operationCompleted();
} 