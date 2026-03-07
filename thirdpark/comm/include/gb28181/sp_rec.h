/*
 * sp_rec.h
 *
 *  Created on: 2013-11-1
 *      Author: lfx
 */

#ifndef SP_REC_H_
#define SP_REC_H_

#include "sp_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LEN 1024*1024


/* 帧类型定义*/
typedef enum
{
	RTP_PACK_TYPE_FRAME_P=0,
	RTP_PACK_TYPE_FRAME_I,
	RTP_PACK_TYPE_FRAME_B,
	RTP_PACK_TYPE_FRAME_AUDIO,
	RTP_PACK_TYPE_FRAME_JEPG,
	RTP_PACK_TYPE_FRAME_MOTION,
	RTP_PACK_TYPE_FRAME_NUM,
	RTP_PACK_TYPE_FRAME_NULL=0xff,/*结束，无数据*/
}RtpPackType_e;

/**
 *@brief 开始录像
 *
 *@param channelid 通道号
 */
int sp_rec_start(int channelid);
/**
 *@brief 暂停录像
 *
 *@param channelid 通道号
 */
 int sp_rec_pause(int channelid);

/**
 *@brief 结束录像
 *
 *@param channelid 通道号
 */
int sp_rec_stop(int channelid);

/**
 *@brief 检查是否正在录像
 */
int sp_rec_b_on(int channelid);

/**
 *@brief 获取录像通道
 */
int sp_rec_get_channelid();

/**
 * 帧结构
 */
typedef struct
{
	unsigned char *buf;	//帧数据
	int len;
	int framerate;				//帧率
	int width;					//帧高
	int height;					//帧宽
	RtpPackType_e type;			//帧类型
}FrameInfo_t;

typedef enum{
	SP_REC_TYPE_MOTION,	//移动检测录像
	SP_REC_TYPE_TIME,	//定时录像
	SP_REC_TYPE_ALARM,	//报警录像
	SP_REC_TYPE_MANUAL,	//手动录像
	SP_REC_TYPE_ALL,	//全部一直录像
	SP_REC_TYPE_STOP,	//停止录像
	SP_REC_TYPE_DISCONNECT,
}SPRecType_e;

/**
 * 录像文件结构
 */
typedef struct _REC_Search
{
	char fname[128];			//文件路径
	long start_time;	//录像文件开始时间，1970年以来的秒数
	long end_time;		//录像文件结束时间
	int secrecy;				//保密属性/0为不涉密/1为涉密
	SPRecType_e type;				//录像产生类型
	struct _REC_Search *next;
}REC_Search;

typedef void * REC_HANDLE;		//句柄

#define SP_MAX_REC_PLAN_DAY   7

typedef struct
{
	/**
	 * 定时录制的开始时间和结束时间，单位为秒，其取值在0 ~ 24*60*60 之间
	 * 当timing_stop > timing_start时，定时范围就是当天的timing_start到timing_stop
	 * 当timing_stop < timing_start时，定时范围为当天的timing_start到第二天的timing_stop
	 * 
	 */
	unsigned int timing_start;        	//一天中流过的秒数
	unsigned int timing_stop;          	//一天中流过的秒数
}SPTRecordTime;

typedef struct
{
	int day;                	//一周中第几天, 0:未启用1:周日2:周一3:周二4:周三5:周四6:周五7:周六
	SPTRecordTime stRecTime;
}SPTRecordPlan;

typedef struct
{
	BOOL				bEnable;			///< 是否启用手工录制
	BOOL				timing_enable;		///< 是否启用定时录制
	BOOL 				discon_enable; 		///< 是否启用无连接录像
	BOOL 				alarm_enable; 		///< 是否启用报警录像

	BOOL				disconnected;		///< 无连接时录制
	BOOL				detecting;			///< 移动检测录制
	BOOL				alarming;			///< 外部报警录制
	unsigned int 		alarm_pre_record;	///<预录制的时间，最大为10秒
	unsigned int 		alarm_duration;		///<报警发生后，录制的持续时间
	unsigned int		file_length;		///< 录像文件的单个的长度，以秒为单位
	BOOL                bRecAudio;          ///是否录制音频
	SPTRecordPlan     	stRecPlan[SP_MAX_REC_PLAN_DAY]; //最多可部署七天的录像时段
}SP_RecAttr;

typedef enum 
{
	SP_REC_STOP		= 0,	//录像已停止
	SP_REC_NORMAL	= 'N',	//正常录像 78
	SP_REC_DISCON  	= 'D',	//无连接时录像. IPC在无连接时录像
	SP_REC_TIME		= 'T',	//定时录像 84
	SP_REC_MOTION	= 'M',	//移动检测录像 77
	SP_REC_ALARM	= 'A',	//报警录像 65
} SP_RECTYPE;

typedef struct{
	SP_RECTYPE type;
}SP_RecStatus;

/**
 * @brief 创建录像播放句柄
 *
 * @param channelid 通道号
 * @param rec_start_time 要播放的录像开始时间,从1970年到开始播放的秒
 * @param rec_end_time 要播放的录像结束时间，从1970年到结束播放的秒
 * @param record_type 录像类型，“1”为手动录像，“2”为报警录像，“3”为定时录像
 * @return REC_HANDLE 返回录像播放句柄
 */
REC_HANDLE  sp_rec_create(int channelid, long start_time, long end_time, int record_type);

/**
 *@brief 销毁rec句柄
 */
int sp_rec_destroy(REC_HANDLE handle);

/**
 *@brief 恢复播放录像
 *
 *@param channelid 通道号
 */
int sp_rec_resume(int channelid);

/**
 * @brief 检索文件
 *
 * @param channelid 通道号
 * @param rec_start_time 要检索的录像开始时间,从1970年到开始播放的秒
 * @param rec_end_time 要检索的录像结束时间，从1970年到结束播放的秒
 *
 * @return 检索结果输出
 *
 * @note 搜索结果，需要调用#sp_rec_search_release释放资源
 *
 */
REC_Search *sp_rec_search(int channelid, long start_time, long end_time);

/**
 * @brief 删除检索信息
 *
 * @param list 搜索结果。#sp_rec_search 的结果
 *
 * @return 检索结果输出
 *
 */
int sp_rec_search_release(REC_Search *list);
/**
 * @brief 检索信息数量
 *
 * @param list 搜索结果。#sp_rec_search 的结果
 *
 * @return 检索结果数量输出
 *
 */
 int sp_rec_search_cnt(REC_Search *list);

/**
 * @brief 定位视频播放位置
 */
int sp_rec_seek(REC_HANDLE handle, int play_range_begin);

/**
 * @brief 读取录像文件
 *
 * @param handle 录像播放句柄
 * @param buf 返回帧结构
 *
 * @return 0
 */
int sp_rec_read(REC_HANDLE handle, FrameInfo_t *buf);

/**
 * @brief 设置录像模式
 *
 * @param channelid 通道号
 * @param rectype 录像模式
 *
 * @return 0 或者是错误号
 */
int sp_rec_set_mode(int channelid, SPRecType_e rectype);
/**
 * @brief 获得录像模式
 *
 * @param channelid 通道号
 *
 * @return 录像模式
 */
SPRecType_e sp_rec_get_mode(int channelid);

/**
 * @brief 设置录像文件长度
 *
 * @param channelid 通道号
 * @param 录像文件打包长度单位分钟
 *
 * @return 0 或者是错误号
 */
unsigned int sp_rec_set_length(int channelid, unsigned int len);
/**
 * @brief 获得录像文件打包时长
 *
 * @param channelid 通道号
 *
 * @return 录像文件打包时间长度
 */
unsigned int sp_rec_get_length(int channelid);


#ifdef TUTK_SUPPORT
/**
 * @brief 创建tutk远程回放播放器
 */
int sp_rec_tutk_create(int channelid, long start_time,int type);
#endif

int sp_rec_get_param(int channelid, SP_RecAttr *recattr);

int sp_rec_get_status(int channelid, SP_RecStatus *recStatus);

int sp_rec_set_param(int channelid, SP_RecAttr *recattr);

#ifdef __cplusplus
}
#endif

#endif /* SP_REC_H_ */
