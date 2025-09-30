# Smart Scope Qt 模块使用指南

本文档详细介绍了Smart Scope Qt各个模块的使用方法，帮助开发者快速上手和集成这些模块。

## 目录

1. [基础设施层](#基础设施层)
   1. [日志模块](#日志模块)
   2. [异常处理模块](#异常处理模块)
   3. [配置管理模块](#配置管理模块)
   4. [文件管理模块](#文件管理模块)
2. [核心层](#核心层)
   1. [相机模块](./camera_module.md)
3. [应用层](#应用层)
   1. [UI模块](#ui模块)

## 基础设施层

### 日志模块

日志模块提供了统一的日志记录功能，支持多级别日志和多种输出目标。

#### 主要类

- `Logger`: 日志记录器，单例模式
- `LogFormatter`: 日志格式化器
- `LogAppender`: 日志输出目标基类
- `ConsoleAppender`: 控制台输出
- `FileAppender`: 文件输出

#### 使用示例

```cpp
#include "infrastructure/logging/logger.h"

// 初始化日志系统
Logger::instance().init("logs/app.log", LogLevel::INFO);
Logger::instance().setConsoleEnabled(true);
Logger::instance().setFileEnabled(true);

// 记录不同级别的日志
LOG_DEBUG("这是一条调试日志");
LOG_INFO("这是一条信息日志");
LOG_WARNING("这是一条警告日志");
LOG_ERROR("这是一条错误日志");
LOG_FATAL("这是一条致命错误日志");
```

### 异常处理模块

异常处理模块提供了统一的异常处理机制，支持多种异常类型和异常处理策略。

#### 主要类

- `AppException`: 应用异常基类
- `ConfigException`: 配置异常类及其子类
- `FileException`: 文件异常类及其子类
- `NetworkException`: 网络异常类及其子类
- `BusinessException`: 业务异常类及其子类
- `ExceptionHandler`: 异常处理器

#### 使用示例

```cpp
#include "infrastructure/exception/app_exception.h"
#include "infrastructure/exception/file_exception.h"
#include "infrastructure/exception/exception_handler.h"

// 使用宏抛出异常
THROW_APP_EXCEPTION("这是一个应用异常");
THROW_FILE_EXCEPTION("这是一个文件异常");

// 使用异常处理器处理异常
ExceptionHandler::instance().handle([&]() {
    // 可能抛出异常的代码
    if (!QFile::exists("config.toml")) {
        throw FileNotFoundException("config.toml");
    }
});
```

### 配置管理模块

配置管理模块提供了统一的配置管理功能，支持TOML格式配置文件的读写、配置参数管理和配置变更通知。

#### 主要类

- `ConfigManager`: 配置管理器，单例模式，负责配置的读取、存储和管理

#### 使用示例

```cpp
#include "infrastructure/config/config_manager.h"

// 获取配置管理器实例
ConfigManager& configManager = ConfigManager::instance();

// 初始化配置管理器
if (!configManager.init("")) {
    LOG_ERROR("初始化配置管理器失败");
    return;
}

// 加载TOML配置文件
if (!configManager.loadTomlConfig("config.toml")) {
    LOG_ERROR("加载配置文件失败");
    return;
}

// 使用宏获取配置值
QString appName = CONFIG_GET_VALUE("app/name", "默认应用名称").toString();
int logLevel = CONFIG_GET_VALUE("log/level", 1).toInt();
bool enableDebug = CONFIG_GET_VALUE("app/debug", false).toBool();

// 使用宏设置配置值
CONFIG_SET_VALUE("app/version", "1.0.0");

// 保存配置
if (!configManager.saveConfig()) {
    LOG_ERROR("保存配置失败");
    return;
}

// 监听配置变更
connect(&configManager, &ConfigManager::configChanged, 
        [](const QString& key, const QVariant& value) {
    LOG_INFO(QString("配置已变更: %1 = %2").arg(key).arg(value.toString()));
});
```

### 文件管理模块

文件管理模块提供了统一的文件管理功能，支持文件读写、目录管理、文件监视、文件类型检测等功能。

#### 主要类

- `FileManager`: 文件管理器，单例模式
- `FileOperations`: 文件操作类
- `DirectoryWatcher`: 目录监视器
- `FileTypeDetector`: 文件类型检测器

#### 基本文件操作

```cpp
#include "infrastructure/file/file_manager.h"

// 获取文件管理器实例
FileManager& fileManager = FileManager::instance();

// 初始化文件管理器
if (!fileManager.init("data", "temp", true)) {
    LOG_ERROR("初始化文件管理器失败");
    return;
}

// 读写文本文件
QString content = "这是一个测试文件";
if (!fileManager.writeTextFile("data/test.txt", content)) {
    LOG_ERROR("写入文件失败");
    return;
}

QString readContent;
if (!fileManager.readTextFile("data/test.txt", readContent)) {
    LOG_ERROR("读取文件失败");
    return;
}

// 读写二进制文件
QByteArray data = QByteArray::fromHex("0102030405");
if (!fileManager.writeBinaryFile("data/test.bin", data)) {
    LOG_ERROR("写入二进制文件失败");
    return;
}

QByteArray readData;
if (!fileManager.readBinaryFile("data/test.bin", readData)) {
    LOG_ERROR("读取二进制文件失败");
    return;
}

// 目录操作
if (!fileManager.createDirectory("data/subdir")) {
    LOG_ERROR("创建目录失败");
    return;
}

QStringList entries;
if (!fileManager.listDirectory("data", entries)) {
    LOG_ERROR("列出目录内容失败");
    return;
}

// 文件信息
if (fileManager.exists("data/test.txt")) {
    bool isFile = fileManager.isFile("data/test.txt");
    bool isDir = fileManager.isDirectory("data/test.txt");
    qint64 size = fileManager.getFileSize("data/test.txt");
    QDateTime modTime = fileManager.getFileModifiedTime("data/test.txt");
}

// 文件操作
if (!fileManager.copyFile("data/test.txt", "data/test_copy.txt")) {
    LOG_ERROR("复制文件失败");
    return;
}

if (!fileManager.moveFile("data/test_copy.txt", "data/test_moved.txt")) {
    LOG_ERROR("移动文件失败");
    return;
}

if (!fileManager.deleteFile("data/test_moved.txt")) {
    LOG_ERROR("删除文件失败");
    return;
}

// 临时文件
QString tempFile;
if (!fileManager.createTempFile("test", ".tmp", tempFile)) {
    LOG_ERROR("创建临时文件失败");
    return;
}

QString tempDir;
if (!fileManager.createTempDirectory("test_dir", tempDir)) {
    LOG_ERROR("创建临时目录失败");
    return;
}

// 清理临时文件
fileManager.cleanupTempFiles();
```

#### 文件监视

```cpp
#include "infrastructure/file/file_manager.h"
#include "infrastructure/file/directory_watcher.h"

// 创建目录监视器
DirectoryWatcher* watcher = fileManager.createDirectoryWatcher("data");

// 连接信号
QObject::connect(watcher, &DirectoryWatcher::fileCreated, [](const QString& path) {
    LOG_INFO(QString("文件已创建: %1").arg(path));
});

QObject::connect(watcher, &DirectoryWatcher::fileModified, [](const QString& path) {
    LOG_INFO(QString("文件已修改: %1").arg(path));
});

QObject::connect(watcher, &DirectoryWatcher::fileDeleted, [](const QString& path) {
    LOG_INFO(QString("文件已删除: %1").arg(path));
});

QObject::connect(watcher, &DirectoryWatcher::directoryCreated, [](const QString& path) {
    LOG_INFO(QString("目录已创建: %1").arg(path));
});

QObject::connect(watcher, &DirectoryWatcher::directoryDeleted, [](const QString& path) {
    LOG_INFO(QString("目录已删除: %1").arg(path));
});

// 启动监视
watcher->start();

// 停止监视
watcher->stop();

// 销毁监视器
fileManager.destroyDirectoryWatcher(watcher);
```

#### 文件类型检测

```cpp
#include "infrastructure/file/file_manager.h"
#include "infrastructure/file/file_type_detector.h"

// 检测文件类型
FileType type = fileManager.detectFileType("data/test.txt");
QString typeStr = FileTypeDetector::typeToString(type);
QString mimeType = FileTypeDetector::typeToMimeType(type);

// 检查文件扩展名
bool isTextFile = fileManager.hasExtension("data/test.txt", QStringList() << "txt" << "log");
QString ext = fileManager.getFileExtension("data/test.txt");
```

#### 文件选择对话框

```cpp
#include "infrastructure/file/file_manager.h"

// 打开文件对话框
QString filePath = fileManager.getOpenFileName(
    nullptr,                          // 父窗口
    "选择文件",                       // 对话框标题
    "data",                           // 初始目录
    "文本文件 (*.txt);;所有文件 (*)" // 文件过滤器
);

// 打开多个文件对话框
QStringList filePaths = fileManager.getOpenFileNames(
    nullptr,
    "选择多个文件",
    "data",
    "文本文件 (*.txt);;所有文件 (*)"
);

// 保存文件对话框
QString savePath = fileManager.getSaveFileName(
    nullptr,
    "保存文件",
    "data",
    "文本文件 (*.txt);;所有文件 (*)"
);

// 选择目录对话框
QString dirPath = fileManager.getExistingDirectory(
    nullptr,
    "选择目录",
    "data"
);
```

#### 批量文件操作

```cpp
#include "infrastructure/file/file_manager.h"

// 批量删除文件
QStringList filesToDelete;
filesToDelete << "data/file1.txt" << "data/file2.txt" << "data/file3.txt";
int deletedCount = fileManager.batchDeleteFiles(filesToDelete);
LOG_INFO(QString("成功删除 %1 个文件").arg(deletedCount));

// 批量复制文件
QStringList filesToCopy;
filesToCopy << "data/file1.txt" << "data/file2.txt" << "data/file3.txt";
int copiedCount = fileManager.batchCopyFiles(filesToCopy, "data/backup");
LOG_INFO(QString("成功复制 %1 个文件").arg(copiedCount));

// 批量移动文件
QStringList filesToMove;
filesToMove << "data/file1.txt" << "data/file2.txt" << "data/file3.txt";
int movedCount = fileManager.batchMoveFiles(filesToMove, "data/archive");
LOG_INFO(QString("成功移动 %1 个文件").arg(movedCount));
```

#### 文件搜索

```cpp
#include "infrastructure/file/file_manager.h"

// 按名称搜索文件
QStringList files = fileManager.searchFiles("data", "*.txt", true);
LOG_INFO(QString("找到 %1 个文本文件").arg(files.size()));

// 按内容搜索文件
QStringList matchedFiles = fileManager.searchFileContent("data", "关键词", "*.txt", true);
LOG_INFO(QString("找到 %1 个包含关键词的文件").arg(matchedFiles.size()));
```

#### 文件重命名

```cpp
#include "infrastructure/file/file_manager.h"

// 重命名文件
if (!fileManager.rename("data/old_name.txt", "data/new_name.txt")) {
    LOG_ERROR("重命名文件失败");
    return;
}
```

## 应用层

### UI模块

UI模块提供了应用程序的用户界面，包括主窗口、状态栏等组件。

#### 主要类

- `MainWindow`: 主窗口
- `StatusBar`: 状态栏

#### 使用示例

```cpp
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
```

## 最佳实践

### 1. 初始化顺序

推荐的模块初始化顺序：

1. 日志模块
2. 异常处理模块
3. 文件管理模块
4. 配置管理模块
5. 其他模块

```cpp
// 初始化日志模块
Logger& logger = Logger::instance();
bool logInitResult = logger.init("logs/app.log", LogLevel::INFO, true, true);
if (!logInitResult) {
    qCritical() << "日志模块初始化失败";
    return 1;
}

// 初始化文件管理模块
FileManager& fileManager = FileManager::instance();
bool fileInitResult = fileManager.init("data", "temp", true);
if (!fileInitResult) {
    LOG_ERROR("文件管理模块初始化失败");
    return 1;
}

// 初始化配置管理模块
ConfigManager& configManager = ConfigManager::instance();
bool configInitResult = configManager.loadConfig("config.toml");
if (!configInitResult) {
    LOG_ERROR("配置管理模块初始化失败");
    return 1;
}

// 根据配置更新日志设置
QString logLevel = configManager.getValue("app.log_level", "info").toString();
logger.setLogLevel(Logger::levelFromString(logLevel));

QString logFile = configManager.getValue("app.log_file", "logs/app.log").toString();
logger.setLogFilePath(logFile);
```

### 2. 异常处理

推荐的异常处理方式：

```cpp
// 在主函数中捕获所有异常
int main(int argc, char *argv[])
{
    try {
        QApplication app(argc, argv);
        
        // 初始化模块
        // ...
        
        // 创建主窗口
        MainWindow mainWindow;
        mainWindow.show();
        
        return app.exec();
    } catch (const AppException& e) {
        // 处理应用异常
        qCritical() << "应用异常:" << e.getFormattedMessage();
        QMessageBox::critical(nullptr, "应用异常", e.getFormattedMessage());
    } catch (const std::exception& e) {
        // 处理标准异常
        qCritical() << "标准异常:" << e.what();
        QMessageBox::critical(nullptr, "标准异常", e.what());
    } catch (...) {
        // 处理未知异常
        qCritical() << "未知异常";
        QMessageBox::critical(nullptr, "未知异常", "发生未知异常");
    }
    
    return 1;
}
```

### 3. 资源清理

确保在应用程序退出前正确清理资源：

```cpp
// 在 main.cpp 中
void cleanupResources()
{
    // 清理配置管理模块
    ConfigManager::instance().saveConfig("config.toml");
    
    // 清理文件管理模块
    FileManager::instance().cleanupTempFiles();
    
    // 清理日志模块
    Logger::instance().setFileEnabled(false);
    
    qDebug() << "资源清理完成";
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 连接应用程序退出信号
    QObject::connect(&app, &QApplication::aboutToQuit, cleanupResources);
    
    // ...
    
    return app.exec();
}
```

## 常见问题

### 1. 日志模块

**Q: 如何禁用所有日志输出？**

A: 设置日志级别为 FATAL 并禁用控制台和文件输出：

```cpp
Logger& logger = Logger::instance();
logger.setLogLevel(LogLevel::FATAL);
logger.setConsoleEnabled(false);
logger.setFileEnabled(false);
```

**Q: 如何在多线程环境中使用日志模块？**

A: 日志模块是线程安全的，可以直接在多线程环境中使用。

### 2. 异常处理模块

**Q: 如何在非 GUI 环境中使用异常处理模块？**

A: 在 CMakeLists.txt 中添加 `QT_NO_WIDGETS` 宏定义，并在调用 `handle()` 方法时将第二个参数设置为 `false`：

```cpp
// 在 CMakeLists.txt 中
target_compile_definitions(YourTarget PRIVATE QT_NO_WIDGETS)

// 在代码中
ExceptionHandler::instance().handle([]() {
    // 可能抛出异常的代码
}, false); // 不显示消息框
```

### 3. 配置管理模块

**Q: 如何处理配置文件不存在的情况？**

A: 在加载配置文件失败时，可以创建默认配置：

```cpp
ConfigManager& configManager = ConfigManager::instance();
if (!configManager.loadConfig("config.toml")) {
    LOG_WARNING("配置文件不存在，使用默认配置");
    configManager.setDefaultConfig();
    configManager.saveConfig("config.toml");
}
```

**Q: 如何处理配置格式错误的情况？**

A: 使用异常处理捕获配置异常：

```cpp
try {
    ConfigManager& configManager = ConfigManager::instance();
    configManager.loadConfig("config.toml");
} catch (const ConfigException& e) {
    LOG_ERROR(QString("配置加载失败: %1").arg(e.getMessage()));
    // 使用默认配置
    ConfigManager::instance().setDefaultConfig();
}
```

### 4. 文件管理模块

**Q: 如何处理大文件读写？**

A: 使用分块读写或内存映射文件：

```cpp
// 分块读取大文件
QFile file("large_file.dat");
if (file.open(QIODevice::ReadOnly)) {
    const qint64 chunkSize = 1024 * 1024; // 1MB
    QByteArray buffer;
    
    while (!file.atEnd()) {
        buffer = file.read(chunkSize);
        // 处理数据块
        processDataChunk(buffer);
    }
    
    file.close();
}

// 使用内存映射文件
FileManager& fileManager = FileManager::instance();
char* mappedData = nullptr;
qint64 size = 0;

bool success = fileManager.mapFile("large_file.dat", mappedData, size);
if (success) {
    // 处理映射的数据
    processMemoryMappedData(mappedData, size);
    
    // 解除映射
    fileManager.unmapFile(mappedData);
}
```

**Q: 如何处理文件权限问题？**

A: 在操作文件前检查权限：

```cpp
FileManager& fileManager = FileManager::instance();

// 检查文件权限
FilePermissions permissions;
bool success = fileManager.getFilePermissions("data/config.toml", permissions);
if (success) {
    if (permissions.canRead && permissions.canWrite) {
        // 可以读写文件
    } else if (permissions.canRead) {
        // 只能读取文件
    } else {
        // 没有权限
    }
}

// 设置文件权限
permissions.canRead = true;
permissions.canWrite = true;
permissions.canExecute = false;
success = fileManager.setFilePermissions("data/script.sh", permissions);
```

## 更多资源

- [Smart Scope Qt 开发文档](./development_progress.md)
- [Smart Scope Qt 功能模块规划](../FUNCTIONAL_MODULES.md)
- [Smart Scope Qt README](../README.md) 