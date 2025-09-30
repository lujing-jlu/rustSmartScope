#ifndef FILE_TYPE_DETECTOR_H
#define FILE_TYPE_DETECTOR_H

#include <QString>
#include <QMap>
#include <QByteArray>

/**
 * @brief 文件类型枚举
 */
enum class FileType {
    UNKNOWN,        // 未知类型
    TEXT,           // 文本文件
    IMAGE,          // 图像文件
    AUDIO,          // 音频文件
    VIDEO,          // 视频文件
    DOCUMENT,       // 文档文件
    ARCHIVE,        // 压缩文件
    EXECUTABLE,     // 可执行文件
    BINARY,         // 二进制文件
    POINT_CLOUD,    // 点云文件
    MODEL_3D,       // 3D模型文件
    CONFIG,         // 配置文件
    DATABASE,       // 数据库文件
    FONT,           // 字体文件
    SCRIPT          // 脚本文件
};

/**
 * @brief 文件类型检测器类
 * 
 * 检测文件类型，支持基于扩展名和文件头的检测
 */
class FileTypeDetector
{
public:
    /**
     * @brief 构造函数
     */
    FileTypeDetector();
    
    /**
     * @brief 析构函数
     */
    virtual ~FileTypeDetector();
    
    /**
     * @brief 检测文件类型
     * @param filePath 文件路径
     * @return 文件类型
     */
    FileType detectType(const QString& filePath);
    
    /**
     * @brief 检测文件类型
     * @param data 文件数据
     * @param filePath 文件路径（可选，用于扩展名检测）
     * @return 文件类型
     */
    FileType detectType(const QByteArray& data, const QString& filePath = QString());
    
    /**
     * @brief 根据扩展名检测文件类型
     * @param filePath 文件路径
     * @return 文件类型
     */
    FileType detectTypeByExtension(const QString& filePath);
    
    /**
     * @brief 根据文件头检测文件类型
     * @param data 文件数据
     * @return 文件类型
     */
    FileType detectTypeByHeader(const QByteArray& data);
    
    /**
     * @brief 获取文件类型的字符串表示
     * @param type 文件类型
     * @return 字符串表示
     */
    static QString typeToString(FileType type);
    
    /**
     * @brief 从字符串解析文件类型
     * @param typeStr 文件类型字符串
     * @return 文件类型
     */
    static FileType typeFromString(const QString& typeStr);
    
    /**
     * @brief 获取文件类型的MIME类型
     * @param type 文件类型
     * @return MIME类型
     */
    static QString typeToMimeType(FileType type);
    
    /**
     * @brief 从MIME类型解析文件类型
     * @param mimeType MIME类型
     * @return 文件类型
     */
    static FileType typeFromMimeType(const QString& mimeType);
    
    /**
     * @brief 获取文件类型的扩展名列表
     * @param type 文件类型
     * @return 扩展名列表
     */
    QStringList getExtensionsForType(FileType type) const;
    
    /**
     * @brief 注册自定义文件类型检测器
     * @param extension 文件扩展名
     * @param type 文件类型
     */
    void registerCustomType(const QString& extension, FileType type);
    
private:
    // 初始化扩展名映射表
    void initExtensionMap();
    
    // 初始化文件头映射表
    void initHeaderMap();
    
    // 扩展名到文件类型的映射表
    QMap<QString, FileType> m_extensionMap;
    
    // 文件头到文件类型的映射表
    struct FileHeader {
        QByteArray header;
        int offset;
        FileType type;
    };
    QList<FileHeader> m_headerList;
};

#endif // FILE_TYPE_DETECTOR_H 