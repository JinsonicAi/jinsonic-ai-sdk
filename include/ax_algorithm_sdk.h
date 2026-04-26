#ifndef __AX_ALGORITHM_SDK_H__
#define __AX_ALGORITHM_SDK_H__

#ifdef __cplusplus
extern "C"
{
#endif
#define AX_ALGORITHM_MAX_OBJ_NUM 32
#define AX_ALGORITHM_FACE_POINT_LEN 5
#define AX_ALGORITHM_FACE_FEATURE_LEN 512
    typedef enum _log_level_e
    {
        ax_log_disable = -1,
        ax_log_emergency = 0,
        ax_log_alert = 1,
        ax_log_critical = 2,
        ax_log_error = 3,
        ax_log_warn = 4,
        ax_log_notice = 5,
        ax_log_info = 6,
        ax_log_debug = 7,
        ax_log_all = 8,
    } ax_log_level_e;

    typedef enum _error_code_e
    {
        ax_error_code_fail = -1,
        ax_error_code_success = 0,

        ax_error_code_init_fail = 0x10000,
        ax_error_code_init_bsp_fail,
        ax_error_code_init_license_fail,
        ax_error_code_init_model_fail,
        ax_error_code_init_model_not_exist,
        ax_error_code_init_device_not_found,
        ax_error_code_init_device_out_of_range,

        ax_error_code_run_fail = 0x20000,
        ax_error_code_run_det_fail,
        ax_error_code_run_roi_fail,
        ax_error_code_run_crop_fail,
        ax_error_code_run_align_fail,
        ax_error_code_run_quality_fail,
        ax_error_code_run_recog_fail,
        ax_error_code_run_invalid_index,
        ax_error_code_run_type_not_match,
        ax_error_code_run_no_implement,
        ax_error_code_run_invalid_image,
        ax_error_code_run_device_id_mismatch,
    } ax_error_code_e;

    typedef enum _color_space_e
    {
        ax_color_space_unknown,
        ax_color_space_nv12,
        ax_color_space_nv21,
        ax_color_space_bgr,
        ax_color_space_rgb,
    } ax_color_space_e;

    /**
     * @brief: 图像前处理后端选择（AXCL 用）
     *
     * - auto: host 输入优先走 OpenCV；device 输入走硬件（IVPS）
     * - hardware: 强制走硬件（host 输入会触发整帧上传到 device）
     * - opencv: 强制走 OpenCV（device 输入会自动回退到硬件）
     *
     * 环境变量：
     * - AISDK_IMGPROC_BACKEND=auto|hardware|opencv
     */
    typedef enum _imgproc_backend_e
    {
        ax_imgproc_backend_auto = 0,
        ax_imgproc_backend_hardware = 1,
        ax_imgproc_backend_opencv = 2,
    } ax_imgproc_backend_e;

    typedef struct _point_t
    {
        float x, y;
    } ax_point_t;

    typedef struct _bbox_t
    {
        float x, y, w, h;
    } ax_bbox_t;

    typedef struct _image_t
    {
        unsigned long long int pPhy;
        void *pVir;
        unsigned int nSize;
        unsigned int nWidth;
        unsigned int nHeight;
        ax_color_space_e eDtype;
        int tStride_W;
    } ax_image_t;

    typedef enum _model_type_e
    {
        ax_model_type_person_detection,
        ax_model_type_person_attr,

        ax_model_type_lpr,

        ax_model_type_face_detection,
        ax_model_type_face_recognition,
        ax_model_type_face_attr,

        ax_model_type_fire_smoke,
        ax_model_type_cat_dog,
        ax_model_type_violence,
        ax_model_type_motor,

        ax_model_type_end
    } ax_model_type_e;

    typedef enum _npu_affinity_e
    {
        ax_npu1_affinity_0 = 0b001,
        ax_npu1_affinity_1 = 0b010,
        ax_npu1_affinity_2 = 0b100,

        ax_npu2_affinity_0 = 0b01,
        ax_npu2_affinity_1 = 0b10,

        ax_npu3_affinity_0 = 0b1
    } ax_npu_affinity_e;

    typedef struct _body_attr_t
    {
        /**
         * track_id: 跟踪ID,用作历史状态跟踪，如果设置成 0，则不跟踪，只输出当前图像推理结果
         */
        unsigned long int track_id;

        unsigned char isHuman;          // ["Uncertain", "Normal", "Abnormal"]
        unsigned char age;              // ["Uncertain", "Toddler", "Teenager", "Youth", "Middle-aged", "Elderly"]
        unsigned char gender;           // ["Uncertain", "Male", "Female"]
        unsigned char race;             // ["Uncertain", "East Asia", "Caucasian", "African", "South Asia"]
        unsigned char umbrella;         // ["Uncertain", "No", "Yes"]
        unsigned char headwear;         // ["Uncertain", "No", "hat", "Helmet"]
        unsigned char glasses;          // ["Uncertain", "No", "Glasses", "Sunglasses"]
        unsigned char faceMask;         // ["Uncertain", "No", "Yes"]
        unsigned char smoke;            // ["Uncertain", "No", "Yes"]
        unsigned char carryingItem;     // ["Uncertain", "No", "Yes"]
        unsigned char cellphone;        // ["Uncertain", "No", "Yes"]
        unsigned char safetyClothing;   // ["Uncertain", "No", "Yes"]
        unsigned char upperWear;        // ["Uncertain", "Long-sleeve", "Short-sleeve"]
        unsigned char upperColor;       // ["Uncertain", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Pink", "Black", "White", "Gray", "Brown"]
        unsigned char upperWearFg;      // ["Uncertain", "T-shirt", "Sleeveless Top", "Shirt", "Suit", "Sweater", "Jacket", "Down Jacket", "Trench Coat", "Coat"]
        unsigned char upperWearTexture; // ["Uncertain", "Solid Color", "Pattern", "Small Floral", "Stripes or Plaid"]
        unsigned char bag;              // ["Uncertain", "No", "Crossbody Bag", "Backpack"]
        unsigned char safetyRope;       // ["Uncertain", "No", "Yes"]
        unsigned char upperCut;         // ["Uncertain", "No", "Yes"]
        unsigned char lowerWear;        // ["Uncertain", "Long Pants", "Shorts", "Long Dress", "Short Skirt"]
        unsigned char lowerColor;       // ["Uncertain", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Pink", "Black", "White", "Gray", "Brown"]
        unsigned char vehicle;          // ["Uncertain", "No", "Motorcycle", "Bicycle", "Tricycle"]
        unsigned char lowerCut;         // ["Uncertain", "No", "Yes"]
        unsigned char occlusion;        // ["Uncertain", "No", "Mild Occlusion", "Heavy Occlusion"]
        unsigned char orientation;      // ["Uncertain", "Front", "Back", "Right Side", "Left Side"]
    } ax_body_attr_t;

    typedef struct _face_attr_t
    {
        /**
         * track_id: 跟踪ID,用作历史状态跟踪，如果设置成 0，则不跟踪，只输出当前图像推理结果
         */
        unsigned long int track_id;

        unsigned char age;        // ['0-2', '3-9', '10-19', '20-29', '30-39', '40-49', '50-59', '60-69', 'more than 70']
        unsigned char gender;     // ['Female', 'Male']
        unsigned char race;       // ['Black', 'East Asian', 'Indian', 'Latino_Hispanic', 'Middle Eastern', 'Southeast Asian', 'White']
        unsigned char expression; // ["Anger", "Disgust", "Fear", "Happiness", "Neutral", "Sadness", "Surprise"]
    } ax_face_attr_t;

    typedef struct _car_attr_t
    {
        /**
         * 车辆属性: 品牌/类型/颜色
         *
         * brand:
         *   0:UNKNOWN, 1:奥迪, 2:华泰, 3:奇瑞, 4:保时捷, 5:奔驰
         *   6:福田, 7:中华, 8:兰博基尼, 9:蔚来, 10:奔腾, 11:道奇
         *   12:GMC, 13:斯柯达, 14:北京汽车, 15:东南, 16:东风, 17:特斯拉
         *   18:沃尔沃, 19:日产, 20:力帆汽车, 21:起亚, 22:江铃, 23:迈凯伦
         *   24:北汽幻速, 25:哪吒汽车, 26:帕加尼, 27:英菲尼迪, 28:广汽, 29:马自达
         *   30:一汽, 31:大众, 32:宝骏, 33:玛莎拉蒂, 34:克莱斯勒, 35:纳智捷
         *   36:雪佛兰, 37:雪铁龙, 38:雷诺, 39:铃木, 40:陆风, 41:比亚迪
         *   42:福特, 43:欧拉, 44:捷途, 45:长城, 46:标致, 47:上汽大通MAXUS
         *   48:名爵, 49:五菱汽车, 50:捷豹, 51:问界, 52:Polestar汽车, 53:欧宝
         *   54:坦克, 55:阿斯顿马丁, 56:林肯, 57:Jeep, 58:哈弗, 59:劳斯莱斯
         *   60:理想, 61:MINI, 62:凯迪拉克, 63:本田, 64:红旗, 65:别克
         *   66:长安凯程, 67:众泰, 68:金杯, 69:斯巴鲁, 70:迈莎锐, 71:DS
         *   72:智己, 73:路虎, 74:海马, 75:岚图汽车, 76:北汽威旺, 77:零跑汽车
         *   78:吉利, 79:五十铃, 80:启辰, 81:宾利, 82:法拉利, 83:ARCFOX极狐
         *   84:长安欧尚, 85:宝马, 86:江淮, 87:荣威, 88:现代, 89:长安
         *   90:三菱, 91:魏牌, 92:丰田, 93:小鹏汽车, 94:讴歌, 95:菲亚特
         *   96:领克, 97:雷克萨斯, 98:乐道, 99:享界, 100:仰望, 101:埃安
         *   102:尊界, 103:小米汽车, 104:方程豹, 105:智界, 106:极氪, 107:深蓝汽车
         *   108:腾势, 109:阿维塔
         *
         * vType:
         *   0:UNKNOWN, 1:SEDAN, 2:SUV, 3:BUS, 4:MICROBUS, 5:TRUCK, 6:BICYCLE, 7:MOTORCYCLE, 8:ELECTRIC_VEHICLE
         *
         * vColor:
         *   0:UNKNOWN, 1:BROWN, 2:ORANGE, 3:GRAY, 4:WHITE, 5:PINK, 6:PURPLE, 7:RED, 8:GREEN, 9:BLUE, 10:SILVER, 11:YELLOW, 12:BLACK
         *
         * 类别表源文件: algo_models/vehicle_attrs.txt
         */
        unsigned char brand;
        unsigned char vType;
        unsigned char vColor;
    } ax_car_attr_t;

    typedef struct _object_t
    {
        ax_bbox_t bbox;
        float score;
        int label;
        unsigned long int track_id;

        struct
        {
            /*
            0到1之间的值，表示人脸质量，越高越好
            */
            float quality;
            ax_point_t points[AX_ALGORITHM_FACE_POINT_LEN];
        } face_info;

        struct
        {
            /*
            人体状态： 0：正面， 1：侧面，2：背面， 3：非人
            */
            int status;
        } person_info;

        struct
        {
            /**
            火、烟、其他
            */
            int label;
            float score;
        } fire_smoke_info;

        struct
        {
            /*
            如果 b_is_track_plate = 1，则表示当前帧没有识别到车牌，返回的是历史上 track_id 上一次识别到的车牌结果
            如果 b_is_track_plate = 0，且 len_plate_id > 0, 则表示当前帧识别到了车牌
            如果 b_is_track_plate = 0，且 len_plate_id = 0, 则表示当前帧没有识别到车牌，且是历史上 track_id 也没有结果
            */
            int b_is_track_plate;
            int len_plate_id;
            int plate_id[16];
        } vehicle_info;

        struct
        {
            /**
            猫、狗、其他
            */
            int label;
        } pet_info;

        struct
        {
            /**
             * motorcycle、bicycle、person
             */
            int label;
        } motor_info;

    } ax_object_t;

    typedef struct _result_t
    {
        ax_model_type_e model_type;
        ax_object_t objects[AX_ALGORITHM_MAX_OBJ_NUM];
        int n_objects;
    } ax_result_t;

    typedef struct _algorithm_param_t
    {
        /**
         * det_threshold: 检测阈值，0-1之间
         */
        float det_threshold;
        struct
        {
            /**
             *  quality_threshold: 人脸质量评分阈值，0-1之间
             */
            float quality_threshold;
        } face_param;

        struct
        {
            /**
             *  fire_smoke_threshold: 置信度阈值，0-1之间，低于这个阈值label返回 其他
             */
            float fire_smoke_threshold;
        } fire_smoke_param;

        struct
        {
            /**
             *  lpr_threshold: 车牌识别阈值，0-1之间
             */
            float lpr_threshold;
        } vehicle_param;
    } ax_algorithm_param_t;

    typedef void *ax_algorithm_handle_t;

    typedef struct _algorithm_init_t
    {
        char license_path[256];
        char model_file[256];
        ax_model_type_e model_type;
        ax_algorithm_param_t param;
        int device_id; // for accelerometer card
    } ax_algorithm_init_t;

    typedef struct
    {
        int device_id; // -1 for host, > 0 for accelerometer card index
        char fingerprint[256];
    } ax_algorithm_fingerprint_t;

    int ax_algorithm_init(ax_algorithm_init_t *init_info, ax_algorithm_handle_t *handle);
    void ax_algorithm_deinit(ax_algorithm_handle_t handle);

    int ax_algorithm_get_fingerprint(ax_algorithm_fingerprint_t *fingerprint);

    /**
     * @brief: 设置算法的亲和性（目前都是NPU1模型）
     * @param[in] handle: 算法句柄
     * @param[in] affinity: 亲和性
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_set_affinity(ax_algorithm_handle_t handle, ax_npu_affinity_e affinity);

    /**
     * @brief: 设置图像前处理后端（handle 级别）
     * @param[in] handle: 算法句柄
     * @param[in] backend: 前处理后端
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_set_imgproc_backend_for_handle(ax_algorithm_handle_t handle, ax_imgproc_backend_e backend);
    ax_imgproc_backend_e ax_algorithm_get_imgproc_backend_for_handle(ax_algorithm_handle_t handle);

    /**
     * @brief: 设置图像前处理后端（全局默认，影响未设置 handle override 的句柄）
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_set_imgproc_backend(ax_imgproc_backend_e backend);
    ax_imgproc_backend_e ax_algorithm_get_imgproc_backend(void);

    /**
     * @brief: 与ax_algorithm_track的区别是不进行跟踪, 一般只用作精度验证，或者人脸注册的检测阶段
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[out] result: 检测结果
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_detect(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result);
    /**
     * @brief: 检测+跟踪等一系列算法
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[out] result: 跟踪结果
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_track(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result);

    /**
     * @brief: 重置算法状态，主要是跟踪信息
     * @param[in] handle: 算法句柄
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_reset(ax_algorithm_handle_t handle);

    ax_model_type_e ax_algorithm_get_model_type(ax_algorithm_handle_t handle);

    /**
     * @brief: 获取算法参数
     * @param[in] handle: 算法句柄
     * @return 算法参数
     */
    ax_algorithm_param_t ax_algorithm_get_param(ax_algorithm_handle_t handle);
    /**
     * @brief: 设置算法参数
     * @param[in] handle: 算法句柄
     * @param[in] param: 算法参数
     */
    void ax_algorithm_set_param(ax_algorithm_handle_t handle, ax_algorithm_param_t *param);
    /**
     * @brief: 获取默认算法参数
     * @return 算法参数
     */
    ax_algorithm_param_t ax_algorithm_get_default_param();

    /**
     * @brief: 设置日志级别
     * @param[in] level: 小于level的日志将被打印，大于level的日志将被忽略
     */
    void ax_algorithm_set_log_level(ax_log_level_e level);

    /**
     * @brief: 保存调试图像
     * @param[in] handle: 算法句柄
     * @param[in] enable: 1: 启用保存调试图像, 0: 禁用
     */
    void ax_algorithm_save_debug_image(ax_algorithm_handle_t handle, int enable);

    /**
     * @brief: 车牌识别
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[out] plate_id: 识别到的车牌id
     * @param[out] len: plate_id 数组的长度
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_plate_id(ax_algorithm_handle_t handle, ax_image_t *image, int plate_id[16], int *len_plate_id);

    /**
     * @brief: 将 plate_id 转换为字符串
     * @param[in] plate_id: plate_id 数组
     * @param[in] len: plate_id 数组的长度
     * @param[out] plate_str: plate_id 的字符串
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_plate_str(int *plate_id, int len, char *plate_str);

    /**
     * @brief: 获取车辆属性(品牌/类型/颜色)
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[in] bbox: 车辆 bounding box, 传 NULL 表示整图
     * @param[out] car_attr: 车辆属性
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_car_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_bbox_t *bbox, ax_car_attr_t *car_attr);

    /**
     * @brief: 获取检测到的人体属性
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[in] bbox: 检测到的人体 bounding box
     * @param[out] body_attr: 检测到的人体属性
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_body_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_bbox_t *bbox, ax_body_attr_t *body_attr);

    /**
     * @brief: 获取检测到的人脸属性
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[in] result: 检测结果
     * @param[in] idx: result.objects 中的人脸索引,此api不会自动检测人脸
     *                  - 视频流:使用 ax_algorithm_track 返回的 results 中所需的 idx，
     *                  - 单张图:使用 ax_algorithm_detect 进行非跟踪人脸检测
     * @param[out] face_attr: 检测到的人脸属性
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_face_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result, int idx, ax_face_attr_t *face_attr);

    /**
     * @brief: 获取人脸属性
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据(头肩图像, 会自动检测人脸)
     * @param[out] feature: 检测到的人脸属性
     */
    int ax_algorithm_get_face_attr_2(ax_algorithm_handle_t handle, ax_image_t *image, ax_object_t *obj, ax_face_attr_t *face_attr);

    /**
     * @brief: 获取人脸质量
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据(头肩图像, 会自动检测人脸)
     * @param[out] quality: 人脸质量
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_face_quality(ax_algorithm_handle_t handle, ax_image_t *image, float *quality);

    /**
     * @brief: 获取检测到的人脸特征
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据
     * @param[in] result: 检测结果
     * @param[in] idx: result.objects 中的人脸索引,此api不会自动检测人脸
     *                  - 视频流:使用 ax_algorithm_track 返回的 results 中所需的 idx，
     *                  - 单张图:使用 ax_algorithm_detect 进行非跟踪人脸检测
     * @param[out] feature: 512维的人脸特征
     * @return 0 成功，非零表示失败。
     */
    int ax_algorithm_get_face_feature(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result, int idx, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
     * @brief: 获取人脸特征
     * @param[in] handle: 算法句柄
     * @param[in] image: 图像数据(头肩图像, 会自动检测人脸)
     * @param[out] feature: 512维的人脸特征
     */
    int ax_algorithm_get_face_feature_2(ax_algorithm_handle_t handle, ax_image_t *image, ax_object_t *obj, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
     * @brief: 比较两个人脸特征
     * @param[in] a: 第一个人脸特征数组
     * @param[in] b: 第二个人脸特征数组
     * @return 两个人脸特征之间的相似度评分
     */
    float ax_algorithm_face_compare(float a[AX_ALGORITHM_FACE_FEATURE_LEN], float b[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
     * @brief: 根据指定的参数创建一幅图像。
     * @param[in] width: 图像的宽度。
     * @param[in] height: 图像的高度。
     * @param[in] stride: 图像的步幅。
     * @param[in] color: 图像的颜色空间 (例如，NV12, NV21, BGR, RGB)。
     * @param[out] image: 指向图像结构体的指针，用于初始化。
     * @return 成功时返回 0，失败时返回非零值。
     */
    int ax_create_image(int width, int height, int stride, ax_color_space_e color, ax_image_t *image, int device_id);

    /**
     * @brief: 释放ax_create_image创建的一幅图像。
     * @param[in] image: 指向要释放的图像结构体的指针。
     */
    void ax_release_image(ax_image_t *image, int device_id);

#ifdef __cplusplus
}

#endif

#endif // __AX_ALGORITHM_SDK_H__
