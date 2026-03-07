#ifndef __SP_DEFINE_H__
#define __SP_DEFINE_H__


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef BOOL
#define BOOL int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define CHAR_LEN 128

typedef struct{
	int x;
	int y;
	int w;
	int h;
}SPRect_t;

typedef struct{
	int x;
	int y;
}SPVector;

typedef struct{
	int w;
	int h;
}SPRes_t;

//可以打印文件名和行号,lck20100417
#ifndef Printf
#define Printf(fmt...)  \
do{\
	if(1){	\
		printf("[%s]:%d ", __FILE__, __LINE__);\
		printf(fmt);} \
} while(0)
#endif

#ifndef CPrintf
#define CPrintf(fmt...)  \
do{\
	if(1){	\
		printf("\33[31m");\
		printf(fmt);} \
		printf("\33[0m ");\
} while(0)
#endif


#define __FUNC_DBG__() \
	do{ \
		if (2 > 1) \
			printf("\33[31mNOT_FINISHED %s:%s =>%4d: called\33[0m\n", __FILE__, __func__, __LINE__); \
	} while(0)

#define SP_MAX_PLAN_DAY        7
#define SP_MAX_PLAN_TIME_CNT   4
#define SP_MAX_ALARMIN         2
#define SP_MAX_ALARMOUT        2
#define	SP_MAX_LIST_COUNT			20	//最大列表个数
#define SP_MAX_ALARM_SRC_CNT		5  //关联最大报警源个数
#define SP_ALARM_TYPE_LEN	  32
typedef struct
{
	BOOL         bEnable;
	unsigned int uiStartTime;		//一天中流过的秒数
	unsigned int uiEndTime;			//一天中流过的秒数
}SPTPlanTime;
		
typedef struct
{
	int day;				//一周中第几天, 0:未启用1:周日2:周一3:周二4:周三5:周四6:周五7:周六
	SPTPlanTime stPlanTime[SP_MAX_PLAN_TIME_CNT];
}SPTPlanDay;

typedef struct
{
	BOOL		bAllTime;							//全时段防护使能	
	BOOL		bSOS;								//是否启用紧急报警防护
	SPTPlanDay	stPlanDay[SP_MAX_PLAN_DAY];		//防护时段
}SPTProtecPlan;

typedef struct
{
	BOOL        bFlick;				//是否联动闪烁
	int         nStrength;			//闪烁强度
}SPTAlarmLightCfg;

typedef struct
{
	int 		iAlarmOutId;		//报警输出通道id
	BOOL		bLinkEnable;		//报警输出联动使能
	BOOL		bLinkClose; 		//报警联动闭合，TRUE: 联动闭合 FALSE:联动断开
}SPTAlarmOutLink;

typedef struct
{
	BOOL        bEnable;			//是否开启
	char		server[128];		//报警推送服务器
}SPHttp_post_t;

typedef struct
{
	BOOL bEnableRecord; 	//是否开启报警录像
	BOOL bEnableSnap;		//是否开启报警抓拍
	BOOL bBuzzing;			//是否蜂鸣器报警
	BOOL bSendtoClient; 	//是否发送至分控
	BOOL bSendEmail;		//是否发送邮件
	BOOL bSendtoVMS;		//是否发送至VMS服务器
	BOOL bAlarmSound;		//是否播放报警声音
	BOOL bSendFTP;			//是否发送至FTP
	int  nPreset;           //联动预置位
	SPTAlarmLightCfg stWhiteLightCfg;	//白光灯联动配置
	SPTAlarmLightCfg stLedLightCfg;	//红蓝灯联动配置
	SPTAlarmOutLink stAlarmOutLink[SP_MAX_ALARMOUT]; //可用于联动的报警输出
	SPHttp_post_t stHttpPost;		//报警输出到http
}SPTAlarmLinkCfg; 			//报警联动配置

typedef enum
{
	SP_SNAP_AFTER_LEAVEING = 0,
	SP_SNAP_IN_TRACKING,
	SP_SNAP_MIXED,
	SP_SNAP_INTERVAL,
}sp_snapmode_e;

typedef struct
{
	BOOL        enable;				//报警声音使能
	char        sound_name[CHAR_LEN];	//报警声音名字
}sp_alarm_sound_cfg_t;

typedef struct
{
	BOOL        enable;			//是否开启
	int         interval;			//抓拍间隔，单位秒
	int 		count;				//抓拍张数
}sp_alarm_snap_cfg_t;

typedef struct
{
	char alarm_type[SP_ALARM_TYPE_LEN];
}sp_alarm_source_t;

typedef struct
{
	BOOL		all_time;					//全时段防护使能
	int everyday_max_timecnt;
	SPTPlanDay	plan_day[SP_MAX_PLAN_DAY];	//布防时间
}sp_alarm_defence_plan_cfg_t;		//布防计划配置(新方案)

typedef struct
{
	int			plan_id;				//布防计划ID
	char		plan_name[CHAR_LEN];	//布防计划名称
	char		plan_add_time[CHAR_LEN];
	sp_alarm_defence_plan_cfg_t defence_cfg;
	sp_alarm_source_t alarm_source[SP_MAX_ALARM_SRC_CNT];
}sp_alarm_defence_plan_info_t;		//布防计划配置(新方案)

typedef struct
{
	BOOL client_en;		  //是否发送到客户端软件
	BOOL email_en;		  //是否发送邮件
	sp_alarm_snap_cfg_t snap;
	int duration;
	SPTAlarmLightCfg white_light;	//白光灯联动配置
	SPTAlarmLightCfg rgb_light;		//红蓝灯联动配置
	sp_alarm_sound_cfg_t alarmsound;		//报警声音配置
	int preset_id;						//联动预置位
	int alarmout[SP_MAX_ALARMOUT]; //可用于联动的报警输出
} sp_alarm_link_cfg_t;					//报警联动配置（新方案）

typedef struct
{
	int link_id;			  //报警联动ID
	char link_name[CHAR_LEN]; //报警联动名称
	char link_add_time[CHAR_LEN];
	sp_alarm_link_cfg_t link_cfg;
	sp_alarm_source_t alarm_source[SP_MAX_ALARM_SRC_CNT];
} sp_alarm_link_info_t;					//报警联动配置（新方案）

#endif /* #ifndef __JV_COMMON_H__ */

