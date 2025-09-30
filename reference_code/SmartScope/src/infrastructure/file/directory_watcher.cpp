#include "infrastructure/file/directory_watcher.h"
#include "infrastructure/logging/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QDateTime>

// 构造函数
DirectoryWatcher::DirectoryWatcher(const QString& dirPath, QObject* parent)
    : QObject(parent)
    , m_dirPath(dirPath)
    , m_watcher(new QFileSystemWatcher(this))
    , m_isWatching(false)
    , m_recursive(false)
{
    // 连接信号槽
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &DirectoryWatcher::onDirectoryChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &DirectoryWatcher::onFileChanged);
}

// 析构函数
DirectoryWatcher::~DirectoryWatcher()
{
    stop();
    delete m_watcher;
}

// 获取监视的目录路径
QString DirectoryWatcher::getDirectoryPath() const
{
    return m_dirPath;
}

// 启动监视
bool DirectoryWatcher::start()
{
    if (m_isWatching) {
        return true; // 已经在监视中
    }
    
    QDir dir(m_dirPath);
    if (!dir.exists()) {
        LOG_ERROR(QString("目录不存在: %1").arg(m_dirPath));
        return false;
    }
    
    // 添加目录到监视列表
    addDirectory(m_dirPath);
    
    // 扫描目录，建立初始文件列表
    updateFileList();
    
    m_isWatching = true;
    LOG_INFO(QString("开始监视目录: %1").arg(m_dirPath));
    
    return true;
}

// 停止监视
void DirectoryWatcher::stop()
{
    if (!m_isWatching) {
        return;
    }
    
    // 清除所有监视路径
    m_watcher->removePaths(m_watcher->directories());
    m_watcher->removePaths(m_watcher->files());
    
    m_fileModifiedTimes.clear();
    m_isWatching = false;
    
    LOG_INFO(QString("停止监视目录: %1").arg(m_dirPath));
}

// 是否正在监视
bool DirectoryWatcher::isWatching() const
{
    return m_isWatching;
}

// 设置是否监视子目录
void DirectoryWatcher::setRecursive(bool recursive)
{
    if (m_recursive != recursive) {
        m_recursive = recursive;
        
        if (m_isWatching) {
            // 重新启动监视以应用新设置
            stop();
            start();
        }
    }
}

// 是否监视子目录
bool DirectoryWatcher::isRecursive() const
{
    return m_recursive;
}

// 设置文件过滤器
void DirectoryWatcher::setFilters(const QStringList& filters)
{
    m_filters = filters;
    
    if (m_isWatching) {
        // 重新启动监视以应用新设置
        stop();
        start();
    }
}

// 获取文件过滤器
QStringList DirectoryWatcher::getFilters() const
{
    return m_filters;
}

// 目录变化槽函数
void DirectoryWatcher::onDirectoryChanged(const QString& path)
{
    if (!m_isWatching) {
        return;
    }
    
    LOG_DEBUG(QString("检测到目录变化: %1").arg(path));
    
    // 延迟处理，避免频繁触发
    QTimer::singleShot(100, this, &DirectoryWatcher::scanDirectory);
}

// 文件变化槽函数
void DirectoryWatcher::onFileChanged(const QString& path)
{
    if (!m_isWatching) {
        return;
    }
    
    LOG_DEBUG(QString("检测到文件变化: %1").arg(path));
    
    QFileInfo fileInfo(path);
    
    if (fileInfo.exists()) {
        // 文件修改
        QDateTime lastModified = fileInfo.lastModified();
        
        if (m_fileModifiedTimes.contains(path)) {
            QDateTime prevModified = m_fileModifiedTimes[path];
            
            if (lastModified > prevModified) {
                m_fileModifiedTimes[path] = lastModified;
                emit fileModified(path);
            }
        } else {
            // 新文件
            m_fileModifiedTimes[path] = lastModified;
            emit fileCreated(path);
        }
        
        // 重新添加到监视列表（某些系统在文件修改后会自动移除监视）
        if (!m_watcher->files().contains(path)) {
            m_watcher->addPath(path);
        }
    } else {
        // 文件删除
        m_fileModifiedTimes.remove(path);
        emit fileDeleted(path);
    }
}

// 扫描目录槽函数
void DirectoryWatcher::scanDirectory()
{
    if (!m_isWatching) {
        return;
    }
    
    updateFileList();
}

// 添加目录到监视列表
void DirectoryWatcher::addDirectory(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }
    
    // 添加目录到监视列表
    if (!m_watcher->directories().contains(path)) {
        m_watcher->addPath(path);
    }
    
    // 如果启用了递归监视，添加所有子目录
    if (m_recursive) {
        QFileInfoList dirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& info : dirList) {
            addDirectory(info.absoluteFilePath());
        }
    }
}

// 添加文件到监视列表
void DirectoryWatcher::addFile(const QString& path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return;
    }
    
    // 检查文件是否匹配过滤器
    if (!matchesFilter(path)) {
        return;
    }
    
    // 添加文件到监视列表
    if (!m_watcher->files().contains(path)) {
        m_watcher->addPath(path);
    }
    
    // 记录文件修改时间
    m_fileModifiedTimes[path] = fileInfo.lastModified();
}

// 更新文件列表
void DirectoryWatcher::updateFileList()
{
    QDir dir(m_dirPath);
    if (!dir.exists()) {
        return;
    }
    
    // 获取当前所有文件
    QMap<QString, QDateTime> currentFiles;
    
    // 处理当前目录中的文件
    QFileInfoList fileList = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& info : fileList) {
        QString filePath = info.absoluteFilePath();
        
        if (matchesFilter(filePath)) {
            currentFiles[filePath] = info.lastModified();
            
            // 检查是否为新文件
            if (!m_fileModifiedTimes.contains(filePath)) {
                m_fileModifiedTimes[filePath] = info.lastModified();
                emit fileCreated(filePath);
            }
            
            // 添加到监视列表
            if (!m_watcher->files().contains(filePath)) {
                m_watcher->addPath(filePath);
            }
        }
    }
    
    // 处理当前目录中的子目录
    if (m_recursive) {
        QFileInfoList dirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        // 检查是否有新目录
        for (const QFileInfo& info : dirList) {
            QString dirPath = info.absoluteFilePath();
            
            if (!m_watcher->directories().contains(dirPath)) {
                emit directoryCreated(dirPath);
                addDirectory(dirPath);
                
                // 递归处理子目录中的文件
                QDir subDir(dirPath);
                QFileInfoList subFileList = subDir.entryInfoList(QDir::Files);
                for (const QFileInfo& subInfo : subFileList) {
                    QString subFilePath = subInfo.absoluteFilePath();
                    
                    if (matchesFilter(subFilePath)) {
                        currentFiles[subFilePath] = subInfo.lastModified();
                        
                        // 检查是否为新文件
                        if (!m_fileModifiedTimes.contains(subFilePath)) {
                            m_fileModifiedTimes[subFilePath] = subInfo.lastModified();
                            emit fileCreated(subFilePath);
                        }
                        
                        // 添加到监视列表
                        if (!m_watcher->files().contains(subFilePath)) {
                            m_watcher->addPath(subFilePath);
                        }
                    }
                }
            }
        }
    }
    
    // 检查是否有文件被删除
    QStringList deletedFiles;
    for (auto it = m_fileModifiedTimes.begin(); it != m_fileModifiedTimes.end(); ++it) {
        if (!currentFiles.contains(it.key())) {
            deletedFiles.append(it.key());
        }
    }
    
    // 处理删除的文件
    for (const QString& filePath : deletedFiles) {
        m_fileModifiedTimes.remove(filePath);
        emit fileDeleted(filePath);
    }
}

// 检查文件是否匹配过滤器
bool DirectoryWatcher::matchesFilter(const QString& filePath) const
{
    if (m_filters.isEmpty()) {
        return true; // 没有过滤器，匹配所有文件
    }
    
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    for (const QString& filter : m_filters) {
        if (filter == "*") {
            return true; // 通配符，匹配所有文件
        }
        
        if (filter.startsWith("*.")) {
            // 扩展名过滤器
            QString filterSuffix = filter.mid(2).toLower();
            if (suffix == filterSuffix) {
                return true;
            }
        } else if (fileInfo.fileName().contains(filter, Qt::CaseInsensitive)) {
            // 文件名包含过滤字符串
            return true;
        }
    }
    
    return false;
} 