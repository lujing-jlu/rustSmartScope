#include "infrastructure/file/file_manager.h"
#include "infrastructure/file/directory_watcher.h"
#include "infrastructure/file/file_type_detector.h"
#include "infrastructure/logging/logger.h"
#include "infrastructure/exception/file_exception.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>
#include <QMutexLocker>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QBuffer>
#include <QFileSystemWatcher>
#include <QDirIterator>
#include <QDebug>

#ifndef QT_NO_WIDGETS
#include <QFileDialog>
#endif

#include <QEvent>
#include <QApplication>
#include <QLineEdit>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QInputMethod>
#include <QScreen>
#include <QTimer>
#include <QStyle>
#include <QPushButton>

// 初始化静态成员
std::unique_ptr<FileManager, FileManagerDeleter> FileManager::s_instance;

// 获取单例实例
FileManager& FileManager::instance()
{
    if (!s_instance) {
        s_instance.reset(new FileManager());
    }
    return *s_instance;
}

// 构造函数
FileManager::FileManager()
    : m_workingDir("data")
    , m_tempDir("temp")
    , m_fileTypeDetector(new FileTypeDetector())
{
}

// 析构函数
FileManager::~FileManager()
{
    // 清理所有监视器
    QMutexLocker locker(&m_mutex);
    for (auto it = m_watchers.begin(); it != m_watchers.end(); ++it) {
        delete it.value();
    }
    m_watchers.clear();
}

// 初始化文件管理器
bool FileManager::init(const QString& workingDir, const QString& tempDir, bool createIfNotExist)
{
    QMutexLocker locker(&m_mutex);
    
    m_workingDir = workingDir;
    m_tempDir = tempDir;
    
    // 确保工作目录存在
    QDir workDir(m_workingDir);
    if (!workDir.exists()) {
        if (createIfNotExist) {
            if (!workDir.mkpath(".")) {
                LOG_ERROR(QString("无法创建工作目录: %1").arg(m_workingDir));
                return false;
            }
        } else {
            LOG_ERROR(QString("工作目录不存在: %1").arg(m_workingDir));
            return false;
        }
    }
    
    // 确保临时目录存在
    QDir tempDirObj(m_tempDir);
    if (!tempDirObj.exists()) {
        if (createIfNotExist) {
            if (!tempDirObj.mkpath(".")) {
                LOG_ERROR(QString("无法创建临时目录: %1").arg(m_tempDir));
                return false;
            }
        } else {
            LOG_ERROR(QString("临时目录不存在: %1").arg(m_tempDir));
            return false;
        }
    }
    
    LOG_INFO(QString("文件管理器初始化成功，工作目录: %1，临时目录: %2").arg(m_workingDir).arg(m_tempDir));
    return true;
}

// 读取文本文件
bool FileManager::readTextFile(const QString& filePath, QString& content)
{
    try {
        QFile file(filePath);
        if (!file.exists()) {
            LOG_WARNING(QString("文件不存在: %1").arg(filePath));
            return false;
        }
        
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_ERROR(QString("无法打开文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            return false;
        }
        
        QTextStream in(&file);
        content = in.readAll();
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("读取文本文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 写入文本文件
bool FileManager::writeTextFile(const QString& filePath, const QString& content, bool append)
{
    try {
        QFile file(filePath);
        QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
        if (append) {
            mode |= QIODevice::Append;
        }
        
        if (!file.open(mode)) {
            LOG_ERROR(QString("无法打开文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            return false;
        }
        
        QTextStream out(&file);
        out << content;
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("写入文本文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 读取二进制文件
bool FileManager::readBinaryFile(const QString& filePath, QByteArray& data)
{
    try {
        QFile file(filePath);
        if (!file.exists()) {
            LOG_WARNING(QString("文件不存在: %1").arg(filePath));
            return false;
        }
        
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_ERROR(QString("无法打开文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            return false;
        }
        
        data = file.readAll();
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("读取二进制文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 写入二进制文件
bool FileManager::writeBinaryFile(const QString& filePath, const QByteArray& data, bool append)
{
    try {
        QFile file(filePath);
        QIODevice::OpenMode mode = QIODevice::WriteOnly;
        if (append) {
            mode |= QIODevice::Append;
        }
        
        if (!file.open(mode)) {
            LOG_ERROR(QString("无法打开文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            return false;
        }
        
        qint64 bytesWritten = file.write(data);
        file.close();
        
        if (bytesWritten != data.size()) {
            LOG_ERROR(QString("写入数据不完整: %1, 已写入: %2, 总大小: %3").arg(filePath).arg(bytesWritten).arg(data.size()));
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("写入二进制文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 创建目录
bool FileManager::createDirectory(const QString& dirPath, bool createParents)
{
    try {
        QDir dir(dirPath);
        if (dir.exists()) {
            return true;
        }
        
        if (createParents) {
            return dir.mkpath(".");
        } else {
            return dir.mkdir(".");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("创建目录异常: %1, 错误: %2").arg(dirPath).arg(e.what()));
        return false;
    }
}

// 列出目录内容
bool FileManager::listDirectory(const QString& dirPath, QStringList& entries, const QString& filter)
{
    try {
        QDir dir(dirPath);
        if (!dir.exists()) {
            LOG_WARNING(QString("目录不存在: %1").arg(dirPath));
            return false;
        }
        
        QStringList filters;
        filters << filter;
        dir.setNameFilters(filters);
        
        entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("列出目录内容异常: %1, 错误: %2").arg(dirPath).arg(e.what()));
        return false;
    }
}

// 检查路径是否存在
bool FileManager::exists(const QString& path)
{
    return QFileInfo::exists(path);
}

// 检查路径是否为目录
bool FileManager::isDirectory(const QString& path)
{
    return QFileInfo(path).isDir();
}

// 检查路径是否为文件
bool FileManager::isFile(const QString& path)
{
    return QFileInfo(path).isFile();
}

// 获取文件大小
qint64 FileManager::getFileSize(const QString& filePath)
{
    return QFileInfo(filePath).size();
}

// 获取文件修改时间
QDateTime FileManager::getFileModifiedTime(const QString& filePath)
{
    return QFileInfo(filePath).lastModified();
}

// 复制文件
bool FileManager::copyFile(const QString& sourcePath, const QString& destPath, bool overwrite)
{
    try {
        QFileInfo sourceInfo(sourcePath);
        QFileInfo destInfo(destPath);
        
        // 检查源文件是否存在
        if (!sourceInfo.exists()) {
            throw FileNotFoundException(sourcePath);
        }
        
        // 检查目标文件是否已存在
        if (destInfo.exists()) {
            if (!overwrite) {
                throw FileException(QString("目标文件已存在且不允许覆盖: %1").arg(destPath));
            }
            if (!QFile::remove(destPath)) {
                throw FileException(QString("无法删除已存在的目标文件: %1").arg(destPath));
            }
        }
        
        // 确保目标目录存在
        QDir destDir = destInfo.dir();
        if (!destDir.exists() && !destDir.mkpath(".")) {
            throw FileException(QString("无法创建目标目录: %1").arg(destDir.path()));
        }
        
        // 执行复制操作
        if (!QFile::copy(sourcePath, destPath)) {
            throw FileException(QString("复制文件失败: %1 -> %2").arg(sourcePath, destPath));
        }
        
        return true;
    } catch (const AppException& e) {
        qWarning() << e.what();
        return false;
    } catch (const std::exception& e) {
        qWarning() << "复制文件时发生标准异常:" << e.what();
        return false;
    }
}

// 移动文件
bool FileManager::moveFile(const QString& sourcePath, const QString& destPath, bool overwrite)
{
    try {
        // 先复制文件
        if (!copyFile(sourcePath, destPath, overwrite)) {
            return false;
        }
        
        // 复制成功后删除源文件
        if (!deleteFile(sourcePath)) {
            // 如果删除源文件失败，需要回滚（删除已复制的目标文件）
            deleteFile(destPath);
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        qWarning() << "移动文件时发生标准异常:" << e.what();
        return false;
    }
}

// 删除文件
bool FileManager::deleteFile(const QString& filePath)
{
    try {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            throw FileNotFoundException(filePath);
        }
        
        QFile file(filePath);
        if (!file.remove()) {
            throw FileException(QString("删除文件失败: %1").arg(filePath));
        }
        
        return true;
    } catch (const AppException& e) {
        qWarning() << e.what();
        return false;
    } catch (const std::exception& e) {
        qWarning() << "删除文件时发生标准异常:" << e.what();
        return false;
    }
}

// 删除目录
bool FileManager::deleteDirectory(const QString& dirPath, bool recursive)
{
    try {
        QDir dir(dirPath);
        if (!dir.exists()) {
            LOG_WARNING(QString("目录不存在: %1").arg(dirPath));
            return false;
        }
        
        if (recursive) {
            return dir.removeRecursively();
        } else {
            return dir.rmdir(".");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("删除目录异常: %1, 错误: %2").arg(dirPath).arg(e.what()));
        return false;
    }
}

// 获取文件权限
bool FileManager::getFilePermissions(const QString& path, FilePermissions& permissions)
{
    try {
        QFileInfo fileInfo(path);
        if (!fileInfo.exists()) {
            LOG_WARNING(QString("路径不存在: %1").arg(path));
            return false;
        }
        
        permissions.canRead = fileInfo.isReadable();
        permissions.canWrite = fileInfo.isWritable();
        permissions.canExecute = fileInfo.isExecutable();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("获取文件权限异常: %1, 错误: %2").arg(path).arg(e.what()));
        return false;
    }
}

// 设置文件权限
bool FileManager::setFilePermissions(const QString& path, const FilePermissions& permissions)
{
    try {
        QFile file(path);
        if (!file.exists()) {
            LOG_WARNING(QString("文件不存在: %1").arg(path));
            return false;
        }
        
        QFile::Permissions qtPermissions = QFile::Permissions();
        
        if (permissions.canRead) {
            qtPermissions |= QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther;
        }
        
        if (permissions.canWrite) {
            qtPermissions |= QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther;
        }
        
        if (permissions.canExecute) {
            qtPermissions |= QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther;
        }
        
        return file.setPermissions(qtPermissions);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("设置文件权限异常: %1, 错误: %2").arg(path).arg(e.what()));
        return false;
    }
}

// 创建临时文件
bool FileManager::createTempFile(const QString& prefix, const QString& suffix, QString& filePath)
{
    try {
        QString tempDirPath = getTempDirPath();
        QDir tempDir(tempDirPath);
        
        if (!tempDir.exists() && !tempDir.mkpath(".")) {
            LOG_ERROR(QString("无法创建临时目录: %1").arg(tempDirPath));
            return false;
        }
        
        QString uniqueFileName = generateUniqueFileName(prefix, suffix);
        filePath = tempDir.filePath(uniqueFileName);
        
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            LOG_ERROR(QString("无法创建临时文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            return false;
        }
        
        file.close();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("创建临时文件异常: %1, 错误: %2").arg(prefix).arg(e.what()));
        return false;
    }
}

// 创建临时目录
bool FileManager::createTempDirectory(const QString& prefix, QString& dirPath)
{
    try {
        QString tempDirPath = getTempDirPath();
        QDir tempDir(tempDirPath);
        
        if (!tempDir.exists() && !tempDir.mkpath(".")) {
            LOG_ERROR(QString("无法创建临时目录: %1").arg(tempDirPath));
            return false;
        }
        
        QString uniqueDirName = generateUniqueFileName(prefix, "");
        dirPath = tempDir.filePath(uniqueDirName);
        
        QDir newDir;
        if (!newDir.mkpath(dirPath)) {
            LOG_ERROR(QString("无法创建临时目录: %1").arg(dirPath));
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("创建临时目录异常: %1, 错误: %2").arg(prefix).arg(e.what()));
        return false;
    }
}

// 清理临时文件
bool FileManager::cleanupTempFiles(qint64 olderThan)
{
    try {
        QString tempDirPath = getTempDirPath();
        QDir tempDir(tempDirPath);
        
        if (!tempDir.exists()) {
            return true; // 临时目录不存在，无需清理
        }
        
        QDateTime now = QDateTime::currentDateTime();
        QDateTime cutoff = now.addMSecs(-olderThan);
        
        QFileInfoList entries = tempDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        int removedCount = 0;
        
        for (const QFileInfo& entry : entries) {
            if (entry.lastModified() < cutoff) {
                if (entry.isDir()) {
                    QDir dir(entry.absoluteFilePath());
                    if (dir.removeRecursively()) {
                        removedCount++;
                    } else {
                        LOG_WARNING(QString("无法删除临时目录: %1").arg(entry.absoluteFilePath()));
                    }
                } else {
                    if (QFile::remove(entry.absoluteFilePath())) {
                        removedCount++;
                    } else {
                        LOG_WARNING(QString("无法删除临时文件: %1").arg(entry.absoluteFilePath()));
                    }
                }
            }
        }
        
        LOG_INFO(QString("清理临时文件完成，共删除 %1 个文件/目录").arg(removedCount));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("清理临时文件异常: %1").arg(e.what()));
        return false;
    }
}

// 备份文件
bool FileManager::backupFile(const QString& filePath, QString& backupPath)
{
    try {
        if (!exists(filePath)) {
            LOG_WARNING(QString("文件不存在: %1").arg(filePath));
            return false;
        }
        
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        QString backupFileName = fileName + ".bak";
        backupPath = fileInfo.path() + QDir::separator() + backupFileName;
        
        return copyFile(filePath, backupPath, true);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("备份文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 恢复文件
bool FileManager::restoreFile(const QString& backupPath, const QString& filePath)
{
    try {
        if (!exists(backupPath)) {
            LOG_WARNING(QString("备份文件不存在: %1").arg(backupPath));
            return false;
        }
        
        return copyFile(backupPath, filePath, true);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("恢复文件异常: %1 -> %2, 错误: %3").arg(backupPath).arg(filePath).arg(e.what()));
        return false;
    }
}

// 创建目录监视器
DirectoryWatcher* FileManager::createDirectoryWatcher(const QString& dirPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_watchers.contains(dirPath)) {
        return m_watchers[dirPath];
    }
    
    DirectoryWatcher* watcher = new DirectoryWatcher(dirPath);
    m_watchers[dirPath] = watcher;
    
    return watcher;
}

// 销毁目录监视器
void FileManager::destroyDirectoryWatcher(DirectoryWatcher* watcher)
{
    if (!watcher) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString dirPath = watcher->getDirectoryPath();
    if (m_watchers.contains(dirPath) && m_watchers[dirPath] == watcher) {
        m_watchers.remove(dirPath);
        delete watcher;
    }
}

// 检查文件是否具有指定扩展名
bool FileManager::hasExtension(const QString& filePath, const QStringList& extensions, bool caseSensitive)
{
    QString fileExt = getFileExtension(filePath);
    
    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    
    for (const QString& ext : extensions) {
        if (fileExt.compare(ext, cs) == 0) {
            return true;
        }
    }
    
    return false;
}

// 获取文件扩展名
QString FileManager::getFileExtension(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix();
}

// 获取文件名（不含路径）
QString FileManager::getFileName(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.fileName();
}

// 获取文件名（不含路径和扩展名）
QString FileManager::getFileBaseName(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.baseName();
}

// 获取文件所在目录
QString FileManager::getFileDirectory(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.path();
}

// 获取绝对路径
QString FileManager::getAbsolutePath(const QString& path)
{
    return QFileInfo(path).absoluteFilePath();
}

// 获取相对路径
QString FileManager::getRelativePath(const QString& path, const QString& basePath)
{
    QDir baseDir(basePath.isEmpty() ? QDir::currentPath() : basePath);
    return baseDir.relativeFilePath(path);
}

// 内存映射文件
bool FileManager::mapFile(const QString& filePath, char*& data, qint64& size, bool readOnly)
{
    try {
        QFile file(filePath);
        QIODevice::OpenMode mode = readOnly ? QIODevice::ReadOnly : QIODevice::ReadWrite;
        
        if (!file.open(mode)) {
            LOG_ERROR(QString("无法打开文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            return false;
        }
        
        size = file.size();
        if (size == 0) {
            LOG_WARNING(QString("文件大小为0: %1").arg(filePath));
            file.close();
            data = nullptr;
            return false;
        }
        
        data = (char*)file.map(0, size, readOnly ? QFile::MapPrivateOption : QFile::MapPrivateOption);
        if (!data) {
            LOG_ERROR(QString("无法映射文件: %1, 错误: %2").arg(filePath).arg(file.errorString()));
            file.close();
            return false;
        }
        
        file.close();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("映射文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 解除内存映射
bool FileManager::unmapFile(char* data)
{
    if (!data) {
        return false;
    }
    
    // Qt的QFile::unmap方法是私有的，无法直接调用
    // 在实际应用中，可能需要保存QFile对象以便后续调用unmap
    // 这里简化处理，直接返回true
    return true;
}

// 检测文件类型
FileType FileManager::detectFileType(const QString& filePath)
{
    if (!m_fileTypeDetector) {
        LOG_ERROR("文件类型检测器未初始化");
        return FileType::UNKNOWN;
    }
    
    return m_fileTypeDetector->detectType(filePath);
}

// 获取临时目录路径
QString FileManager::getTempDirPath() const
{
    return m_tempDir;
}

// 生成唯一文件名
QString FileManager::generateUniqueFileName(const QString& prefix, const QString& suffix) const
{
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    
    QString fileName = prefix + uuid + "_" + timestamp;
    if (!suffix.isEmpty()) {
        if (!suffix.startsWith(".")) {
            fileName += ".";
        }
        fileName += suffix;
    }
    
    return fileName;
}

bool FileManager::rename(const QString& oldPath, const QString& newPath)
{
    try {
        QFileInfo oldFileInfo(oldPath);
        QFileInfo newFileInfo(newPath);
        
        // 检查源文件是否存在
        if (!oldFileInfo.exists()) {
            throw FileNotFoundException(oldPath);
        }
        
        // 检查目标路径是否已存在
        if (newFileInfo.exists()) {
            throw FileException(QString("目标路径已存在: %1").arg(newPath));
        }
        
        // 检查目标目录是否存在
        QDir destDir = newFileInfo.dir();
        if (!destDir.exists() && !destDir.mkpath(".")) {
            throw FileException(QString("无法创建目标目录: %1").arg(destDir.path()));
        }
        
        // 执行重命名操作
        if (!QFile::rename(oldPath, newPath)) {
            throw FileException(QString("重命名失败: %1 -> %2").arg(oldPath, newPath));
        }
        
        return true;
    } catch (const AppException& e) {
        qWarning() << e.what();
        return false;
    } catch (const std::exception& e) {
        qWarning() << "重命名文件时发生标准异常:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "重命名文件时发生未知异常";
        return false;
    }
}

CustomFileDialog::CustomFileDialog(QWidget* parent, const QString& caption,
                                 const QString& directory, const QString& filter)
    : QFileDialog(parent, caption, directory, filter)
{
    // 设置窗口标志
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
    setOption(QFileDialog::DontUseNativeDialog);
    
    // 设置对话框大小为屏幕的一半
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    resize(screenGeometry.width() / 2, screenGeometry.height() / 2);
    
    // 设置字体大小
    QFont font = this->font();
    font.setPointSize(font.pointSize() * 1.2);  // 减小字体放大倍数
    setFont(font);
    
    // 设置所有子控件的输入法策略和样式
    QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (qobject_cast<QLineEdit*>(widget)) {
            QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget);
            lineEdit->setMinimumHeight(50);
            lineEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
            lineEdit->setFocusPolicy(Qt::StrongFocus);
            
            // 连接焦点获得信号
            connect(lineEdit, &QLineEdit::cursorPositionChanged, this, [=]() {
                QTimer::singleShot(100, [=]() {
                    if (lineEdit->hasFocus()) {
                        QGuiApplication::inputMethod()->show();
                    }
                });
            });
        }
        
        // 设置按钮样式
        if (QPushButton* button = qobject_cast<QPushButton*>(widget)) {
            // 为按钮设置稍小的字体
            QFont buttonFont = font;
            buttonFont.setPointSize(font.pointSize() * 0.8);  // 进一步减小按钮字体
            button->setFont(buttonFont);
            
            // 特别处理关闭按钮
            if (button->text() == "取消" || button->text() == "关闭" || 
                button->text().contains("Close", Qt::CaseInsensitive)) {
                button->setMinimumSize(120, 60);  // 增加按钮宽度
                button->setStyleSheet(
                    "QPushButton {"
                    "    padding: 10px 20px;"
                    "    margin: 5px;"
                    "    border: 2px solid #666666;"
                    "    border-radius: 8px;"
                    "    background-color: #444444;"
                    "    min-width: 120px;"    // 确保最小宽度
                    "    min-height: 60px;"    // 确保最小高度
                    "}"
                    "QPushButton:hover {"
                    "    background-color: #555555;"
                    "}"
                    "QPushButton:pressed {"
                    "    background-color: #333333;"
                    "}"
                );
            } else {
                button->setMinimumSize(100, 60);  // 其他按钮稍小一点
            }
        }
    }
    
    // 查找并修改标题栏关闭按钮
    if (QDialogButtonBox* buttonBox = findChild<QDialogButtonBox*>()) {
        QList<QAbstractButton*> buttons = buttonBox->buttons();
        for (QAbstractButton* button : buttons) {
            QFont buttonFont = font;
            buttonFont.setPointSize(font.pointSize() * 0.8);  // 进一步减小按钮字体
            button->setFont(buttonFont);
            
            if (button->text() == "取消" || button->text() == "关闭" || 
                button->text().contains("Close", Qt::CaseInsensitive)) {
                button->setMinimumSize(120, 60);  // 增加关闭按钮宽度
            } else {
                button->setMinimumSize(100, 60);  // 其他按钮稍小一点
            }
        }
    }
}

bool CustomFileDialog::event(QEvent* event)
{
    if (event->type() == QEvent::Show) {
        // 确保对话框显示在屏幕上半部分
        QRect screenGeometry = QApplication::primaryScreen()->geometry();
        int dialogY = (screenGeometry.height() - height()) / 4;
        move((screenGeometry.width() - width()) / 2, dialogY);
        
        // 重新设置所有输入控件的输入法策略
        QList<QLineEdit*> lineEdits = findChildren<QLineEdit*>();
        for (QLineEdit* lineEdit : lineEdits) {
            lineEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
            lineEdit->setFocusPolicy(Qt::StrongFocus);
        }
        
        // 确保所有按钮都有正确的大小和字体
        QList<QPushButton*> buttons = findChildren<QPushButton*>();
        for (QPushButton* button : buttons) {
            QFont buttonFont = font();
            buttonFont.setPointSize(buttonFont.pointSize() * 0.8);  // 进一步减小按钮字体
            button->setFont(buttonFont);
            
            if (button->text() == "取消" || button->text() == "关闭" || 
                button->text().contains("Close", Qt::CaseInsensitive)) {
                button->setMinimumSize(120, 60);  // 增加关闭按钮宽度
            } else {
                button->setMinimumSize(100, 60);  // 其他按钮稍小一点
            }
        }
    }
    return QFileDialog::event(event);
}

#ifndef QT_NO_WIDGETS
QString FileManager::getOpenFileName(QWidget* parent, const QString& caption, const QString& dir, 
                                   const QString& filter, QString* selectedFilter, QFileDialog::Options options)
{
    try {
        QString defaultDir = dir.isEmpty() ? m_workingDir : dir;
        CustomFileDialog dialog(parent, caption, defaultDir, filter);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setOptions(options);
        
        if (dialog.exec() == QDialog::Accepted) {
            QStringList files = dialog.selectedFiles();
            return files.isEmpty() ? QString() : files.first();
        }
        return QString();
    } catch (const std::exception& e) {
        qWarning() << "选择文件时发生异常:" << e.what();
        return QString();
    }
}

QStringList FileManager::getOpenFileNames(QWidget* parent, const QString& caption, const QString& dir, 
                                        const QString& filter, QString* selectedFilter, QFileDialog::Options options)
{
    try {
        QString defaultDir = dir.isEmpty() ? m_workingDir : dir;
        CustomFileDialog dialog(parent, caption, defaultDir, filter);
        dialog.setFileMode(QFileDialog::ExistingFiles);
        dialog.setOptions(options);
        
        if (dialog.exec() == QDialog::Accepted) {
            return dialog.selectedFiles();
        }
        return QStringList();
    } catch (const std::exception& e) {
        qWarning() << "选择多个文件时发生异常:" << e.what();
        return QStringList();
    }
}

QString FileManager::getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, 
                                   const QString& filter, QString* selectedFilter, QFileDialog::Options options)
{
    try {
        QString defaultDir = dir.isEmpty() ? m_workingDir : dir;
        CustomFileDialog dialog(parent, caption, defaultDir, filter);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setOptions(options);
        
        if (dialog.exec() == QDialog::Accepted) {
            QStringList files = dialog.selectedFiles();
            return files.isEmpty() ? QString() : files.first();
        }
        return QString();
    } catch (const std::exception& e) {
        qWarning() << "保存文件时发生异常:" << e.what();
        return QString();
    }
}

QString FileManager::getExistingDirectory(QWidget* parent, const QString& caption, const QString& dir, 
                                        QFileDialog::Options options)
{
    try {
        QString defaultDir = dir.isEmpty() ? m_workingDir : dir;
        CustomFileDialog dialog(parent, caption, defaultDir);
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOptions(options | QFileDialog::ShowDirsOnly);
        
        if (dialog.exec() == QDialog::Accepted) {
            QStringList files = dialog.selectedFiles();
            return files.isEmpty() ? QString() : files.first();
        }
        return QString();
    } catch (const std::exception& e) {
        qWarning() << "选择目录时发生异常:" << e.what();
        return QString();
    }
}
#endif

int FileManager::batchDeleteFiles(const QStringList& filePaths)
{
    int successCount = 0;
    for (const QString& filePath : filePaths) {
        try {
            if (deleteFile(filePath)) {
                successCount++;
            }
        } catch (const std::exception& e) {
            qWarning() << "删除文件失败:" << filePath << e.what();
        }
    }
    return successCount;
}

int FileManager::batchCopyFiles(const QStringList& sourcePaths, const QString& destDir, bool overwrite)
{
    int successCount = 0;
    
    // 确保目标目录存在
    QDir dir(destDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "创建目标目录失败:" << destDir;
        return 0;
    }
    
    for (const QString& sourcePath : sourcePaths) {
        try {
            QString fileName = QFileInfo(sourcePath).fileName();
            QString destPath = dir.filePath(fileName);
            
            if (copyFile(sourcePath, destPath, overwrite)) {
                successCount++;
            }
        } catch (const std::exception& e) {
            qWarning() << "复制文件失败:" << sourcePath << e.what();
        }
    }
    return successCount;
}

int FileManager::batchMoveFiles(const QStringList& sourcePaths, const QString& destDir, bool overwrite)
{
    int successCount = 0;
    
    // 确保目标目录存在
    QDir dir(destDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "创建目标目录失败:" << destDir;
        return 0;
    }
    
    for (const QString& sourcePath : sourcePaths) {
        try {
            QString fileName = QFileInfo(sourcePath).fileName();
            QString destPath = dir.filePath(fileName);
            
            if (moveFile(sourcePath, destPath, overwrite)) {
                successCount++;
            }
        } catch (const std::exception& e) {
            qWarning() << "移动文件失败:" << sourcePath << e.what();
        }
    }
    return successCount;
}

QStringList FileManager::searchFiles(const QString& dirPath, const QString& namePattern, bool recursive)
{
    try {
        QDir dir(dirPath);
        if (!dir.exists()) {
            throw FileNotFoundException(dirPath);
        }
        
        QStringList result;
        QStringList filters;
        filters << namePattern;
        
        QDir::Filters flags = QDir::Files | QDir::NoDotAndDotDot;
        if (recursive) {
            flags |= QDir::AllDirs;
        }
        
        QDirIterator it(dirPath, filters, flags,
                       recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        
        while (it.hasNext()) {
            result << it.next();
        }
        
        return result;
    } catch (const std::exception& e) {
        qWarning() << "搜索文件时发生异常:" << e.what();
        return QStringList();
    }
}

QStringList FileManager::searchFileContent(const QString& dirPath, const QString& content, 
                                         const QString& namePattern, bool recursive, bool caseSensitive)
{
    try {
        QStringList result;
        QStringList files = searchFiles(dirPath, namePattern, recursive);
        
        Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        
        for (const QString& filePath : files) {
            QString fileContent;
            if (readTextFile(filePath, fileContent)) {
                if (fileContent.contains(content, cs)) {
                    result << filePath;
                }
            }
        }
        
        return result;
    } catch (const std::exception& e) {
        qWarning() << "搜索文件内容时发生异常:" << e.what();
        return QStringList();
    }
} 