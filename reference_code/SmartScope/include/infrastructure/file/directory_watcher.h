#ifndef DIRECTORY_WATCHER_H
#define DIRECTORY_WATCHER_H

#include <QObject>
#include <QString>
#include <QFileSystemWatcher>
#include <QMap>
#include <QDateTime>

/**
 * @brief 目录监视器类
 * 
 * 监视目录变化，提供文件创建、修改、删除的通知
 */
class DirectoryWatcher : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * @param dirPath 目录路径
     * @param parent 父对象
     */
    explicit DirectoryWatcher(const QString& dirPath, QObject* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    virtual ~DirectoryWatcher();
    
    /**
     * @brief 获取监视的目录路径
     * @return 目录路径
     */
    QString getDirectoryPath() const;
    
    /**
     * @brief 启动监视
     * @return 是否成功启动
     */
    bool start();
    
    /**
     * @brief 停止监视
     */
    void stop();
    
    /**
     * @brief 是否正在监视
     * @return 是否正在监视
     */
    bool isWatching() const;
    
    /**
     * @brief 设置是否监视子目录
     * @param recursive 是否监视子目录
     */
    void setRecursive(bool recursive);
    
    /**
     * @brief 是否监视子目录
     * @return 是否监视子目录
     */
    bool isRecursive() const;
    
    /**
     * @brief 设置文件过滤器
     * @param filters 文件过滤器列表
     */
    void setFilters(const QStringList& filters);
    
    /**
     * @brief 获取文件过滤器
     * @return 文件过滤器列表
     */
    QStringList getFilters() const;
    
signals:
    /**
     * @brief 文件创建信号
     * @param path 文件路径
     */
    void fileCreated(const QString& path);
    
    /**
     * @brief 文件修改信号
     * @param path 文件路径
     */
    void fileModified(const QString& path);
    
    /**
     * @brief 文件删除信号
     * @param path 文件路径
     */
    void fileDeleted(const QString& path);
    
    /**
     * @brief 目录创建信号
     * @param path 目录路径
     */
    void directoryCreated(const QString& path);
    
    /**
     * @brief 目录删除信号
     * @param path 目录路径
     */
    void directoryDeleted(const QString& path);
    
private slots:
    /**
     * @brief 目录变化槽函数
     * @param path 目录路径
     */
    void onDirectoryChanged(const QString& path);
    
    /**
     * @brief 文件变化槽函数
     * @param path 文件路径
     */
    void onFileChanged(const QString& path);
    
    /**
     * @brief 扫描目录槽函数
     */
    void scanDirectory();
    
private:
    // 添加目录到监视列表
    void addDirectory(const QString& path);
    
    // 添加文件到监视列表
    void addFile(const QString& path);
    
    // 更新文件列表
    void updateFileList();
    
    // 检查文件是否匹配过滤器
    bool matchesFilter(const QString& filePath) const;
    
    // 目录路径
    QString m_dirPath;
    
    // 文件系统监视器
    QFileSystemWatcher* m_watcher;
    
    // 是否正在监视
    bool m_isWatching;
    
    // 是否监视子目录
    bool m_recursive;
    
    // 文件过滤器
    QStringList m_filters;
    
    // 文件修改时间映射表
    QMap<QString, QDateTime> m_fileModifiedTimes;
};

#endif // DIRECTORY_WATCHER_H 