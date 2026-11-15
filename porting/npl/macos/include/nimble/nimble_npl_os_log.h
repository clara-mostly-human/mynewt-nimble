/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _NIMBLE_NPL_OS_LOG_H_
#define _NIMBLE_NPL_OS_LOG_H_

#include <stdarg.h>
#include <stdio.h>

/* Messages below BLE_HS_LOG_LVL (0 debug, 1 info, 2 warn, 3 error, 4 critical) are dropped, so a
 * daemon is not flooded with per-packet debug dumps. */
#define BLE_NPL_LOG_LVL_DEBUG    0
#define BLE_NPL_LOG_LVL_INFO     1
#define BLE_NPL_LOG_LVL_WARN     2
#define BLE_NPL_LOG_LVL_ERROR    3
#define BLE_NPL_LOG_LVL_CRITICAL 4
#ifdef MYNEWT_VAL_BLE_HS_LOG_LVL
#define BLE_NPL_LOG_MIN_LVL MYNEWT_VAL_BLE_HS_LOG_LVL
#else
#define BLE_NPL_LOG_MIN_LVL 0
#endif

#define BLE_NPL_LOG_IMPL(lvl)                                                 \
    static inline void _BLE_NPL_LOG_CAT(                                      \
        BLE_NPL_LOG_MODULE, _BLE_NPL_LOG_CAT(_, lvl))(const char *fmt, ...)   \
    {                                                                         \
        va_list args;                                                         \
        if (BLE_NPL_LOG_LVL_##lvl < BLE_NPL_LOG_MIN_LVL) {                    \
            return;                                                           \
        }                                                                     \
        va_start(args, fmt);                                                  \
        vprintf(fmt, args);                                                   \
        va_end(args);                                                         \
    }

#endif /* _NIMBLE_NPL_OS_LOG_H_ */
