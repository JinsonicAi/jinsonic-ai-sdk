/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __AXCL_RT_DEVICE_H__
#define __AXCL_RT_DEVICE_H__

#include "axcl_rt_type.h"

// clang-format off
#ifdef __cplusplus
extern "C" {
#endif


#define AXCL_DEF_DEVICE_ERR(e)              AXCL_DEF_RUNTIME_ERR(AXCL_RUNTIME_DEVICE, (e))

#define AXCL_ERR_DEVICE_NULL_POINTER        AXCL_DEF_DEVICE_ERR(AXCL_ERR_NULL_POINTER)
#define AXCL_ERR_DEVICE_OPEN                AXCL_DEF_DEVICE_ERR(AXCL_ERR_OPEN)
#define AXCL_ERR_DEVICE_UNSUPPORT           AXCL_DEF_DEVICE_ERR(AXCL_ERR_UNSUPPORT)

#define AXCL_ERR_DEVICE_QUERY_DEVICE        AXCL_DEF_DEVICE_ERR(0x80)
#define AXCL_ERR_DEVICE_NO_CONNECT          AXCL_DEF_DEVICE_ERR(0x81)
#define AXCL_ERR_DEVICE_PROBE               AXCL_DEF_DEVICE_ERR(0x82)
#define AXCL_ERR_DEVICE_NOT_ACTIVE          AXCL_DEF_DEVICE_ERR(0x83)
#define AXCL_ERR_DEVICE_INVALID_ID          AXCL_DEF_DEVICE_ERR(0x84)
#define AXCL_ERR_DEVICE_NO_ACTIVE_DEVICE    AXCL_DEF_DEVICE_ERR(0x85)
#define AXCL_ERR_DEVICE_PORT_ALLOCATE       AXCL_DEF_DEVICE_ERR(0x86)
#define AXCL_ERR_DEVICE_REBOOT              AXCL_DEF_DEVICE_ERR(0x87)
#define AXCL_ERR_DEVICE_PORT_DESTROY        AXCL_DEF_DEVICE_ERR(0x88)

axclError axclrtSetDevice(int32_t deviceId);
axclError axclrtResetDevice(int32_t deviceId);
axclError axclrtGetDevice(int32_t *deviceId);
axclError axclrtGetDeviceCount(uint32_t *count);
axclError axclrtGetDeviceList(axclrtDeviceList *deviceList);
axclError axclrtSynchronizeDevice();
axclError axclrtGetDeviceProperties(int32_t deviceId, axclrtDeviceProperties *properties);
axclError axclrtRebootDevice(int32_t deviceId);

/**
 * @brief Register device status callback
 *        The callback function will be called when the device status changes.
 *        Example:
 *          void device_status_callback(int32_t device, AXCL_DEVICE_STATUS_E status, void *userdata) {
 *              printf("device: %d, status: %d\n", device, status);
 *              if (status == AXCL_DEVICE_STATUS_OFFLINE) {
 *                  axclrtRebootDevice(device);
 *              }
 *          }
 *          axclrtRegisterDeviceStatusCallback(device_status_callback, NULL);
 * @param callback Device status callback function
 * @param userdata User data
 */
axclError axclrtRegisterDeviceStatusCallback(axclrtDeviceStatusCallback callback, void *userdata);

#ifdef __cplusplus
}
#endif
// clang-format on

#endif /* __AXCL_RT_DEVICE_H__ */