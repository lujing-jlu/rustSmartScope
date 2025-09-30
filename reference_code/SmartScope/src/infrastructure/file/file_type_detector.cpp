#include "infrastructure/file/file_type_detector.h"
#include "infrastructure/logging/logger.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

// 构造函数
FileTypeDetector::FileTypeDetector()
{
    initExtensionMap();
    initHeaderMap();
}

// 析构函数
FileTypeDetector::~FileTypeDetector()
{
}

// 检测文件类型
FileType FileTypeDetector::detectType(const QString& filePath)
{
    // 首先尝试通过扩展名检测
    FileType type = detectTypeByExtension(filePath);
    if (type != FileType::UNKNOWN) {
        return type;
    }
    
    // 如果扩展名检测失败，尝试通过文件头检测
    QByteArray data;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        // 只读取前 512 字节用于检测
        data = file.read(512);
        file.close();
        
        type = detectTypeByHeader(data);
    }
    
    return type;
}

// 检测文件类型
FileType FileTypeDetector::detectType(const QByteArray& data, const QString& filePath)
{
    // 如果提供了文件路径，先尝试通过扩展名检测
    if (!filePath.isEmpty()) {
        FileType type = detectTypeByExtension(filePath);
        if (type != FileType::UNKNOWN) {
            return type;
        }
    }
    
    // 通过文件头检测
    return detectTypeByHeader(data);
}

// 根据扩展名检测文件类型
FileType FileTypeDetector::detectTypeByExtension(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (m_extensionMap.contains(extension)) {
        return m_extensionMap[extension];
    }
    
    // 使用Qt的MIME类型系统作为备选
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath, QMimeDatabase::MatchExtension);
    QString mimeTypeName = mime.name();
    
    if (mimeTypeName.startsWith("text/")) {
        return FileType::TEXT;
    } else if (mimeTypeName.startsWith("image/")) {
        return FileType::IMAGE;
    } else if (mimeTypeName.startsWith("audio/")) {
        return FileType::AUDIO;
    } else if (mimeTypeName.startsWith("video/")) {
        return FileType::VIDEO;
    } else if (mimeTypeName.contains("document") || 
               mimeTypeName.contains("pdf") || 
               mimeTypeName.contains("office")) {
        return FileType::DOCUMENT;
    } else if (mimeTypeName.contains("archive") || 
               mimeTypeName.contains("compressed")) {
        return FileType::ARCHIVE;
    } else if (mimeTypeName.contains("executable")) {
        return FileType::EXECUTABLE;
    }
    
    return FileType::UNKNOWN;
}

// 根据文件头检测文件类型
FileType FileTypeDetector::detectTypeByHeader(const QByteArray& data)
{
    if (data.isEmpty()) {
        return FileType::UNKNOWN;
    }
    
    // 检查每个已知的文件头
    for (const FileHeader& header : m_headerList) {
        if (data.size() > header.offset + header.header.size()) {
            if (data.mid(header.offset, header.header.size()) == header.header) {
                return header.type;
            }
        }
    }
    
    // 检查是否为文本文件
    bool isProbablyText = true;
    for (int i = 0; i < qMin(data.size(), 512); ++i) {
        char c = data[i];
        if (c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r')) {
            isProbablyText = false;
            break;
        }
    }
    
    if (isProbablyText) {
        return FileType::TEXT;
    }
    
    return FileType::BINARY;
}

// 获取文件类型的字符串表示
QString FileTypeDetector::typeToString(FileType type)
{
    switch (type) {
        case FileType::TEXT:
            return "TEXT";
        case FileType::IMAGE:
            return "IMAGE";
        case FileType::AUDIO:
            return "AUDIO";
        case FileType::VIDEO:
            return "VIDEO";
        case FileType::DOCUMENT:
            return "DOCUMENT";
        case FileType::ARCHIVE:
            return "ARCHIVE";
        case FileType::EXECUTABLE:
            return "EXECUTABLE";
        case FileType::BINARY:
            return "BINARY";
        case FileType::POINT_CLOUD:
            return "POINT_CLOUD";
        case FileType::MODEL_3D:
            return "MODEL_3D";
        case FileType::CONFIG:
            return "CONFIG";
        case FileType::DATABASE:
            return "DATABASE";
        case FileType::FONT:
            return "FONT";
        case FileType::SCRIPT:
            return "SCRIPT";
        case FileType::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

// 从字符串解析文件类型
FileType FileTypeDetector::typeFromString(const QString& typeStr)
{
    QString upperTypeStr = typeStr.toUpper();
    
    if (upperTypeStr == "TEXT") {
        return FileType::TEXT;
    } else if (upperTypeStr == "IMAGE") {
        return FileType::IMAGE;
    } else if (upperTypeStr == "AUDIO") {
        return FileType::AUDIO;
    } else if (upperTypeStr == "VIDEO") {
        return FileType::VIDEO;
    } else if (upperTypeStr == "DOCUMENT") {
        return FileType::DOCUMENT;
    } else if (upperTypeStr == "ARCHIVE") {
        return FileType::ARCHIVE;
    } else if (upperTypeStr == "EXECUTABLE") {
        return FileType::EXECUTABLE;
    } else if (upperTypeStr == "BINARY") {
        return FileType::BINARY;
    } else if (upperTypeStr == "POINT_CLOUD") {
        return FileType::POINT_CLOUD;
    } else if (upperTypeStr == "MODEL_3D") {
        return FileType::MODEL_3D;
    } else if (upperTypeStr == "CONFIG") {
        return FileType::CONFIG;
    } else if (upperTypeStr == "DATABASE") {
        return FileType::DATABASE;
    } else if (upperTypeStr == "FONT") {
        return FileType::FONT;
    } else if (upperTypeStr == "SCRIPT") {
        return FileType::SCRIPT;
    } else {
        return FileType::UNKNOWN;
    }
}

// 获取文件类型的MIME类型
QString FileTypeDetector::typeToMimeType(FileType type)
{
    switch (type) {
        case FileType::TEXT:
            return "text/plain";
        case FileType::IMAGE:
            return "image/*";
        case FileType::AUDIO:
            return "audio/*";
        case FileType::VIDEO:
            return "video/*";
        case FileType::DOCUMENT:
            return "application/document";
        case FileType::ARCHIVE:
            return "application/x-archive";
        case FileType::EXECUTABLE:
            return "application/x-executable";
        case FileType::BINARY:
            return "application/octet-stream";
        case FileType::POINT_CLOUD:
            return "application/x-point-cloud";
        case FileType::MODEL_3D:
            return "application/x-3d-model";
        case FileType::CONFIG:
            return "application/x-config";
        case FileType::DATABASE:
            return "application/x-database";
        case FileType::FONT:
            return "application/x-font";
        case FileType::SCRIPT:
            return "application/x-script";
        case FileType::UNKNOWN:
        default:
            return "application/octet-stream";
    }
}

// 从MIME类型解析文件类型
FileType FileTypeDetector::typeFromMimeType(const QString& mimeType)
{
    if (mimeType.startsWith("text/")) {
        return FileType::TEXT;
    } else if (mimeType.startsWith("image/")) {
        return FileType::IMAGE;
    } else if (mimeType.startsWith("audio/")) {
        return FileType::AUDIO;
    } else if (mimeType.startsWith("video/")) {
        return FileType::VIDEO;
    } else if (mimeType.contains("document") || 
               mimeType.contains("pdf") || 
               mimeType.contains("office")) {
        return FileType::DOCUMENT;
    } else if (mimeType.contains("archive") || 
               mimeType.contains("compressed")) {
        return FileType::ARCHIVE;
    } else if (mimeType.contains("executable")) {
        return FileType::EXECUTABLE;
    } else if (mimeType.contains("point-cloud")) {
        return FileType::POINT_CLOUD;
    } else if (mimeType.contains("3d-model")) {
        return FileType::MODEL_3D;
    } else if (mimeType.contains("config")) {
        return FileType::CONFIG;
    } else if (mimeType.contains("database")) {
        return FileType::DATABASE;
    } else if (mimeType.contains("font")) {
        return FileType::FONT;
    } else if (mimeType.contains("script")) {
        return FileType::SCRIPT;
    } else {
        return FileType::BINARY;
    }
}

// 获取文件类型的扩展名列表
QStringList FileTypeDetector::getExtensionsForType(FileType type) const
{
    QStringList extensions;
    
    for (auto it = m_extensionMap.begin(); it != m_extensionMap.end(); ++it) {
        if (it.value() == type) {
            extensions.append(it.key());
        }
    }
    
    return extensions;
}

// 注册自定义文件类型检测器
void FileTypeDetector::registerCustomType(const QString& extension, FileType type)
{
    m_extensionMap[extension.toLower()] = type;
}

// 初始化扩展名映射表
void FileTypeDetector::initExtensionMap()
{
    // 文本文件
    m_extensionMap["txt"] = FileType::TEXT;
    m_extensionMap["log"] = FileType::TEXT;
    m_extensionMap["ini"] = FileType::TEXT;
    m_extensionMap["csv"] = FileType::TEXT;
    m_extensionMap["xml"] = FileType::TEXT;
    m_extensionMap["json"] = FileType::TEXT;
    m_extensionMap["html"] = FileType::TEXT;
    m_extensionMap["htm"] = FileType::TEXT;
    m_extensionMap["md"] = FileType::TEXT;
    m_extensionMap["cpp"] = FileType::TEXT;
    m_extensionMap["h"] = FileType::TEXT;
    m_extensionMap["c"] = FileType::TEXT;
    m_extensionMap["py"] = FileType::TEXT;
    m_extensionMap["java"] = FileType::TEXT;
    m_extensionMap["js"] = FileType::TEXT;
    m_extensionMap["css"] = FileType::TEXT;
    
    // 图像文件
    m_extensionMap["jpg"] = FileType::IMAGE;
    m_extensionMap["jpeg"] = FileType::IMAGE;
    m_extensionMap["png"] = FileType::IMAGE;
    m_extensionMap["gif"] = FileType::IMAGE;
    m_extensionMap["bmp"] = FileType::IMAGE;
    m_extensionMap["tiff"] = FileType::IMAGE;
    m_extensionMap["tif"] = FileType::IMAGE;
    m_extensionMap["svg"] = FileType::IMAGE;
    m_extensionMap["webp"] = FileType::IMAGE;
    
    // 音频文件
    m_extensionMap["mp3"] = FileType::AUDIO;
    m_extensionMap["wav"] = FileType::AUDIO;
    m_extensionMap["ogg"] = FileType::AUDIO;
    m_extensionMap["flac"] = FileType::AUDIO;
    m_extensionMap["aac"] = FileType::AUDIO;
    m_extensionMap["wma"] = FileType::AUDIO;
    
    // 视频文件
    m_extensionMap["mp4"] = FileType::VIDEO;
    m_extensionMap["avi"] = FileType::VIDEO;
    m_extensionMap["mkv"] = FileType::VIDEO;
    m_extensionMap["mov"] = FileType::VIDEO;
    m_extensionMap["wmv"] = FileType::VIDEO;
    m_extensionMap["flv"] = FileType::VIDEO;
    m_extensionMap["webm"] = FileType::VIDEO;
    
    // 文档文件
    m_extensionMap["pdf"] = FileType::DOCUMENT;
    m_extensionMap["doc"] = FileType::DOCUMENT;
    m_extensionMap["docx"] = FileType::DOCUMENT;
    m_extensionMap["xls"] = FileType::DOCUMENT;
    m_extensionMap["xlsx"] = FileType::DOCUMENT;
    m_extensionMap["ppt"] = FileType::DOCUMENT;
    m_extensionMap["pptx"] = FileType::DOCUMENT;
    m_extensionMap["odt"] = FileType::DOCUMENT;
    m_extensionMap["ods"] = FileType::DOCUMENT;
    m_extensionMap["odp"] = FileType::DOCUMENT;
    
    // 压缩文件
    m_extensionMap["zip"] = FileType::ARCHIVE;
    m_extensionMap["rar"] = FileType::ARCHIVE;
    m_extensionMap["7z"] = FileType::ARCHIVE;
    m_extensionMap["tar"] = FileType::ARCHIVE;
    m_extensionMap["gz"] = FileType::ARCHIVE;
    m_extensionMap["bz2"] = FileType::ARCHIVE;
    
    // 可执行文件
    m_extensionMap["exe"] = FileType::EXECUTABLE;
    m_extensionMap["dll"] = FileType::EXECUTABLE;
    m_extensionMap["so"] = FileType::EXECUTABLE;
    m_extensionMap["dylib"] = FileType::EXECUTABLE;
    m_extensionMap["app"] = FileType::EXECUTABLE;
    
    // 点云文件
    m_extensionMap["pcd"] = FileType::POINT_CLOUD;
    m_extensionMap["ply"] = FileType::POINT_CLOUD;
    m_extensionMap["xyz"] = FileType::POINT_CLOUD;
    m_extensionMap["pts"] = FileType::POINT_CLOUD;
    m_extensionMap["las"] = FileType::POINT_CLOUD;
    
    // 3D模型文件
    m_extensionMap["obj"] = FileType::MODEL_3D;
    m_extensionMap["stl"] = FileType::MODEL_3D;
    m_extensionMap["fbx"] = FileType::MODEL_3D;
    m_extensionMap["dae"] = FileType::MODEL_3D;
    m_extensionMap["3ds"] = FileType::MODEL_3D;
    
    // 配置文件
    m_extensionMap["cfg"] = FileType::CONFIG;
    m_extensionMap["conf"] = FileType::CONFIG;
    m_extensionMap["config"] = FileType::CONFIG;
    m_extensionMap["properties"] = FileType::CONFIG;
    m_extensionMap["toml"] = FileType::CONFIG;
    m_extensionMap["yaml"] = FileType::CONFIG;
    m_extensionMap["yml"] = FileType::CONFIG;
    
    // 数据库文件
    m_extensionMap["db"] = FileType::DATABASE;
    m_extensionMap["sqlite"] = FileType::DATABASE;
    m_extensionMap["sqlite3"] = FileType::DATABASE;
    m_extensionMap["mdb"] = FileType::DATABASE;
    
    // 字体文件
    m_extensionMap["ttf"] = FileType::FONT;
    m_extensionMap["otf"] = FileType::FONT;
    m_extensionMap["woff"] = FileType::FONT;
    m_extensionMap["woff2"] = FileType::FONT;
    m_extensionMap["eot"] = FileType::FONT;
    
    // 脚本文件
    m_extensionMap["sh"] = FileType::SCRIPT;
    m_extensionMap["bat"] = FileType::SCRIPT;
    m_extensionMap["ps1"] = FileType::SCRIPT;
    m_extensionMap["cmd"] = FileType::SCRIPT;
}

// 初始化文件头映射表
void FileTypeDetector::initHeaderMap()
{
    // JPEG
    FileHeader jpegHeader;
    jpegHeader.header = QByteArray::fromHex("FFD8FF");
    jpegHeader.offset = 0;
    jpegHeader.type = FileType::IMAGE;
    m_headerList.append(jpegHeader);
    
    // PNG
    FileHeader pngHeader;
    pngHeader.header = QByteArray::fromHex("89504E470D0A1A0A");
    pngHeader.offset = 0;
    pngHeader.type = FileType::IMAGE;
    m_headerList.append(pngHeader);
    
    // GIF
    FileHeader gifHeader;
    gifHeader.header = QByteArray::fromHex("474946");
    gifHeader.offset = 0;
    gifHeader.type = FileType::IMAGE;
    m_headerList.append(gifHeader);
    
    // BMP
    FileHeader bmpHeader;
    bmpHeader.header = QByteArray::fromHex("424D");
    bmpHeader.offset = 0;
    bmpHeader.type = FileType::IMAGE;
    m_headerList.append(bmpHeader);
    
    // PDF
    FileHeader pdfHeader;
    pdfHeader.header = QByteArray::fromHex("25504446");
    pdfHeader.offset = 0;
    pdfHeader.type = FileType::DOCUMENT;
    m_headerList.append(pdfHeader);
    
    // ZIP
    FileHeader zipHeader;
    zipHeader.header = QByteArray::fromHex("504B0304");
    zipHeader.offset = 0;
    zipHeader.type = FileType::ARCHIVE;
    m_headerList.append(zipHeader);
    
    // RAR
    FileHeader rarHeader;
    rarHeader.header = QByteArray::fromHex("526172211A07");
    rarHeader.offset = 0;
    rarHeader.type = FileType::ARCHIVE;
    m_headerList.append(rarHeader);
    
    // 7Z
    FileHeader sevenZHeader;
    sevenZHeader.header = QByteArray::fromHex("377ABCAF271C");
    sevenZHeader.offset = 0;
    sevenZHeader.type = FileType::ARCHIVE;
    m_headerList.append(sevenZHeader);
    
    // MP3
    FileHeader mp3Header;
    mp3Header.header = QByteArray::fromHex("494433");
    mp3Header.offset = 0;
    mp3Header.type = FileType::AUDIO;
    m_headerList.append(mp3Header);
    
    // WAV
    FileHeader wavHeader;
    wavHeader.header = QByteArray::fromHex("52494646");
    wavHeader.offset = 0;
    wavHeader.type = FileType::AUDIO;
    m_headerList.append(wavHeader);
    
    // MP4
    FileHeader mp4Header;
    mp4Header.header = QByteArray::fromHex("00000020667479704D534E56");
    mp4Header.offset = 0;
    mp4Header.type = FileType::VIDEO;
    m_headerList.append(mp4Header);
    
    // AVI
    FileHeader aviHeader;
    aviHeader.header = QByteArray::fromHex("52494646");
    aviHeader.offset = 0;
    aviHeader.type = FileType::VIDEO;
    m_headerList.append(aviHeader);
    
    // EXE
    FileHeader exeHeader;
    exeHeader.header = QByteArray::fromHex("4D5A");
    exeHeader.offset = 0;
    exeHeader.type = FileType::EXECUTABLE;
    m_headerList.append(exeHeader);
    
    // ELF
    FileHeader elfHeader;
    elfHeader.header = QByteArray::fromHex("7F454C46");
    elfHeader.offset = 0;
    elfHeader.type = FileType::EXECUTABLE;
    m_headerList.append(elfHeader);
    
    // SQLite
    FileHeader sqliteHeader;
    sqliteHeader.header = QByteArray::fromHex("53514C69746520666F726D6174203300");
    sqliteHeader.offset = 0;
    sqliteHeader.type = FileType::DATABASE;
    m_headerList.append(sqliteHeader);
} 