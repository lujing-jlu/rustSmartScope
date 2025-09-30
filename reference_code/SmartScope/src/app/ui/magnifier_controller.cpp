#include "app/ui/magnifier_internal.h"

MagnifierController::MagnifierController()
{
}

MagnifierController::~MagnifierController()
{
}

void MagnifierController::show(QWidget* magnifierContainer)
{
    if (magnifierContainer) {
        magnifierContainer->show();
        magnifierContainer->raise();
        // qDebug() << "放大镜已显示";
    }
}

void MagnifierController::hide(QWidget* magnifierContainer)
{
    if (magnifierContainer) {
        magnifierContainer->hide();
        // qDebug() << "放大镜已隐藏";
    }
} 