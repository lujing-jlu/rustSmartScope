#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVariant>
#include <QMutex>
#include <QMap>
#include <QFileDialog>
#include <QDialog>
#include <QPushButton>
#include <QScreen>

#ifndef QT_NO_WIDGETS
#include <QFileDialog>
#endif

#include <memory>

#include "infrastructure/file/file_operations.h"
#include "infrastructure/file/directory_watcher.h"
#include "infrastructure/file/file_type_detector.h"

class DirectoryWatcher;
class FileTypeDetector;
enum class FileType;

/**
 * @brief 文件权限结构体
 */
struct FilePermissions {
    bool canRead;
    bool canWrite;
    bool canExecute;
    
    FilePermissions() : canRead(false), canWrite(false), canExecute(false) {}
};

// 前向声明
class FileManager;

/**
 * @brief FileManager的删除器类
 */
class FileManagerDeleter
{
public:
    void operator()(FileManager* manager);
};

class CustomFileDialog : public QFileDialog {
    Q_OBJECT
public:
    explicit CustomFileDialog(QWidget* parent = nullptr,
                            const QString& caption = QString(),
                            const QString& directory = QString(),
                            const QString& filter = QString());
protected:
    bool event(QEvent* event) override;
};

/**
 * @brief 文件管理器类
 * 
 * 提供统一的文件操作接口，支持文件读写、目录管理、文件监视等功能
 */
class FileManager : public QObject
{
public:
    /**
     * @brief 获取单例实例
     * @return 单例实例引用
     */
    static FileManager& instance();
    
    /**
     * @brief 初始化文件管理器
     * @param workingDir 工作目录
     * @param tempDir 临时目录
     * @param createIfNotExist 如果目录不存在是否创建
     * @return 是否成功初始化
     */
    bool init(const QString& workingDir = "data", const QString& tempDir = "temp", bool createIfNotExist = true);
    
    /**
     * @brief 读取文本文件
     * @param filePath 文件路径
     * @param content 输出参数，文件内容
     * @return 是否成功读取
     */
    bool readTextFile(const QString& filePath, QString& content);
    
    /**
     * @brief 写入文本文件
     * @param filePath 文件路径
     * @param content 文件内容
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    bool writeTextFile(const QString& filePath, const QString& content, bool append = false);
    
    /**
     * @brief 读取二进制文件
     * @param filePath 文件路径
     * @param data 输出参数，文件数据
     * @return 是否成功读取
     */
    bool readBinaryFile(const QString& filePath, QByteArray& data);
    
    /**
     * @brief 写入二进制文件
     * @param filePath 文件路径
     * @param data 文件数据
     * @param append 是否追加模式
     * @return 是否成功写入
     */
    bool writeBinaryFile(const QString& filePath, const QByteArray& data, bool append = false);
    
    /**
     * @brief 创建目录
     * @param dirPath 目录路径
     * @param createParents 是否创建父目录
     * @return 是否成功创建
     */
    bool createDirectory(const QString& dirPath, bool createParents = true);
    
    /**
     * @brief 列出目录内容
     * @param dirPath 目录路径
     * @param entries 输出参数，目录条目列表
     * @param filter 过滤器
     * @return 是否成功列出
     */
    bool listDirectory(const QString& dirPath, QStringList& entries, const QString& filter = "*");
    
    /**
     * @brief 检查路径是否存在
     * @param path 路径
     * @return 是否存在
     */
    bool exists(const QString& path);
    
    /**
     * @brief 检查路径是否为目录
     * @param path 路径
     * @return 是否为目录
     */
    bool isDirectory(const QString& path);
    
    /**
     * @brief 检查路径是否为文件
     * @param path 路径
     * @return 是否为文件
     */
    bool isFile(const QString& path);
    
    /**
     * @brief 获取文件大小
     * @param filePath 文件路径
     * @return 文件大小（字节）
     */
    qint64 getFileSize(const QString& filePath);
    
    /**
     * @brief 获取文件修改时间
     * @param filePath 文件路径
     * @return 文件修改时间
     */
    QDateTime getFileModifiedTime(const QString& filePath);
    
    /**
     * @brief 复制文件
     * @param sourcePath 源文件路径
     * @param destPath 目标文件路径
     * @param overwrite 是否覆盖已存在的文件
     * @return 是否成功复制
     */
    bool copyFile(const QString& sourcePath, const QString& destPath, bool overwrite = true);
    
    /**
     * @brief 移动文件
     * @param sourcePath 源文件路径
     * @param destPath 目标文件路径
     * @param overwrite 是否覆盖已存在的文件
     * @return 是否成功移动
     */
    bool moveFile(const QString& sourcePath, const QString& destPath, bool overwrite = true);
    
    /**
     * @brief 删除文件
     * @param filePath 文件路径
     * @return 是否成功删除
     */
    bool deleteFile(const QString& filePath);
    
    /**
     * @brief 删除目录
     * @param dirPath 目录路径
     * @param recursive 是否递归删除
     * @return 是否成功删除
     */
    bool deleteDirectory(const QString& dirPath, bool recursive = false);
    
    /**
     * @brief 获取文件权限
     * @param path 文件路径
     * @param permissions 输出参数，文件权限
     * @return 是否成功获取
     */
    bool getFilePermissions(const QString& path, FilePermissions& permissions);
    
    /**
     * @brief 设置文件权限
     * @param path 文件路径
     * @param permissions 文件权限
     * @return 是否成功设置
     */
    bool setFilePermissions(const QString& path, const FilePermissions& permissions);
    
    /**
     * @brief 创建临时文件
     * @param prefix 文件名前缀
     * @param suffix 文件名后缀
     * @param filePath 输出参数，临时文件路径
     * @return 是否成功创建
     */
    bool createTempFile(const QString& prefix, const QString& suffix, QString& filePath);
    
    /**
     * @brief 创建临时目录
     * @param prefix 目录名前缀
     * @param dirPath 输出参数，临时目录路径
     * @return 是否成功创建
     */
    bool createTempDirectory(const QString& prefix, QString& dirPath);
    
    /**
     * @brief 清理临时文件
     * @param olderThan 清理早于指定时间的文件（毫秒）
     * @return 是否成功清理
     */
    bool cleanupTempFiles(qint64 olderThan = 86400000); // 默认清理一天前的文件
    
    /**
     * @brief 备份文件
     * @param filePath 文件路径
     * @param backupPath 输出参数，备份文件路径
     * @return 是否成功备份
     */
    bool backupFile(const QString& filePath, QString& backupPath);
    
    /**
     * @brief 恢复文件
     * @param backupPath 备份文件路径
     * @param filePath 目标文件路径
     * @return 是否成功恢复
     */
    bool restoreFile(const QString& backupPath, const QString& filePath);
    
    /**
     * @brief 创建目录监视器
     * @param dirPath 目录路径
     * @return 目录监视器指针
     */
    DirectoryWatcher* createDirectoryWatcher(const QString& dirPath);
    
    /**
     * @brief 销毁目录监视器
     * @param watcher 目录监视器指针
     */
    void destroyDirectoryWatcher(DirectoryWatcher* watcher);
    
    /**
     * @brief 检查文件是否具有指定扩展名
     * @param filePath 文件路径
     * @param extensions 扩展名列表
     * @param caseSensitive 是否区分大小写
     * @return 是否具有指定扩展名
     */
    bool hasExtension(const QString& filePath, const QStringList& extensions, bool caseSensitive = false);
    
    /**
     * @brief 获取文件扩展名
     * @param filePath 文件路径
     * @return 文件扩展名
     */
    QString getFileExtension(const QString& filePath);
    
    /**
     * @brief 获取文件名（不含路径）
     * @param filePath 文件路径
     * @return 文件名
     */
    QString getFileName(const QString& filePath);
    
    /**
     * @brief 获取文件名（不含路径和扩展名）
     * @param filePath 文件路径
     * @return 文件名
     */
    QString getFileBaseName(const QString& filePath);
    
    /**
     * @brief 获取文件所在目录
     * @param filePath 文件路径
     * @return 目录路径
     */
    QString getFileDirectory(const QString& filePath);
    
    /**
     * @brief 获取绝对路径
     * @param path 相对路径
     * @return 绝对路径
     */
    QString getAbsolutePath(const QString& path);
    
    /**
     * @brief 获取相对路径
     * @param path 绝对路径
     * @param basePath 基准路径
     * @return 相对路径
     */
    QString getRelativePath(const QString& path, const QString& basePath = QString());
    
    /**
     * @brief 内存映射文件
     * @param filePath 文件路径
     * @param data 输出参数，映射的数据指针
     * @param size 输出参数，映射的数据大小
     * @param readOnly 是否只读模式
     * @return 是否成功映射
     */
    bool mapFile(const QString& filePath, char*& data, qint64& size, bool readOnly = true);
    
    /**
     * @brief 解除内存映射
     * @param data 映射的数据指针
     * @return 是否成功解除映射
     */
    bool unmapFile(char* data);
    
    /**
     * @brief 检测文件类型
     * @param filePath 文件路径
     * @return 文件类型
     */
    FileType detectFileType(const QString& filePath);
    
    /**
     * @brief 重命名文件或目录
     * @param oldPath 原路径
     * @param newPath 新路径
     * @return 是否成功
     */
    bool rename(const QString& oldPath, const QString& newPath);

#ifndef QT_NO_WIDGETS
    /**
     * @brief 打开文件对话框
     * @param parent 父窗口
     * @param caption 对话框标题
     * @param dir 初始目录
     * @param filter 文件过滤器
     * @param selectedFilter 选中的过滤器
     * @param options 对话框选项
     * @return 选中的文件路径
     */
    QString getOpenFileName(QWidget* parent = nullptr, const QString& caption = QString(), 
                          const QString& dir = QString(), const QString& filter = QString(), 
                          QString* selectedFilter = nullptr, QFileDialog::Options options = QFileDialog::Options());
    
    /**
     * @brief 打开多个文件对话框
     * @param parent 父窗口
     * @param caption 对话框标题
     * @param dir 初始目录
     * @param filter 文件过滤器
     * @param selectedFilter 选中的过滤器
     * @param options 对话框选项
     * @return 选中的文件路径列表
     */
    QStringList getOpenFileNames(QWidget* parent = nullptr, const QString& caption = QString(), 
                               const QString& dir = QString(), const QString& filter = QString(), 
                               QString* selectedFilter = nullptr, QFileDialog::Options options = QFileDialog::Options());
    
    /**
     * @brief 保存文件对话框
     * @param parent 父窗口
     * @param caption 对话框标题
     * @param dir 初始目录
     * @param filter 文件过滤器
     * @param selectedFilter 选中的过滤器
     * @param options 对话框选项
     * @return 选中的文件路径
     */
    QString getSaveFileName(QWidget* parent = nullptr, const QString& caption = QString(), 
                          const QString& dir = QString(), const QString& filter = QString(), 
                          QString* selectedFilter = nullptr, QFileDialog::Options options = QFileDialog::Options());
    
    /**
     * @brief 选择目录对话框
     * @param parent 父窗口
     * @param caption 对话框标题
     * @param dir 初始目录
     * @param options 对话框选项
     * @return 选中的目录路径
     */
    QString getExistingDirectory(QWidget* parent = nullptr, const QString& caption = QString(), 
                               const QString& dir = QString(), 
                               QFileDialog::Options options = QFileDialog::ShowDirsOnly);
#endif
    
    /**
     * @brief 批量删除文件
     * @param filePaths 文件路径列表
     * @return 成功删除的文件数量
     */
    int batchDeleteFiles(const QStringList& filePaths);
    
    /**
     * @brief 批量复制文件
     * @param sourcePaths 源文件路径列表
     * @param destDir 目标目录
     * @param overwrite 是否覆盖已存在的文件
     * @return 成功复制的文件数量
     */
    int batchCopyFiles(const QStringList& sourcePaths, const QString& destDir, bool overwrite = true);
    
    /**
     * @brief 批量移动文件
     * @param sourcePaths 源文件路径列表
     * @param destDir 目标目录
     * @param overwrite 是否覆盖已存在的文件
     * @return 成功移动的文件数量
     */
    int batchMoveFiles(const QStringList& sourcePaths, const QString& destDir, bool overwrite = true);
    
    /**
     * @brief 搜索文件
     * @param dirPath 目录路径
     * @param namePattern 文件名模式
     * @param recursive 是否递归搜索子目录
     * @return 匹配的文件路径列表
     */
    QStringList searchFiles(const QString& dirPath, const QString& namePattern, bool recursive = true);
    
    /**
     * @brief 搜索文件内容
     * @param dirPath 目录路径
     * @param content 要搜索的内容
     * @param namePattern 文件名模式
     * @param recursive 是否递归搜索子目录
     * @param caseSensitive 是否区分大小写
     * @return 包含指定内容的文件路径列表
     */
    QStringList searchFileContent(const QString& dirPath, const QString& content, 
                                const QString& namePattern = "*", bool recursive = true, 
                                bool caseSensitive = false);
    
protected:
    /**
     * @brief 析构函数
     */
    ~FileManager();
    
    // 允许 FileManagerDeleter 访问私有成员
    friend class FileManagerDeleter;
    
private:
    // 私有构造函数，防止外部创建实例
    FileManager();
    
    // 禁用拷贝构造和赋值操作
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // 获取临时目录路径
    QString getTempDirPath() const;
    
    // 生成唯一文件名
    QString generateUniqueFileName(const QString& prefix, const QString& suffix) const;
    
    // 工作目录
    QString m_workingDir;
    
    // 临时目录
    QString m_tempDir;
    
    // 目录监视器映射表
    QMap<QString, DirectoryWatcher*> m_watchers;
    
    // 文件类型检测器
    std::unique_ptr<FileTypeDetector> m_fileTypeDetector;
    
    // 互斥锁，保证线程安全
    mutable QMutex m_mutex;
    
    // 单例实例
    static std::unique_ptr<FileManager, FileManagerDeleter> s_instance;
};

// 实现 FileManagerDeleter 的 operator()
inline void FileManagerDeleter::operator()(FileManager* manager) {
    delete manager;
}

#endif // FILE_MANAGER_H 