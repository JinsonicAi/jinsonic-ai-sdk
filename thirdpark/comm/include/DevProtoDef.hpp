#ifndef __DEV_PROTO_DEF_HPP__
#define __DEV_PROTO_DEF_HPP__

/**
 * @brief 人员类型枚举
 */
enum class PersonType : int {
	NONE	  = 0,		 // 无特殊类型
	STRANGER  = 1 << 0,	 // 陌生人
	WHITELIST = 1 << 1,	 // 白名单人员(可信人员)
	BLACKLIST = 1 << 2,	 // 黑名单人员(可疑/危险人员)
	VIP		  = 1 << 3,	 // VIP人员(重要人员)
	VISITOR	  = 1 << 4	 // 访客(临时访问人员)
};

/**
 * @brief 报警人员类别枚举
 */
enum class AlarmCategory : int {
	WHITELIST = 1,	// 白名单人员(可信人员)
	BLACKLIST = 2,	// 黑名单人员(可疑/危险人员)
	VIP		  = 3,	// VIP人员(重要人员)
	VISITOR	  = 4	// 访客(临时访问人员)
};

/**
 * @brief 智能分析事件类型枚举
 */
enum class EventType : int {
	MOTION_DETECTION	  = 1,	 // 移动侦测报警
	INTRUSION_DETECTION	  = 2,	 // 智能分析报警:区域入侵或绊线检测报警
	VIDEO_BLIND			  = 3,	 // 智能分析报警:视频遮挡报警
	BABY_CRYING			  = 4,	 // 智能分析报警:婴儿啼哭报警
	CROWD_GATHERING		  = 5,	 // 智能分析报警:人员聚集检测报警
	PERSON_LOITERING	  = 6,	 // 智能分析报警:人员徘徊检测报警
	FAST_MOVING			  = 7,	 // 智能分析报警:快速移动报警
	FIRE_ALARM			  = 8,	 // 火焰报警
	SOUND_SPIKE			  = 9,	 // 智能分析报警:音量陡升报警
	NOISE_DROP			  = 10,	 // 智能分析报警:环境噪声陡降报警
	HUMAN_DETECTION		  = 11,	 // 人形检测实时抓拍
	WHITELIST_DETECTION	  = 12,	 // 白名单检测
	BLACKLIST_DETECTION	  = 13,	 // 黑名单检测
	VIP_DETECTION		  = 14,	 // VIP检测
	POST_ABANDONMENT	  = 15,	 // 离岗检测
	FALL_DETECTION		  = 16,	 // 跌倒检测
	VITAL_SIGN_DETECTION  = 17,	 // 呼吸心跳检测
	KITCHEN_MONITORING	  = 18,	 // 明厨亮灶检测报警
	BEHAVIOR_DETECTION	  = 19,	 // 行为检测
	NON_MOTORIZED_VEHICLE = 20	 // 非机动车检测
};

/**
 * @brief 车辆类型枚举
 */
enum class VehicleType : int {
	UNKNOWN			  = 0,	 // 未知
	SEDAN			  = 1,	 // 轿车
	TRUCK			  = 2,	 // 货车
	MINIVAN			  = 3,	 // 面包车
	BUS				  = 4,	 // 客车
	LIGHT_TRUCK		  = 5,	 // 小货车
	SUV				  = 6,	 // SUV
	MIDDLE_BUS		  = 7,	 // 中型客车
	MOTORCYCLE		  = 8,	 // 摩托车
	PEDESTRIAN		  = 9,	 // 行人
	SCHOOL_BUS		  = 10,	 // 校车
	DUMP_TRUCK		  = 11,	 // 泥头车-渣土车
	HIGH_RISK_VEHICLE = 12,	 // 高危车
	RIDER			  = 13,	 // 骑行人
	MINI_SEDAN		  = 15,	 // 微型轿车
	SMALL_SEDAN		  = 16,	 // 小型轿车
	COMPACT_SEDAN	  = 17,	 // 紧凑型轿车
	HATCHBACK		  = 18,	 // 两厢轿车
	TRUNK_SEDAN		  = 19,	 // 三厢轿车
	LIGHT_BUS		  = 20,	 // 轻客
	SMALL_SUV		  = 21,	 // 小型SUV
	COMPACT_SUV		  = 22,	 // 紧凑型SUV
	MIDDLE_SUV		  = 23,	 // 中型SUV
	MEDIUM_LARGE_SUV  = 24,	 // 中大型SUV
	LARGE_SUV		  = 25,	 // 大型SUV
	MICRO_MINIVAN	  = 26,	 // 微型面包车
	MPV				  = 27,	 // MPV
	COUPE			  = 28,	 // 轿跑
	MICRO_PICKUP	  = 29,	 // 微卡
	PICKUP			  = 30,	 // 皮卡
	MEDIUM_TRUCK	  = 31,	 // 中卡
	LIGHT_DUTY_TRUCK  = 32,	 // 轻卡
	HEAVY_TRUCK		  = 33,	 // 重卡
	TAXI			  = 34,	 // 出租车
	TANKER			  = 35,	 // 油罐车
	CRANE			  = 36	 // 吊车
};

#endif	//__DEV_PROTO_DEF_HPP__
