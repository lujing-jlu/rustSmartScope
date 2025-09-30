#include "app/ui/measurement_object.h"
#include "infrastructure/logging/logger.h"
#include <QDebug>
#include <cmath>

// MeasurementObject 实现
MeasurementObject::MeasurementObject(QObject *parent)
    : QObject(parent)
    , m_type(MeasurementType::Length)
    , m_color(Qt::red)
    , m_visible(true)
    , m_selected(false)
{
    LOG_INFO("创建新的测量对象");
}

MeasurementObject::~MeasurementObject()
{
    LOG_INFO("销毁测量对象");
}

// 克隆当前对象
MeasurementObject* MeasurementObject::clone(QObject *parent) const
{
    MeasurementObject* clone = new MeasurementObject(parent ? parent : this->parent());
    clone->setType(m_type);
    clone->setPoints(m_points);
    clone->setOriginalClickPoints(m_originalClickPoints);
    clone->setResult(m_result);
    clone->setColor(m_color);
    clone->setVisible(m_visible);
    clone->setSelected(m_selected);
    return clone;
}

void MeasurementObject::setType(MeasurementType type)
{
    if (m_type != type) {
        m_type = type;
        LOG_INFO(QString("设置测量类型: %1").arg(static_cast<int>(type)));
    }
}

MeasurementType MeasurementObject::getType() const
{
    return m_type;
}

void MeasurementObject::setPoints(const QVector<QVector3D>& points)
{
    m_points = points;
    LOG_INFO(QString("设置测量点，数量: %1").arg(points.size()));
}

void MeasurementObject::addPoint(const QVector3D& point)
{
    m_points.append(point);
    LOG_INFO(QString("添加测量点: (%1, %2, %3)")
             .arg(point.x()).arg(point.y()).arg(point.z()));
}

const QVector<QVector3D>& MeasurementObject::getPoints() const
{
    return m_points;
}

void MeasurementObject::setOriginalClickPoints(const QVector<QPoint>& clickPoints)
{
    m_originalClickPoints = clickPoints;
    LOG_INFO(QString("设置原始点击位置，数量: %1").arg(clickPoints.size()));
}

void MeasurementObject::addOriginalClickPoint(const QPoint& clickPoint)
{
    m_originalClickPoints.append(clickPoint);
    LOG_INFO(QString("添加原始点击位置: (%1, %2)")
             .arg(clickPoint.x()).arg(clickPoint.y()));
}

const QVector<QPoint>& MeasurementObject::getOriginalClickPoints() const
{
    return m_originalClickPoints;
}

void MeasurementObject::setResult(const QString& result)
{
    if (m_result != result) {
        m_result = result;
        LOG_INFO(QString("设置测量结果: %1").arg(result));
    }
}

QString MeasurementObject::getResult() const
{
    return m_result;
}

void MeasurementObject::setColor(const QColor& color)
{
    if (m_color != color) {
        m_color = color;
        LOG_INFO(QString("设置测量颜色: %1").arg(color.name()));
    }
}

QColor MeasurementObject::getColor() const
{
    return m_color;
}

void MeasurementObject::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        LOG_INFO(QString("设置测量可见性: %1").arg(visible));
    }
}

bool MeasurementObject::isVisible() const
{
    return m_visible;
}

void MeasurementObject::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        LOG_INFO(QString("设置测量选中状态: %1").arg(selected));
    }
}

bool MeasurementObject::isSelected() const
{
    return m_selected;
}

void MeasurementObject::clear()
{
    m_points.clear();
    m_result.clear();
    m_visible = true;
    m_selected = false;
    LOG_INFO("清空测量对象");
}

// 判断两个测量对象是否匹配
bool MeasurementObject::matches(const MeasurementObject* other, float tolerance) const
{
    if (!other || m_type != other->getType()) {
        return false;
    }
    
    const QVector<QVector3D>& otherPoints = other->getPoints();
    
    // 点数不同，不匹配
    if (m_points.size() != otherPoints.size()) {
        return false;
    }
    
    // 比较每个点
    for (int i = 0; i < m_points.size(); i++) {
        const QVector3D& p1 = m_points[i];
        const QVector3D& p2 = otherPoints[i];
        
        // 检查点是否在允许的误差范围内
        if (std::abs(p1.x() - p2.x()) > tolerance ||
            std::abs(p1.y() - p2.y()) > tolerance ||
            std::abs(p1.z() - p2.z()) > tolerance) {
            return false;
        }
    }
    
    return true;
}

// MeasurementManager 实现
MeasurementManager::MeasurementManager(QObject *parent)
    : QObject(parent)
    , m_selectedMeasurement(nullptr)
    , m_recordingHistory(true)
    , m_maxHistorySize(50)  // 默认保存最多50步历史记录
{
    LOG_INFO("创建测量管理器");
}

MeasurementManager::~MeasurementManager()
{
    clearMeasurements(false);
    clearHistory();
    LOG_INFO("销毁测量管理器");
}

void MeasurementManager::addMeasurement(MeasurementObject* measurement, bool recordHistory)
{
    if (measurement && !m_measurements.contains(measurement)) {
        // 记录历史
        if (recordHistory && m_recordingHistory) {
            LOG_INFO(QString("记录添加操作到历史: recordHistory=%1, m_recordingHistory=%2")
                   .arg(recordHistory).arg(m_recordingHistory));
            addToHistory(OperationType::Add, measurement);
        } else {
            LOG_INFO(QString("不记录历史: recordHistory=%1, m_recordingHistory=%2")
                   .arg(recordHistory).arg(m_recordingHistory));
        }
        
        // 添加到列表
        m_measurements.append(measurement);
        LOG_INFO(QString("添加测量对象到管理器，当前对象数: %1").arg(m_measurements.size()));
        
        // 发送信号
        emit measurementsChanged();
        emit measurementAdded(measurement);
    }
}

void MeasurementManager::removeMeasurement(MeasurementObject* measurement, bool recordHistory)
{
    if (measurement && m_measurements.contains(measurement)) {
        // 记录历史
        if (recordHistory && m_recordingHistory) {
            LOG_INFO(QString("记录删除操作到历史: recordHistory=%1, m_recordingHistory=%2")
                   .arg(recordHistory).arg(m_recordingHistory));
            addToHistory(OperationType::Remove, measurement);
        } else {
            LOG_INFO(QString("不记录历史: recordHistory=%1, m_recordingHistory=%2")
                   .arg(recordHistory).arg(m_recordingHistory));
        }
        
        // 从列表中移除
        m_measurements.removeOne(measurement);
        if (m_selectedMeasurement == measurement) {
            m_selectedMeasurement = nullptr;
        }
        
        // 发送信号
        emit measurementsChanged();
        emit measurementRemoved(measurement);
        
        // 最后删除对象（不要提前删除）
        delete measurement;
        LOG_INFO(QString("从管理器移除测量对象，当前对象数: %1").arg(m_measurements.size()));
    }
}

const QVector<MeasurementObject*>& MeasurementManager::getMeasurements() const
{
    return m_measurements;
}

void MeasurementManager::clearMeasurements(bool recordHistory)
{
    if (m_measurements.isEmpty()) {
        return;
    }
    
    // 记录历史
    if (recordHistory && m_recordingHistory) {
        // 为了能够恢复所有对象，需要将每个对象都添加到历史记录中
        for (auto measurement : m_measurements) {
            addToHistory(OperationType::Remove, measurement);
        }
    }
    
    // 清空对象
    for (auto measurement : m_measurements) {
        delete measurement;
    }
    m_measurements.clear();
    m_selectedMeasurement = nullptr;
    
    LOG_INFO("清空所有测量对象");
    emit measurementsChanged();
}

QVector<MeasurementObject*> MeasurementManager::getMeasurementsByType(MeasurementType type) const
{
    QVector<MeasurementObject*> result;
    for (auto measurement : m_measurements) {
        if (measurement->getType() == type) {
            result.append(measurement);
        }
    }
    return result;
}

MeasurementObject* MeasurementManager::getSelectedMeasurement() const
{
    return m_selectedMeasurement;
}

void MeasurementManager::setSelectedMeasurement(MeasurementObject* measurement)
{
    if (m_selectedMeasurement != measurement) {
        if (m_selectedMeasurement) {
            m_selectedMeasurement->setSelected(false);
        }
        m_selectedMeasurement = measurement;
        if (measurement) {
            measurement->setSelected(true);
        }
        LOG_INFO("设置选中的测量对象");
        emit measurementSelected(measurement);
    }
}

// 历史记录和撤销重做功能
bool MeasurementManager::canUndo() const
{
    bool result = !m_undoStack.isEmpty();
    LOG_INFO(QString("检查是否可撤销: %1, 历史记录数: %2").arg(result).arg(m_undoStack.size()));
    return result;
}

bool MeasurementManager::canRedo() const
{
    bool result = !m_redoStack.isEmpty();
    LOG_INFO(QString("检查是否可重做: %1, 重做记录数: %2").arg(result).arg(m_redoStack.size()));
    return result;
}

bool MeasurementManager::undo()
{
    if (!canUndo()) {
        LOG_INFO("无法撤销: 撤销栈为空");
        return false;
    }
    
    // 暂时停止记录历史，避免循环
    m_recordingHistory = false;
    
    // 取出最后一个历史项
    HistoryItem item = m_undoStack.pop();
    LOG_INFO(QString("从历史记录中取出操作: 类型=%1, 剩余历史记录数=%2")
           .arg(static_cast<int>(item.type))
           .arg(m_undoStack.size()));
    
    // 根据操作类型执行相反的操作
    switch (item.type) {
        case OperationType::Add:
            // 撤销添加 = 删除
            {
                LOG_INFO("撤销添加操作: 查找匹配对象");
                MeasurementObject* objToRemove = findMatchingMeasurement(item.object);
                if (objToRemove) {
                    LOG_INFO("找到匹配对象，执行删除");
                    removeMeasurement(objToRemove, false);
                } else {
                    LOG_INFO("未找到匹配对象，撤销失败");
                }
            }
            break;
            
        case OperationType::Remove:
            // 撤销删除 = 添加
            {
                LOG_INFO("撤销删除操作: 创建新对象");
                MeasurementObject* newObj = item.object->clone(this);
                addMeasurement(newObj, false);
                LOG_INFO("创建并添加新对象完成");
            }
            break;
            
        case OperationType::Modify:
            // 撤销修改 = 恢复原始状态
            {
                // 找到被修改的对象
                if (item.originalObject) {
                    LOG_INFO("撤销修改操作: 查找匹配对象");
                    MeasurementObject* objToRestore = findMatchingMeasurement(item.object);
                    if (objToRestore) {
                        LOG_INFO("找到匹配对象，执行恢复");
                        // 创建还原对象
                        MeasurementObject* restoreObj = item.originalObject->clone(this);
                        // 移除当前对象
                        m_measurements.removeOne(objToRestore);
                        // 添加还原后的对象
                        m_measurements.append(restoreObj);
                        // 更新选中状态
                        if (m_selectedMeasurement == objToRestore) {
                            setSelectedMeasurement(restoreObj);
                        }
                        delete objToRestore;
                        emit measurementsChanged();
                        LOG_INFO("对象恢复完成");
                    } else {
                        LOG_INFO("未找到匹配对象，撤销失败");
                    }
                } else {
                    LOG_INFO("原始对象为空，撤销失败");
                }
            }
            break;
            
        case OperationType::Clear:
            // 撤销清空 = 恢复所有对象
            // 这里暂不实现，因为比较复杂
            LOG_INFO("撤销清空操作: 不支持");
            break;
    }
    
    // 添加到重做栈
    m_redoStack.push(item);
    
    // 恢复记录历史
    m_recordingHistory = true;
    
    // 发送信号
    emit historyChanged();
    emit undoRedoStateChanged(canUndo(), canRedo());
    emit measurementsChanged(); // 确保发送测量对象变化信号
    
    LOG_INFO("撤销操作完成");
    return true;
}

bool MeasurementManager::redo()
{
    if (!canRedo()) {
        return false;
    }
    
    // 暂时停止记录历史，避免循环
    m_recordingHistory = false;
    
    // 取出重做栈顶的历史项
    HistoryItem item = m_redoStack.pop();
    
    // 根据操作类型执行原始操作
    switch (item.type) {
        case OperationType::Add:
            // 重做添加 = 添加
            {
                MeasurementObject* newObj = item.object->clone(this);
                addMeasurement(newObj, false);
            }
            break;
            
        case OperationType::Remove:
            // 重做删除 = 删除
            {
                MeasurementObject* objToRemove = findMatchingMeasurement(item.object);
                if (objToRemove) {
                    removeMeasurement(objToRemove, false);
                }
            }
            break;
            
        case OperationType::Modify:
            // 重做修改 = 应用修改后的状态
            {
                // 找到要修改的对象
                MeasurementObject* objToModify = findMatchingMeasurement(item.originalObject);
                if (objToModify) {
                    // 创建修改后的对象
                    MeasurementObject* modifiedObj = item.object->clone(this);
                    // 移除当前对象
                    m_measurements.removeOne(objToModify);
                    // 添加修改后的对象
                    m_measurements.append(modifiedObj);
                    // 更新选中状态
                    if (m_selectedMeasurement == objToModify) {
                        setSelectedMeasurement(modifiedObj);
                    }
                    delete objToModify;
                    emit measurementsChanged();
                }
            }
            break;
            
        case OperationType::Clear:
            // 重做清空 = 清空
            clearMeasurements(false);
            break;
    }
    
    // 添加到撤销栈
    m_undoStack.push(item);
    
    // 清空重做栈
    while (!m_redoStack.isEmpty()) {
        HistoryItem redoItem = m_redoStack.pop();
        delete redoItem.object;
    }
    
    // 限制历史记录大小
    while (m_undoStack.size() > m_maxHistorySize) {
        HistoryItem oldItem = m_undoStack.takeFirst();
        delete oldItem.object;
    }
    
    // 恢复记录历史
    m_recordingHistory = true;
    
    // 发送信号
    emit historyChanged();
    emit undoRedoStateChanged(canUndo(), canRedo());
    
    return true;
}

void MeasurementManager::clearHistory()
{
    // 清空撤销栈
    while (!m_undoStack.isEmpty()) {
        HistoryItem item = m_undoStack.pop();
        delete item.object;
        // 不要删除originalObject，因为它是指向实际对象的指针
    }
    
    // 清空重做栈
    while (!m_redoStack.isEmpty()) {
        HistoryItem item = m_redoStack.pop();
        delete item.object;
        // 不要删除originalObject，因为它是指向实际对象的指针
    }
    
    // 发送信号
    emit historyChanged();
    emit undoRedoStateChanged(false, false);
}

// 添加操作到历史记录
void MeasurementManager::addToHistory(OperationType type, MeasurementObject* object, MeasurementObject* originalObject)
{
    if (!object) {
        LOG_INFO("添加历史记录失败: 对象为空");
        return;
    }
    
    // 添加到历史记录
    HistoryItem item;
    item.type = type;
    item.object = object->clone();
    item.originalObject = originalObject;
    
    // 添加到撤销栈
    m_undoStack.push(item);
    
    LOG_INFO(QString("添加操作到历史记录: 类型=%1, 当前历史记录数=%2")
           .arg(static_cast<int>(type))
           .arg(m_undoStack.size()));
    
    // 清空重做栈
    while (!m_redoStack.isEmpty()) {
        HistoryItem redoItem = m_redoStack.pop();
        delete redoItem.object;
    }
    
    // 限制历史记录大小
    while (m_undoStack.size() > m_maxHistorySize) {
        HistoryItem oldItem = m_undoStack.takeFirst();
        delete oldItem.object;
    }
    
    // 发送信号
    emit historyChanged();
    emit undoRedoStateChanged(canUndo(), canRedo());
}

// 查找匹配的测量对象
MeasurementObject* MeasurementManager::findMatchingMeasurement(const MeasurementObject* measurement, float tolerance) const
{
    if (!measurement) {
        return nullptr;
    }
    
    // 从后向前搜索（通常最近添加的对象更可能被操作）
    for (int i = m_measurements.size() - 1; i >= 0; --i) {
        if (m_measurements[i]->matches(measurement, tolerance)) {
            return m_measurements[i];
        }
    }
    
    return nullptr;
}

// 从3D点创建测量对象
MeasurementObject* MeasurementManager::createMeasurement(MeasurementType type, const QVector<QVector3D>& points, const QVector<QPoint>& clickPoints)
{
    // 创建新的测量对象
    MeasurementObject* measurement = new MeasurementObject(this);
    measurement->setType(type);
    measurement->setPoints(points);
    
    // 设置点击位置
    if (!clickPoints.isEmpty()) {
        measurement->setOriginalClickPoints(clickPoints);
    }
    
    // 更新测量结果
    updateMeasurementResult(measurement);
    
    return measurement;
}

// 更新测量结果
void MeasurementManager::updateMeasurementResult(MeasurementObject* measurement)
{
    if (!measurement) {
        return;
    }
    
    QString result;
    const QVector<QVector3D>& points = measurement->getPoints();
    
    // 根据测量类型计算结果
    switch (measurement->getType()) {
        case MeasurementType::Length:
            result = calculateLengthMeasurement(points);
            break;
            
        case MeasurementType::PointToLine:
            result = calculatePointToLineMeasurement(points);
            break;
            
        case MeasurementType::Depth:
            // 深度测量结果已在 ImageInteractionManager 中计算并设置
            // 避免重复计算，直接使用已设置的结果
            result = measurement->getResult();
            if (result.isEmpty()) {
                // 如果结果为空，则进行计算（兼容性处理）
                result = calculateDepthMeasurement(points);
            }
            break;
            
        case MeasurementType::Area:
            result = calculateAreaMeasurement(points);
            break;
            
        case MeasurementType::Polyline:
            result = calculatePolylineMeasurement(points);
            break;
            
        case MeasurementType::MissingArea:
            result = calculateMissingAreaMeasurement(points);
            break;
            
        default:
            result = "未计算";
            break;
    }
    
    // 更新测量结果
    measurement->setResult(result);
}

// 计算长度测量结果
QString MeasurementManager::calculateLengthMeasurement(const QVector<QVector3D>& points) const
{
    if (points.size() < 2) {
        return "点数不足";
    }
    
    // 计算两点之间的距离
    float distance = (points[1] - points[0]).length();
    return QString("%1 mm").arg(distance, 0, 'f', 2);
}

// 计算点到线距离
QString MeasurementManager::calculatePointToLineMeasurement(const QVector<QVector3D>& points) const
{
    if (points.size() < 3) {
        return "点数不足";
    }
    
    const QVector3D& point = points[0];
    const QVector3D& lineStart = points[1];
    const QVector3D& lineEnd = points[2];
    
    // 计算线段向量
    QVector3D lineVec = lineEnd - lineStart;
    
    // 计算点到线段的向量
    QVector3D pointVec = point - lineStart;
    
    // 计算投影
    float t = QVector3D::dotProduct(pointVec, lineVec) / QVector3D::dotProduct(lineVec, lineVec);
    
    // 限制t在0和1之间，确保是到线段而不是到直线的距离
    t = qBound(0.0f, t, 1.0f);
    
    // 计算投影点
    QVector3D projection = lineStart + t * lineVec;
    
    // 计算点到投影点的距离
    float distance = (point - projection).length();
    
    return QString("%1 mm").arg(distance, 0, 'f', 2);
}

// 计算深度测量
QString MeasurementManager::calculateDepthMeasurement(const QVector<QVector3D>& points) const
{
    if (points.size() < 2) {
        return "点数不足";
    }
    
    // 计算Z轴方向的差值
    float depthDiff = std::abs(points[1].z() - points[0].z());
    return QString("%1 mm").arg(depthDiff, 0, 'f', 2);
}

// 计算面积测量
QString MeasurementManager::calculateAreaMeasurement(const QVector<QVector3D>& points) const
{
    if (points.size() < 3) {
        return "点数不足";
    }
    
    // 简单三角形面积计算
    if (points.size() == 3) {
        QVector3D edge1 = points[1] - points[0];
        QVector3D edge2 = points[2] - points[0];
        
        // 计算叉积的长度除以2得到面积
        float area = QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
        return QString("%1 mm²").arg(area, 0, 'f', 2);
    }
    
    // 多边形面积计算 - 将多边形分解为三角形
    float area = 0.0f;
    for (int i = 1; i < points.size() - 1; i++) {
        QVector3D edge1 = points[i] - points[0];
        QVector3D edge2 = points[i+1] - points[0];
        area += QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
    }
    
    return QString("%1 mm²").arg(area, 0, 'f', 2);
}

// 计算多段线长度
QString MeasurementManager::calculatePolylineMeasurement(const QVector<QVector3D>& points) const
{
    if (points.size() < 2) {
        return "点数不足";
    }
    
    // 计算所有线段长度之和
    float totalLength = 0.0f;
    for (int i = 0; i < points.size() - 1; i++) {
        totalLength += (points[i+1] - points[i]).length();
    }
    
    return QString("%1 mm").arg(totalLength, 0, 'f', 2);
}

// --- 再次添加：setProfile3DPoints 实现 ---
void MeasurementObject::setProfile3DPoints(const QVector<QVector3D>& points)
{
    m_profile3DPoints = points;
    LOG_INFO(QString("设置3D剖面点，数量: %1").arg(points.size()));
}
// --- 结束添加 ---

// --- 再次添加：getProfile3DPoints 实现 ---
const QVector<QVector3D>& MeasurementObject::getProfile3DPoints() const
{
    return m_profile3DPoints;
}
// --- 结束添加 ---

QString MeasurementManager::calculateMissingAreaMeasurement(const QVector<QVector3D>& points) const
{
    // 缺失面积测量需要至少3个点：交点+至少2个后续点构成多边形
    if (points.size() < 3) {
        return "点数不足，需要至少3个点";
    }

    // 传入的points是多边形点（交点+后续点），第一个点是交点
    float area = 0.0f;

    if (points.size() == 3) {
        // 三角形面积计算
        QVector3D edge1 = points[1] - points[0];
        QVector3D edge2 = points[2] - points[0];
        area = QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
    } else {
        // 多边形面积计算 - 将多边形分解为三角形
        // 以第一个点（交点）为公共顶点，将多边形分解为多个三角形
        QVector3D intersectionPoint = points[0]; // 交点是第一个点

        for (int i = 1; i < points.size() - 1; i++) {
            QVector3D edge1 = points[i] - intersectionPoint;
            QVector3D edge2 = points[i+1] - intersectionPoint;
            area += QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
        }

        // 连接最后一个点和第二个点形成闭合多边形
        if (points.size() > 2) {
            QVector3D edge1 = points[points.size()-1] - intersectionPoint;
            QVector3D edge2 = points[1] - intersectionPoint;
            area += QVector3D::crossProduct(edge1, edge2).length() / 2.0f;
        }
    }

    return QString("%1 mm²").arg(area, 0, 'f', 2);
}

void MeasurementObject::setProfileData(const QVector<QPointF>& profileData)
{
    m_profileData = profileData;
    LOG_INFO(QString("设置剖面图数据，数量: %1").arg(profileData.size()));
}

const QVector<QPointF>& MeasurementObject::getProfileData() const
{
    return m_profileData;
} 