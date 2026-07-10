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
    * @brief Image preprocessing backend selection for AXCL.
     *
    * - auto: Prefer OpenCV for host input and hardware (IVPS) for device input.
    * - hardware: Force hardware processing; host input causes a full-frame upload to the device.
    * - opencv: Force OpenCV; device input automatically falls back to hardware processing.
     *
    * Environment variable:
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
         * track_id: Tracking ID used to maintain historical state. Set to 0 to disable tracking and
         * output only the inference result for the current image.
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
         * track_id: Tracking ID used to maintain historical state. Set to 0 to disable tracking and
         * output only the inference result for the current image.
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
         * Vehicle attributes: brand, type, and color.
         *
         * brand:
         *   0:UNKNOWN, 1:AUDI, 2:HAWTAI, 3:CHERY, 4:PORSCHE, 5:MERCEDES_BENZ
         *   6:FOTON, 7:BRILLIANCE, 8:LAMBORGHINI, 9:NIO, 10:BESTUNE, 11:DODGE
         *   12:GMC, 13:SKODA, 14:BAIC, 15:SOUTHEAST, 16:DONGFENG, 17:TESLA
         *   18:VOLVO, 19:NISSAN, 20:LIFAN, 21:KIA, 22:JMC, 23:MCLAREN
         *   24:BAIC_HUANSU, 25:NETA, 26:PAGANI, 27:INFINITI, 28:GAC, 29:MAZDA
         *   30:FAW, 31:VOLKSWAGEN, 32:BAOJUN, 33:MASERATI, 34:CHRYSLER, 35:LUXGEN
         *   36:CHEVROLET, 37:CITROEN, 38:RENAULT, 39:SUZUKI, 40:LANDWIND, 41:BYD
         *   42:FORD, 43:ORA, 44:JETOUR, 45:GREAT_WALL, 46:PEUGEOT, 47:SAIC_MAXUS
         *   48:MG, 49:WULING, 50:JAGUAR, 51:AITO, 52:POLESTAR, 53:OPEL
         *   54:TANK, 55:ASTON_MARTIN, 56:LINCOLN, 57:JEEP, 58:HAVAL, 59:ROLLS_ROYCE
         *   60:LI_AUTO, 61:MINI, 62:CADILLAC, 63:HONDA, 64:HONGQI, 65:BUICK
         *   66:CHANGAN_KAICENE, 67:ZOTYE, 68:JINBEI, 69:SUBARU, 70:MSRT, 71:DS
         *   72:IM_MOTORS, 73:LAND_ROVER, 74:HAIMA, 75:VOYAH, 76:BAIC_WEIWANG, 77:LEAPMOTOR
         *   78:GEELY, 79:ISUZU, 80:VENUCIA, 81:BENTLEY, 82:FERRARI, 83:ARCFOX
         *   84:CHANGAN_OSHAN, 85:BMW, 86:JAC, 87:ROEWE, 88:HYUNDAI, 89:CHANGAN
         *   90:MITSUBISHI, 91:WEY, 92:TOYOTA, 93:XPENG, 94:ACURA, 95:FIAT
         *   96:LYNK_AND_CO, 97:LEXUS, 98:ONVO, 99:STELATO, 100:YANGWANG, 101:AION
         *   102:MAEXTRO, 103:XIAOMI_AUTO, 104:FANGCHENGBAO, 105:LUXEED, 106:ZEEKR, 107:DEEPAL
         *   108:DENZA, 109:AVATR
         *
         * vType:
         *   0:UNKNOWN, 1:SEDAN, 2:SUV, 3:BUS, 4:MICROBUS, 5:TRUCK, 6:BICYCLE, 7:MOTORCYCLE, 8:ELECTRIC_VEHICLE
         *
         * vColor:
         *   0:UNKNOWN, 1:BROWN, 2:ORANGE, 3:GRAY, 4:WHITE, 5:PINK, 6:PURPLE, 7:RED, 8:GREEN, 9:BLUE, 10:SILVER, 11:YELLOW, 12:BLACK
         *
         * Class table source: algo_models/vehicle_attrs.txt
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
            A value from 0 to 1 indicating face quality; higher is better.
            */
            float quality;
            ax_point_t points[AX_ALGORITHM_FACE_POINT_LEN];
        } face_info;

        struct
        {
            /*
            Body orientation: 0 front, 1 side, 2 back, 3 non-human.
            */
            int status;
        } person_info;

        struct
        {
            /**
            Fire, smoke, or other.
            */
            int label;
            float score;
        } fire_smoke_info;

        struct
        {
            /*
            If b_is_track_plate = 1, no plate was recognized in the current frame; the most recent historical
            plate result for track_id is returned.
            If b_is_track_plate = 0 and len_plate_id > 0, a plate was recognized in the current frame.
            If b_is_track_plate = 0 and len_plate_id = 0, neither the current frame nor the history for track_id
            contains a plate result.
            */
            int b_is_track_plate;
            int len_plate_id;
            int plate_id[16];
        } vehicle_info;

        struct
        {
            /**
            Cat, dog, or other.
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
         * det_threshold: Detection threshold in the range [0, 1].
         */
        float det_threshold;
        struct
        {
            /**
             * quality_threshold: Face-quality score threshold in the range [0, 1].
             */
            float quality_threshold;
        } face_param;

        struct
        {
            /**
             * fire_smoke_threshold: Confidence threshold in the range [0, 1]. Labels below it are returned as other.
             */
            float fire_smoke_threshold;
        } fire_smoke_param;

        struct
        {
            /**
             * lpr_threshold: License plate recognition threshold in the range [0, 1].
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
    * @brief Set algorithm affinity. All current models use NPU1.
    * @param[in] handle Algorithm handle.
    * @param[in] affinity NPU affinity.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_set_affinity(ax_algorithm_handle_t handle, ax_npu_affinity_e affinity);

    /**
    * @brief Set the image preprocessing backend for one handle.
    * @param[in] handle Algorithm handle.
    * @param[in] backend Preprocessing backend.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_set_imgproc_backend_for_handle(ax_algorithm_handle_t handle, ax_imgproc_backend_e backend);
    ax_imgproc_backend_e ax_algorithm_get_imgproc_backend_for_handle(ax_algorithm_handle_t handle);

    /**
    * @brief Set the global default image preprocessing backend for handles without an override.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_set_imgproc_backend(ax_imgproc_backend_e backend);
    ax_imgproc_backend_e ax_algorithm_get_imgproc_backend(void);

    /**
    * @brief Detect objects without tracking. Typically used for accuracy validation or the detection phase of face enrollment.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[out] result Detection result.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_detect(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result);
    /**
    * @brief Run detection, tracking, and related algorithms.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[out] result Tracking result.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_track(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result);

    /**
    * @brief Reset algorithm state, primarily tracking information.
    * @param[in] handle Algorithm handle.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_reset(ax_algorithm_handle_t handle);

    ax_model_type_e ax_algorithm_get_model_type(ax_algorithm_handle_t handle);

    /**
    * @brief Get algorithm parameters.
    * @param[in] handle Algorithm handle.
    * @return Algorithm parameters.
     */
    ax_algorithm_param_t ax_algorithm_get_param(ax_algorithm_handle_t handle);
    /**
    * @brief Set algorithm parameters.
    * @param[in] handle Algorithm handle.
    * @param[in] param Algorithm parameters.
     */
    void ax_algorithm_set_param(ax_algorithm_handle_t handle, ax_algorithm_param_t *param);
    /**
    * @brief Get default algorithm parameters.
    * @return Default algorithm parameters.
     */
    ax_algorithm_param_t ax_algorithm_get_default_param();

    /**
    * @brief Set the log level.
    * @param[in] level Messages at or below this level are printed; messages above it are ignored.
     */
    void ax_algorithm_set_log_level(ax_log_level_e level);

    /**
    * @brief Save debug images.
    * @param[in] handle Algorithm handle.
    * @param[in] enable 1 to enable debug image saving; 0 to disable it.
     */
    void ax_algorithm_save_debug_image(ax_algorithm_handle_t handle, int enable);

    /**
    * @brief Recognize a license plate.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[out] plate_id Recognized license plate ID.
    * @param[out] len Length of the plate_id array.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_plate_id(ax_algorithm_handle_t handle, ax_image_t *image, int plate_id[16], int *len_plate_id);

    /**
    * @brief Convert plate_id to a string.
    * @param[in] plate_id plate_id array.
    * @param[in] len Length of the plate_id array.
    * @param[out] plate_str String representation of plate_id.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_plate_str(int *plate_id, int len, char *plate_str);

    /**
    * @brief Get vehicle attributes: brand, type, and color.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[in] bbox Vehicle bounding box; pass NULL for the entire image.
    * @param[out] car_attr Vehicle attributes.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_car_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_bbox_t *bbox, ax_car_attr_t *car_attr);

    /**
    * @brief Get attributes for a detected person.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[in] bbox Bounding box of the detected person.
    * @param[out] body_attr Attributes of the detected person.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_body_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_bbox_t *bbox, ax_body_attr_t *body_attr);

    /**
    * @brief Get attributes for a detected face.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[in] result Detection result.
    * @param[in] idx Index of the face in result.objects. This API does not detect faces automatically.
    *                  - Video stream: use the required idx from results returned by ax_algorithm_track.
    *                  - Single image: use ax_algorithm_detect for non-tracking face detection.
    * @param[out] face_attr Attributes of the detected face.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_face_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result, int idx, ax_face_attr_t *face_attr);

    /**
    * @brief Get face attributes.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data containing a head-and-shoulders image; faces are detected automatically.
        * @param[out] face_attr Attributes of the detected face.
     */
    int ax_algorithm_get_face_attr_2(ax_algorithm_handle_t handle, ax_image_t *image, ax_object_t *obj, ax_face_attr_t *face_attr);

    /**
    * @brief Get face quality.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data containing a head-and-shoulders image; faces are detected automatically.
    * @param[out] quality Face quality.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_face_quality(ax_algorithm_handle_t handle, ax_image_t *image, float *quality);

    /**
    * @brief Get features for a detected face.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data.
    * @param[in] result Detection result.
    * @param[in] idx Index of the face in result.objects. This API does not detect faces automatically.
    *                  - Video stream: use the required idx from results returned by ax_algorithm_track.
    *                  - Single image: use ax_algorithm_detect for non-tracking face detection.
    * @param[out] feature 512-dimensional face feature vector.
    * @return 0 on success; non-zero on failure.
     */
    int ax_algorithm_get_face_feature(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result, int idx, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
    * @brief Get face features.
    * @param[in] handle Algorithm handle.
    * @param[in] image Image data containing a head-and-shoulders image; faces are detected automatically.
    * @param[out] feature 512-dimensional face feature vector.
     */
    int ax_algorithm_get_face_feature_2(ax_algorithm_handle_t handle, ax_image_t *image, ax_object_t *obj, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
    * @brief Compare two face feature vectors.
    * @param[in] a First face feature vector.
    * @param[in] b Second face feature vector.
    * @return Similarity score between the two face feature vectors.
     */
    float ax_algorithm_face_compare(float a[AX_ALGORITHM_FACE_FEATURE_LEN], float b[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
    * @brief Create an image with the specified parameters.
    * @param[in] width Image width.
    * @param[in] height Image height.
    * @param[in] stride Image stride.
    * @param[in] color Image color space, for example NV12, NV21, BGR, or RGB.
    * @param[out] image Pointer to the image structure to initialize.
    * @return 0 on success; non-zero on failure.
     */
    int ax_create_image(int width, int height, int stride, ax_color_space_e color, ax_image_t *image, int device_id);

    /**
    * @brief Release an image created by ax_create_image.
    * @param[in] image Pointer to the image structure to release.
     */
    void ax_release_image(ax_image_t *image, int device_id);

#ifdef __cplusplus
}

#endif

#endif // __AX_ALGORITHM_SDK_H__
