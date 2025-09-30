public:
    // ... existing code ...
    
    /**
     * @brief 从HomePage设置图像
     * @param leftImage 左相机图像
     * @param rightImage 右相机图像
     * @return 是否成功设置
     */
    bool setImagesFromHomePage(const cv::Mat& leftImage, const cv::Mat& rightImage);
    
    /**
     * @brief 更新相机ID
     * 当相机配置变更时调用此方法更新ID
     */
    void updateCameraIds();
    
private slots:
    // ... existing code ... 