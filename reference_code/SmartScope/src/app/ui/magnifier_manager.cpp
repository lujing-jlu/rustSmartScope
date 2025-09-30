#include "app/ui/magnifier_manager.h"
#include "app/ui/magnifier_internal.h"
#include <QDebug>

MagnifierManager::MagnifierManager(QObject *parent)
    : QObject(parent)
{
    // qDebug() << "放大镜管理器初始化";
    // 确保初始状态下放大镜不启用
    m_magnifierEnabled = false;
    
    // 创建内部实现对象
    m_creator = new MagnifierCreator();
    m_renderer = new MagnifierRenderer();
    m_controller = new MagnifierController();
}

MagnifierManager::~MagnifierManager()
{
    // qDebug() << "放大镜管理器销毁开始";
    destroyMagnifier();
    
    // 释放内部实现对象
    delete m_creator;
    delete m_renderer;
    delete m_controller;
    // qDebug() << "放大镜管理器销毁完成";
}

void MagnifierManager::createMagnifier(QWidget* contentWidget, QLabel* leftImageLabel, float leftAreaRatio)
{
    // 如果已存在放大镜容器，先清理
    if (m_magnifierContainer) {
        // qDebug() << "清理已存在的放大镜";
        destroyMagnifier();
    }
    
    // 使用创建器创建放大镜
    bool success = m_creator->create(contentWidget, leftImageLabel, leftAreaRatio, 
                                   &m_magnifierContainer, &m_magnifierLabel, m_magnifierSize);
    
    if (success) {
        // 设置放大镜参数
        m_magnifierEnabled = true;
        
        // 初始化更新放大镜内容
        updateMagnifierContent(leftImageLabel);
        
        // 默认创建后隐藏放大镜，等待用户交互时再显示
        hideMagnifier();
    }
}

void MagnifierManager::updateMagnifierContent(QLabel* leftImageLabel)
{
    // 确保放大镜组件已创建
    if (!m_magnifierContainer || !m_magnifierLabel || !leftImageLabel || !m_magnifierEnabled) {
        return;
    }
    
    // 使用渲染器更新放大镜内容
    m_renderer->updateContent(leftImageLabel, m_magnifierLabel, 
                            m_magnifierContainer, m_magnifierZoom, m_magnifierSize);
}

void MagnifierManager::hideMagnifier()
{
    // 使用控制器隐藏放大镜
    m_controller->hide(m_magnifierContainer);
}

void MagnifierManager::showMagnifier()
{
    // 使用控制器显示放大镜
    if (m_magnifierEnabled) {
        m_controller->show(m_magnifierContainer);
    }
}

void MagnifierManager::destroyMagnifier()
{
    if (m_magnifierContainer) {
        // 使用创建器销毁放大镜
        m_creator->destroy(m_magnifierContainer);
        m_magnifierContainer = nullptr;
        m_magnifierLabel = nullptr;
        m_magnifierEnabled = false;
    }
} 