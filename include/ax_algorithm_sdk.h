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
     * @brief: Image preprocessing backend selection (for AXCL)
     *
     * - auto: host input prefers OpenCV; device input uses hardware (IVPS)
     * - hardware: force hardware (host input triggers uploading the whole frame to the device)
     * - opencv: force OpenCV (device input automatically falls back to hardware)
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
         * track_id: tracking ID, used for historical state tracking. If set to 0, no tracking is
         * performed and only the inference result of the current image is output.
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
         * track_id: tracking ID, used for historical state tracking. If set to 0, no tracking is
         * performed and only the inference result of the current image is output.
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
         * Vehicle attributes: brand / type / color
         *
         * brand:
         *   0:UNKNOWN, 1:Audi, 2:Hawtai, 3:Chery, 4:Porsche, 5:Mercedes-Benz
         *   6:Foton, 7:Brilliance, 8:Lamborghini, 9:NIO, 10:Besturn, 11:Dodge
         *   12:GMC, 13:Skoda, 14:BAIC Motor, 15:Soueast, 16:Dongfeng, 17:Tesla
         *   18:Volvo, 19:Nissan, 20:Lifan, 21:Kia, 22:JMC, 23:McLaren
         *   24:BAIC Huansu, 25:Neta, 26:Pagani, 27:Infiniti, 28:GAC, 29:Mazda
         *   30:FAW, 31:Volkswagen, 32:Baojun, 33:Maserati, 34:Chrysler, 35:Luxgen
         *   36:Chevrolet, 37:Citroen, 38:Renault, 39:Suzuki, 40:Landwind, 41:BYD
         *   42:Ford, 43:ORA, 44:Jetour, 45:Great Wall, 46:Peugeot, 47:SAIC Maxus
         *   48:MG, 49:Wuling, 50:Jaguar, 51:AITO, 52:Polestar, 53:Opel
         *   54:Tank, 55:Aston Martin, 56:Lincoln, 57:Jeep, 58:Haval, 59:Rolls-Royce
         *   60:Li Auto, 61:MINI, 62:Cadillac, 63:Honda, 64:Hongqi, 65:Buick
         *   66:Changan Kaicene, 67:Zotye, 68:Jinbei, 69:Subaru, 70:Maxus (Maisharui), 71:DS
         *   72:IM Motors, 73:Land Rover, 74:Haima, 75:Voyah, 76:BAIC Weiwang, 77:Leapmotor
         *   78:Geely, 79:Isuzu, 80:Venucia, 81:Bentley, 82:Ferrari, 83:ARCFOX
         *   84:Changan Oushang, 85:BMW, 86:JAC, 87:Roewe, 88:Hyundai, 89:Changan
         *   90:Mitsubishi, 91:Wey, 92:Toyota, 93:XPeng, 94:Acura, 95:Fiat
         *   96:Lynk & Co, 97:Lexus, 98:Onvo, 99:Stelato, 100:Yangwang, 101:Aion
         *   102:Maextro, 103:Xiaomi, 104:Fangchengbao, 105:Luxeed, 106:Zeekr, 107:Deepal
         *   108:Denza, 109:Avatr
         *
         * vType:
         *   0:UNKNOWN, 1:SEDAN, 2:SUV, 3:BUS, 4:MICROBUS, 5:TRUCK, 6:BICYCLE, 7:MOTORCYCLE, 8:ELECTRIC_VEHICLE
         *
         * vColor:
         *   0:UNKNOWN, 1:BROWN, 2:ORANGE, 3:GRAY, 4:WHITE, 5:PINK, 6:PURPLE, 7:RED, 8:GREEN, 9:BLUE, 10:SILVER, 11:YELLOW, 12:BLACK
         *
         * Category table source file: algo_models/vehicle_attrs.txt
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
            A value between 0 and 1 indicating face quality; higher is better
            */
            float quality;
            ax_point_t points[AX_ALGORITHM_FACE_POINT_LEN];
        } face_info;

        struct
        {
            /*
            Body status: 0: front, 1: side, 2: back, 3: non-human
            */
            int status;
        } person_info;

        struct
        {
            /**
            fire, smoke, other
            */
            int label;
            float score;
        } fire_smoke_info;

        struct
        {
            /*
            If b_is_track_plate = 1, the current frame did not recognize a plate; the returned result is the
            last plate recognized historically for this track_id.
            If b_is_track_plate = 0 and len_plate_id > 0, a plate was recognized in the current frame.
            If b_is_track_plate = 0 and len_plate_id = 0, no plate was recognized in the current frame and
            there is also no historical result for this track_id.
            */
            int b_is_track_plate;
            int len_plate_id;
            int plate_id[16];
        } vehicle_info;

        struct
        {
            /**
            cat, dog, other
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
         * det_threshold: detection threshold, between 0 and 1
         */
        float det_threshold;
        struct
        {
            /**
             *  quality_threshold: face quality score threshold, between 0 and 1
             */
            float quality_threshold;
        } face_param;

        struct
        {
            /**
             *  fire_smoke_threshold: confidence threshold, between 0 and 1; below this threshold the label returns "other"
             */
            float fire_smoke_threshold;
        } fire_smoke_param;

        struct
        {
            /**
             *  lpr_threshold: license plate recognition threshold, between 0 and 1
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
     * @brief: Set the algorithm affinity (currently all are NPU1 models)
     * @param[in] handle: algorithm handle
     * @param[in] affinity: affinity
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_set_affinity(ax_algorithm_handle_t handle, ax_npu_affinity_e affinity);

    /**
     * @brief: Set the image preprocessing backend (handle level)
     * @param[in] handle: algorithm handle
     * @param[in] backend: preprocessing backend
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_set_imgproc_backend_for_handle(ax_algorithm_handle_t handle, ax_imgproc_backend_e backend);
    ax_imgproc_backend_e ax_algorithm_get_imgproc_backend_for_handle(ax_algorithm_handle_t handle);

    /**
     * @brief: Set the image preprocessing backend (global default; affects handles without a handle-level override)
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_set_imgproc_backend(ax_imgproc_backend_e backend);
    ax_imgproc_backend_e ax_algorithm_get_imgproc_backend(void);

    /**
     * @brief: Difference from ax_algorithm_track is that no tracking is performed; typically used only for accuracy
     *         verification or the detection stage of face registration
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[out] result: detection result
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_detect(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result);
    /**
     * @brief: A series of algorithms such as detection + tracking
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[out] result: tracking result
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_track(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result);

    /**
     * @brief: Reset the algorithm state, mainly the tracking information
     * @param[in] handle: algorithm handle
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_reset(ax_algorithm_handle_t handle);

    ax_model_type_e ax_algorithm_get_model_type(ax_algorithm_handle_t handle);

    /**
     * @brief: Get the algorithm parameters
     * @param[in] handle: algorithm handle
     * @return algorithm parameters
     */
    ax_algorithm_param_t ax_algorithm_get_param(ax_algorithm_handle_t handle);
    /**
     * @brief: Set the algorithm parameters
     * @param[in] handle: algorithm handle
     * @param[in] param: algorithm parameters
     */
    void ax_algorithm_set_param(ax_algorithm_handle_t handle, ax_algorithm_param_t *param);
    /**
     * @brief: Get the default algorithm parameters
     * @return algorithm parameters
     */
    ax_algorithm_param_t ax_algorithm_get_default_param();

    /**
     * @brief: Set the log level
     * @param[in] level: logs below level will be printed; logs above level will be ignored
     */
    void ax_algorithm_set_log_level(ax_log_level_e level);

    /**
     * @brief: Save debug images
     * @param[in] handle: algorithm handle
     * @param[in] enable: 1: enable saving debug images, 0: disable
     */
    void ax_algorithm_save_debug_image(ax_algorithm_handle_t handle, int enable);

    /**
     * @brief: License plate recognition
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[out] plate_id: the recognized license plate id
     * @param[out] len: length of the plate_id array
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_plate_id(ax_algorithm_handle_t handle, ax_image_t *image, int plate_id[16], int *len_plate_id);

    /**
     * @brief: Convert plate_id to a string
     * @param[in] plate_id: plate_id array
     * @param[in] len: length of the plate_id array
     * @param[out] plate_str: the string form of plate_id
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_plate_str(int *plate_id, int len, char *plate_str);

    /**
     * @brief: Get vehicle attributes (brand/type/color)
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[in] bbox: vehicle bounding box; pass NULL for the whole image
     * @param[out] car_attr: vehicle attributes
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_car_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_bbox_t *bbox, ax_car_attr_t *car_attr);

    /**
     * @brief: Get the detected body attributes
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[in] bbox: the detected body bounding box
     * @param[out] body_attr: the detected body attributes
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_body_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_bbox_t *bbox, ax_body_attr_t *body_attr);

    /**
     * @brief: Get the detected face attributes
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[in] result: detection result
     * @param[in] idx: index of the face in result.objects; this API does not detect faces automatically
     *                  - video stream: use the required idx from the results returned by ax_algorithm_track,
     *                  - single image: use ax_algorithm_detect for non-tracking face detection
     * @param[out] face_attr: the detected face attributes
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_face_attr(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result, int idx, ax_face_attr_t *face_attr);

    /**
     * @brief: Get face attributes
     * @param[in] handle: algorithm handle
     * @param[in] image: image data (head-and-shoulders image; faces are detected automatically)
     * @param[out] feature: the detected face attributes
     */
    int ax_algorithm_get_face_attr_2(ax_algorithm_handle_t handle, ax_image_t *image, ax_object_t *obj, ax_face_attr_t *face_attr);

    /**
     * @brief: Get face quality
     * @param[in] handle: algorithm handle
     * @param[in] image: image data (head-and-shoulders image; faces are detected automatically)
     * @param[out] quality: face quality
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_face_quality(ax_algorithm_handle_t handle, ax_image_t *image, float *quality);

    /**
     * @brief: Get the detected face feature
     * @param[in] handle: algorithm handle
     * @param[in] image: image data
     * @param[in] result: detection result
     * @param[in] idx: index of the face in result.objects; this API does not detect faces automatically
     *                  - video stream: use the required idx from the results returned by ax_algorithm_track,
     *                  - single image: use ax_algorithm_detect for non-tracking face detection
     * @param[out] feature: 512-dimensional face feature
     * @return 0 on success, non-zero on failure.
     */
    int ax_algorithm_get_face_feature(ax_algorithm_handle_t handle, ax_image_t *image, ax_result_t *result, int idx, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
     * @brief: Get face feature
     * @param[in] handle: algorithm handle
     * @param[in] image: image data (head-and-shoulders image; faces are detected automatically)
     * @param[out] feature: 512-dimensional face feature
     */
    int ax_algorithm_get_face_feature_2(ax_algorithm_handle_t handle, ax_image_t *image, ax_object_t *obj, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
     * @brief: Compare two face features
     * @param[in] a: the first face feature array
     * @param[in] b: the second face feature array
     * @return the similarity score between the two face features
     */
    float ax_algorithm_face_compare(float a[AX_ALGORITHM_FACE_FEATURE_LEN], float b[AX_ALGORITHM_FACE_FEATURE_LEN]);

    /**
     * @brief: Create an image with the specified parameters.
     * @param[in] width: image width.
     * @param[in] height: image height.
     * @param[in] stride: image stride.
     * @param[in] color: image color space (e.g., NV12, NV21, BGR, RGB).
     * @param[out] image: pointer to the image struct to initialize.
     * @return 0 on success, non-zero on failure.
     */
    int ax_create_image(int width, int height, int stride, ax_color_space_e color, ax_image_t *image, int device_id);

    /**
     * @brief: Release an image created by ax_create_image.
     * @param[in] image: pointer to the image struct to release.
     */
    void ax_release_image(ax_image_t *image, int device_id);

#ifdef __cplusplus
}

#endif

#endif // __AX_ALGORITHM_SDK_H__
