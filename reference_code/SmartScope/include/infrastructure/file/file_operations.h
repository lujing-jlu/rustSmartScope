#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QStringList>

struct FilePermissions;

/**
 * @brief 文件操作类
 * 
 * 提供基本的文件操作功能
 */
class FileOperations
{
public:
    /**
     * @brief 构造函数
     */
    FileOperations();
    
    /**
     * @brief 析构函数
     */
    virtual ~FileOperations();
    
    /**
     * @brief 读取文本文件
     * @param filePath 文件路径
     * @param content 输出参数，文件内容
     * @return 是否成功读取
     */
    static bool readTextFile(const QString& filePath, QString& content);
    
    /**
     * @brief 写入文本文件
     * @param filePath 文件路径
     * @param content 文件内容
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    static bool writeTextFile(const QString& filePath, const QString& content, bool append = false);
    
    /**
     * @brief 读取二进制文件
     * @param filePath 文件路径
     * @param data 输出参数，文件数据
     * @return 是否成功读取
     */
    static bool readBinaryFile(const QString& filePath, QByteArray& data);
    
    /**
     * @brief 写入二进制文件
     * @param filePath 文件路径
     * @param data 文件数据
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    static bool writeBinaryFile(const QString& filePath, const QByteArray& data, bool append = false);
    
    /**
     * @brief 创建目录
     * @param dirPath 目录路径
     * @param createParents 是否创建父目录
     * @return 是否成功创建
     */
    static bool createDirectory(const QString& dirPath, bool createParents = true);
    
    /**
     * @brief 列出目录内容
     * @param dirPath 目录路径
     * @param entries 输出参数，目录条目列表
     * @param filter 过滤器
     * @return 是否成功列出
     */
    static bool listDirectory(const QString& dirPath, QStringList& entries, const QString& filter = "*");
    
    /**
     * @brief 检查路径是否存在
     * @param path 路径
     * @return 是否存在
     */
    static bool exists(const QString& path);
    
    /**
     * @brief 检查路径是否为目录
     * @param path 路径
     * @return 是否为目录
     */
    static bool isDirectory(const QString& path);
    
    /**
     * @brief 检查路径是否为文件
     * @param path 路径
     * @return 是否为文件
     */
    static bool isFile(const QString& path);
    
    /**
     * @brief 获取文件大小
     * @param filePath 文件路径
     * @return 文件大小（字节）
     */
    static qint64 getFileSize(const QString& filePath);
    
    /**
     * @brief 获取文件修改时间
     * @param filePath 文件路径
     * @return 文件修改时间
     */
    static QDateTime getFileModifiedTime(const QString& filePath);
    
    /**
     * @brief 复制文件
     * @param sourcePath 源文件路径
     * @param destPath 目标文件路径
     * @param overwrite 是否覆盖已存在的文件
     * @return 是否成功复制
     */
    static bool copyFile(const QString& sourcePath, const QString& destPath, bool overwrite = true);
    
    /**
     * @brief 移动文件
     * @param sourcePath 源文件路径
     * @param destPath 目标文件路径
     * @param overwrite 是否覆盖已存在的文件
     * @return 是否成功移动
     */
    static bool moveFile(const QString& sourcePath, const QString& destPath, bool overwrite = true);
    
    /**
     * @brief 删除文件
     * @param filePath 文件路径
     * @return 是否成功删除
     */
    static bool deleteFile(const QString& filePath);
    
    /**
     * @brief 删除目录
     * @param dirPath 目录路径
     * @param recursive 是否递归删除
     * @return 是否成功删除
     */
    static bool deleteDirectory(const QString& dirPath, bool recursive = false);
    
    /**
     * @brief 获取文件权限
     * @param path 文件路径
     * @param permissions 输出参数，文件权限
     * @return 是否成功获取
     */
    static bool getFilePermissions(const QString& path, FilePermissions& permissions);
    
    /**
     * @brief 设置文件权限
     * @param path 文件路径
     * @param permissions 文件权限
     * @return 是否成功设置
     */
    static bool setFilePermissions(const QString& path, const FilePermissions& permissions);
    
    /**
     * @brief 获取文件扩展名
     * @param filePath 文件路径
     * @return 文件扩展名
     */
    static QString getFileExtension(const QString& filePath);
    
    /**
     * @brief 获取文件名（不含路径）
     * @param filePath 文件路径
     * @return 文件名
     */
    static QString getFileName(const QString& filePath);
    
    /**
     * @brief 获取文件名（不含路径和扩展名）
     * @param filePath 文件路径
     * @return 文件名
     */
    static QString getFileBaseName(const QString& filePath);
    
    /**
     * @brief 获取文件所在目录
     * @param filePath 文件路径
     * @return 目录路径
     */
    static QString getFileDirectory(const QString& filePath);
    
    /**
     * @brief 获取绝对路径
     * @param path 相对路径
     * @return 绝对路径
     */
    static QString getAbsolutePath(const QString& path);
    
    /**
     * @brief 获取相对路径
     * @param path 绝对路径
     * @param basePath 基准路径
     * @return 相对路径
     */
    static QString getRelativePath(const QString& path, const QString& basePath);
    
    /**
     * @brief 内存映射文件
     * @param filePath 文件路径
     * @param data 输出参数，映射的数据指针
     * @param size 输出参数，映射的数据大小
     * @param readOnly 是否只读模式
     * @return 是否成功映射
     */
    static bool mapFile(const QString& filePath, char*& data, qint64& size, bool readOnly = true);
    
    /**
     * @brief 解除内存映射
     * @param data 映射的数据指针
     * @return 是否成功解除映射
     */
    static bool unmapFile(char* data);
};

#endif // FILE_OPERATIONS_H 