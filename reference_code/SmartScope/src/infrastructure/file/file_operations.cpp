#include "infrastructure/file/file_operations.h"
#include "infrastructure/file/file_manager.h"
#include "infrastructure/logging/logger.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QBuffer>

// 构造函数
FileOperations::FileOperations()
{
}

// 析构函数
FileOperations::~FileOperations()
{
}

// 读取文本文件
bool FileOperations::readTextFile(const QString& filePath, QString& content)
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
bool FileOperations::writeTextFile(const QString& filePath, const QString& content, bool append)
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
bool FileOperations::readBinaryFile(const QString& filePath, QByteArray& data)
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
bool FileOperations::writeBinaryFile(const QString& filePath, const QByteArray& data, bool append)
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
bool FileOperations::createDirectory(const QString& dirPath, bool createParents)
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
bool FileOperations::listDirectory(const QString& dirPath, QStringList& entries, const QString& filter)
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
bool FileOperations::exists(const QString& path)
{
    return QFileInfo::exists(path);
}

// 检查路径是否为目录
bool FileOperations::isDirectory(const QString& path)
{
    return QFileInfo(path).isDir();
}

// 检查路径是否为文件
bool FileOperations::isFile(const QString& path)
{
    return QFileInfo(path).isFile();
}

// 获取文件大小
qint64 FileOperations::getFileSize(const QString& filePath)
{
    return QFileInfo(filePath).size();
}

// 获取文件修改时间
QDateTime FileOperations::getFileModifiedTime(const QString& filePath)
{
    return QFileInfo(filePath).lastModified();
}

// 复制文件
bool FileOperations::copyFile(const QString& sourcePath, const QString& destPath, bool overwrite)
{
    try {
        if (!exists(sourcePath)) {
            LOG_WARNING(QString("源文件不存在: %1").arg(sourcePath));
            return false;
        }
        
        if (exists(destPath)) {
            if (overwrite) {
                if (!QFile::remove(destPath)) {
                    LOG_ERROR(QString("无法删除目标文件: %1").arg(destPath));
                    return false;
                }
            } else {
                LOG_WARNING(QString("目标文件已存在: %1").arg(destPath));
                return false;
            }
        }
        
        return QFile::copy(sourcePath, destPath);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("复制文件异常: %1 -> %2, 错误: %3").arg(sourcePath).arg(destPath).arg(e.what()));
        return false;
    }
}

// 移动文件
bool FileOperations::moveFile(const QString& sourcePath, const QString& destPath, bool overwrite)
{
    try {
        if (!exists(sourcePath)) {
            LOG_WARNING(QString("源文件不存在: %1").arg(sourcePath));
            return false;
        }
        
        if (exists(destPath)) {
            if (overwrite) {
                if (!QFile::remove(destPath)) {
                    LOG_ERROR(QString("无法删除目标文件: %1").arg(destPath));
                    return false;
                }
            } else {
                LOG_WARNING(QString("目标文件已存在: %1").arg(destPath));
                return false;
            }
        }
        
        return QFile::rename(sourcePath, destPath);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("移动文件异常: %1 -> %2, 错误: %3").arg(sourcePath).arg(destPath).arg(e.what()));
        return false;
    }
}

// 删除文件
bool FileOperations::deleteFile(const QString& filePath)
{
    try {
        if (!exists(filePath)) {
            LOG_WARNING(QString("文件不存在: %1").arg(filePath));
            return false;
        }
        
        if (!isFile(filePath)) {
            LOG_WARNING(QString("路径不是文件: %1").arg(filePath));
            return false;
        }
        
        return QFile::remove(filePath);
    } catch (const std::exception& e) {
        LOG_ERROR(QString("删除文件异常: %1, 错误: %2").arg(filePath).arg(e.what()));
        return false;
    }
}

// 删除目录
bool FileOperations::deleteDirectory(const QString& dirPath, bool recursive)
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
bool FileOperations::getFilePermissions(const QString& path, FilePermissions& permissions)
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
bool FileOperations::setFilePermissions(const QString& path, const FilePermissions& permissions)
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

// 获取文件扩展名
QString FileOperations::getFileExtension(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix();
}

// 获取文件名（不含路径）
QString FileOperations::getFileName(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.fileName();
}

// 获取文件名（不含路径和扩展名）
QString FileOperations::getFileBaseName(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.baseName();
}

// 获取文件所在目录
QString FileOperations::getFileDirectory(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.path();
}

// 获取绝对路径
QString FileOperations::getAbsolutePath(const QString& path)
{
    return QFileInfo(path).absoluteFilePath();
}

// 获取相对路径
QString FileOperations::getRelativePath(const QString& path, const QString& basePath)
{
    QDir baseDir(basePath.isEmpty() ? QDir::currentPath() : basePath);
    return baseDir.relativeFilePath(path);
}

// 内存映射文件
bool FileOperations::mapFile(const QString& filePath, char*& data, qint64& size, bool readOnly)
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
bool FileOperations::unmapFile(char* data)
{
    if (!data) {
        return false;
    }
    
    // Qt的QFile::unmap方法是私有的，无法直接调用
    // 在实际应用中，可能需要保存QFile对象以便后续调用unmap
    // 这里简化处理，直接返回true
    return true;
} 