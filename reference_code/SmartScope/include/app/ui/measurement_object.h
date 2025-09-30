#ifndef MEASUREMENT_OBJECT_H
#define MEASUREMENT_OBJECT_H

#include <QObject>
#include <QVector3D>
#include <QVector>
#include <QString>
#include <QColor>
#include <QPoint>
#include <QStack>
#include <QPointF>

// 测量类型枚举
enum class MeasurementType {
    Length,         // 长度测量
    PointToLine,    // 点到线距离
    Depth,          // 深度测量 (将用于点到面)
    Area,           // 面积测量
    Polyline,       // 多段线测量
    Profile,        // 剖面测量
    RegionProfile,  // 区域剖面测量
    MissingArea     // 补缺测量 - 计算缺失区域面积
};

// 操作类型枚举，用于历史记录
enum class OperationType {
    Add,            // 添加操作
    Remove,         // 删除操作
    Modify,         // 修改操作
    Clear           // 清空操作
};

// 前向声明
class MeasurementObject;

// 历史记录项结构体
struct HistoryItem {
    OperationType type;              // 操作类型
    MeasurementObject* object;       // 操作对象（拷贝）
    MeasurementObject* originalObject; // 原始对象引用（仅用于Modify操作）
};

// 测量对象类
class MeasurementObject : public QObject {
    Q_OBJECT

public:
    explicit MeasurementObject(QObject *parent = nullptr);
    ~MeasurementObject();
    
    // 克隆当前对象
    MeasurementObject* clone(QObject *parent = nullptr) const;

    // 设置和获取测量类型
    void setType(MeasurementType type);
    MeasurementType getType() const;

    // 设置和获取标记点
    void setPoints(const QVector<QVector3D>& points);
    void addPoint(const QVector3D& point);
    const QVector<QVector3D>& getPoints() const;
    
    // 设置和获取原始2D点击位置
    void setOriginalClickPoints(const QVector<QPoint>& clickPoints);
    void addOriginalClickPoint(const QPoint& clickPoint);
    const QVector<QPoint>& getOriginalClickPoints() const;

    // 设置和获取计算结果
    void setResult(const QString& result);
    QString getResult() const;

    // 设置和获取颜色
    void setColor(const QColor& color);
    QColor getColor() const;

    // 设置和获取是否可见
    void setVisible(bool visible);
    bool isVisible() const;

    // 设置和获取是否选中
    void setSelected(bool selected);
    bool isSelected() const;

    // 设置和获取3D剖面点
    void setProfile3DPoints(const QVector<QVector3D>& points);
    const QVector<QVector3D>& getProfile3DPoints() const;
    
    // 设置和获取剖面图数据
    void setProfileData(const QVector<QPointF>& profileData);
    const QVector<QPointF>& getProfileData() const;

    // 清空测量对象
    void clear();
    
    // 比较两个测量对象是否匹配
    bool matches(const MeasurementObject* other, float tolerance = 0.001f) const;

private:
    MeasurementType m_type;              // 测量类型
    QVector<QVector3D> m_points;         // 标记点数组
    QVector<QPoint> m_originalClickPoints; // 原始2D点击位置
    QString m_result;                    // 计算结果
    QColor m_color;                      // 显示颜色
    bool m_visible;                      // 是否可见
    bool m_selected;                     // 是否选中
    QVector<QVector3D> m_profile3DPoints;  // 3D剖面点数组
    QVector<QPointF> m_profileData;      // 剖面图数据 (X:距离, Y:深度)
};

// 测量对象管理类
class MeasurementManager : public QObject {
    Q_OBJECT

public:
    explicit MeasurementManager(QObject *parent = nullptr);
    ~MeasurementManager();

    // 添加测量对象 - 记录历史
    void addMeasurement(MeasurementObject* measurement, bool recordHistory = true);
    
    // 移除测量对象 - 记录历史
    void removeMeasurement(MeasurementObject* measurement, bool recordHistory = true);
    
    // 获取所有测量对象
    const QVector<MeasurementObject*>& getMeasurements() const;
    
    // 清空所有测量对象 - 记录历史
    void clearMeasurements(bool recordHistory = true);
    
    // 获取指定类型的测量对象
    QVector<MeasurementObject*> getMeasurementsByType(MeasurementType type) const;
    
    // 获取选中的测量对象
    MeasurementObject* getSelectedMeasurement() const;
    
    // 设置选中的测量对象
    void setSelectedMeasurement(MeasurementObject* measurement);
    
    // 历史记录和撤销重做功能
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void clearHistory();
    
    // 查找匹配的测量对象
    MeasurementObject* findMatchingMeasurement(const MeasurementObject* measurement, float tolerance = 0.001f) const;
    
    // 从3D点创建测量对象
    MeasurementObject* createMeasurement(MeasurementType type, const QVector<QVector3D>& points, 
                                        const QVector<QPoint>& clickPoints = QVector<QPoint>());
    
    // 更新测量结果
    void updateMeasurementResult(MeasurementObject* measurement);

signals:
    // 测量对象变化信号
    void measurementsChanged();
    void measurementAdded(MeasurementObject* measurement);
    void measurementRemoved(MeasurementObject* measurement);
    void measurementSelected(MeasurementObject* measurement);
    void historyChanged();
    void undoRedoStateChanged(bool canUndo, bool canRedo);

private:
    // 添加操作到历史记录
    void addToHistory(OperationType type, MeasurementObject* object, MeasurementObject* originalObject = nullptr);
    
    // 计算不同类型测量的结果
    QString calculateLengthMeasurement(const QVector<QVector3D>& points) const;
    QString calculatePointToLineMeasurement(const QVector<QVector3D>& points) const;
    QString calculateDepthMeasurement(const QVector<QVector3D>& points) const;
    QString calculateAreaMeasurement(const QVector<QVector3D>& points) const;
    QString calculatePolylineMeasurement(const QVector<QVector3D>& points) const;
    QString calculateMissingAreaMeasurement(const QVector<QVector3D>& points) const; // 计算补缺测量结果

private:
    QVector<MeasurementObject*> m_measurements;  // 测量对象列表
    MeasurementObject* m_selectedMeasurement;    // 当前选中的测量对象
    
    QStack<HistoryItem> m_undoStack;             // 撤销栈
    QStack<HistoryItem> m_redoStack;             // 重做栈
    bool m_recordingHistory;                     // 是否记录历史（防止撤销重做时重复记录）
    int m_maxHistorySize;                        // 最大历史记录数量
};

#endif // MEASUREMENT_OBJECT_H 