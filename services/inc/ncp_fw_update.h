/*
 * Copyright (c) 2021 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "hal_platform.h"
#include "system_tick_hal.h"
#include "system_defs.h"
#include "system_mode.h"
// #include "system_network.h"
#include "cellular_hal.h"
#include "platform_ncp.h"
#include "diagnostics.h"
#include "spark_wiring_diagnostics.h"
#include "ncp_fw_update_dynalib.h"
#include "static_assert.h"

#if HAL_PLATFORM_NCP
#include "at_parser.h"
#include "at_response.h"
#endif // HAL_PLATFORM_NCP

#if HAL_PLATFORM_NCP_FW_UPDATE

struct SaraNcpFwUpdateCallbacks {
    uint16_t size;
    uint16_t reserved;

    // System.updatesEnabled() / system_update.h
    int (*system_get_flag)(system_flag_t flag, uint8_t* value, void*);

    // system_cloud.h
    bool (*spark_cloud_flag_connected)(void);
    void (*spark_cloud_flag_connect)(void);
    void (*spark_cloud_flag_disconnect)(void);

    // system_cloud_internal.h
    bool (*publishEvent)(const char* event, const char* data, unsigned flags);
};
PARTICLE_STATIC_ASSERT(SaraNcpFwUpdateCallbacks_size, sizeof(SaraNcpFwUpdateCallbacks) == (sizeof(void*) * 6));

#ifdef UNIT_TEST
int setupHTTPSProperties_impl(void);
int sendCommandWithArgs(_CALLBACKPTR_MDM cb, void* param, system_tick_t timeout_ms, const char* format, va_list args);
#endif

namespace particle {

namespace services {

enum SaraNcpFwUpdateState {
    FW_UPDATE_STATE_IDLE                            = 0,
    FW_UPDATE_STATE_QUALIFY_FLAGS                   = 1,
    FW_UPDATE_STATE_SETUP_CLOUD_CONNECT             = 2,
    FW_UPDATE_STATE_SETUP_CLOUD_CONNECTING          = 3,
    FW_UPDATE_STATE_SETUP_CLOUD_CONNECTED           = 4,
    FW_UPDATE_STATE_DOWNLOAD_CLOUD_DISCONNECT       = 5,
    FW_UPDATE_STATE_DOWNLOAD_CELL_DISCONNECTING     = 6,
    FW_UPDATE_STATE_DOWNLOAD_CELL_CONNECTING        = 7,
    FW_UPDATE_STATE_DOWNLOAD_HTTPS_SETUP            = 8,
    FW_UPDATE_STATE_DOWNLOAD_READY                  = 9,
    FW_UPDATE_STATE_INSTALL_CELL_DISCONNECTING      = 10,
    FW_UPDATE_STATE_INSTALL_STARTING                = 11,
    FW_UPDATE_STATE_INSTALL_WAITING                 = 12,
    FW_UPDATE_STATE_FINISHED_POWER_OFF              = 13,
    FW_UPDATE_STATE_FINISHED_POWERING_OFF           = 14,
    FW_UPDATE_STATE_FINISHED_CLOUD_CONNECTING       = 15,
    FW_UPDATE_STATE_FINISHED_CLOUD_CONNECTED        = 16,
    FW_UPDATE_STATE_FINISHED_IDLE                   = 17,
};
static_assert(FW_UPDATE_STATE_IDLE == 0 && FW_UPDATE_STATE_FINISHED_IDLE == 17, "SaraNcpFwUpdateState size changed!");

enum SaraNcpFwUpdateStatus {
    FW_UPDATE_STATUS_IDLE                                           = 0,
    FW_UPDATE_STATUS_DOWNLOADING                                    = 1,
    FW_UPDATE_STATUS_UPDATING                                       = 2,
    FW_UPDATE_STATUS_SUCCESS                                        = 3,
    FW_UPDATE_STATUS_NONE                                           = -1, // for diagnostics
    FW_UPDATE_STATUS_FAILED                                         = -2,
    FW_UPDATE_STATUS_FAILED_QUALIFY_FLAGS                           = -3,
    FW_UPDATE_STATUS_FAILED_CLOUD_CONNECT_ON_ENTRY_TIMEOUT          = -4,
    FW_UPDATE_STATUS_FAILED_PUBLISH_START                           = -5,
    FW_UPDATE_STATUS_FAILED_SETUP_CELLULAR_DISCONNECT_TIMEOUT       = -6,
    FW_UPDATE_STATUS_FAILED_SETUP_CELLULAR_STILL_CONNECTED          = -7,
    FW_UPDATE_STATUS_FAILED_CELLULAR_CONNECT_TIMEOUT                = -8,
    FW_UPDATE_STATUS_FAILED_HTTPS_SETUP                             = -9,
    FW_UPDATE_STATUS_FAILED_DOWNLOAD_RETRY_MAX                      = -10,
    FW_UPDATE_STATUS_FAILED_INSTALL_CELLULAR_DISCONNECT_TIMEOUT     = -11,
    FW_UPDATE_STATUS_FAILED_START_INSTALL_TIMEOUT                   = -12,
    FW_UPDATE_STATUS_FAILED_INSTALL_AT_ERROR                        = -13,
    FW_UPDATE_STATUS_FAILED_SAME_VERSION                            = -14,
    FW_UPDATE_STATUS_FAILED_INSTALL_TIMEOUT                         = -15,
    FW_UPDATE_STATUS_FAILED_POWER_OFF_TIMEOUT                       = -16,
    FW_UPDATE_STATUS_FAILED_CLOUD_CONNECT_ON_EXIT_TIMEOUT           = -17,
    FW_UPDATE_STATUS_FAILED_PUBLISH_RESULT                          = -18,
};
static_assert(FW_UPDATE_STATUS_IDLE == 0 && FW_UPDATE_STATUS_FAILED_PUBLISH_RESULT == -18, "SaraNcpFwUpdateStatus size changed!");

const system_tick_t NCP_FW_MODEM_INSTALL_ATOK_INTERVAL = 10000;
const system_tick_t NCP_FW_MODEM_INSTALL_START_TIMEOUT = 5 * 60000;
const system_tick_t NCP_FW_MODEM_INSTALL_FINISH_TIMEOUT = 30 * 60000;
const system_tick_t NCP_FW_MODEM_CLOUD_CONNECT_TIMEOUT = 5 * 60000;
const system_tick_t NCP_FW_MODEM_CLOUD_DISCONNECT_TIMEOUT = 1 * 60000;
const system_tick_t NCP_FW_MODEM_CELLULAR_CONNECT_TIMEOUT = 10 * 60000;
const system_tick_t NCP_FW_MODEM_DOWNLOAD_TIMEOUT = 5 * 60000;
const system_tick_t NCP_FW_MODEM_POWER_OFF_TIMEOUT = 1 * 60000;
const int NCP_FW_UBLOX_DEFAULT_CID = 1;
const int NCP_FW_UUFWINSTALL_COMPLETE = 128;

/**
 * struct SaraNcpFwUpdateConfig {
 *     uint16_t size;
 *     uint32_t start_version;
 *     uint32_t end_version;
 *     char filename[256];
 *     char md5sum[32];
 * };
 */
const SaraNcpFwUpdateConfig SARA_NCP_FW_UPDATE_CONFIG[] = {
    // { sizeof(SaraNcpFwUpdateConfig), 31400010, 31400011, "SARA-R510S-01B-00-ES-0314A0001_SARA-R510S-01B-00-XX-0314ENG0099A0001.upd", "09c1a98d03c761bcbea50355f9b2a50f" },
    // { sizeof(SaraNcpFwUpdateConfig), 31400011, 31400010, "SARA-R510S-01B-00-XX-0314ENG0099A0001_SARA-R510S-01B-00-ES-0314A0001.upd", "136caf2883457093c9e41fda3c6a44e3" },
    // { sizeof(SaraNcpFwUpdateConfig), 20600010, 990100010, "SARA-R510S-00B-01_FW02.06_A00.01_IP_SARA-R510S-00B-01_FW99.01_A00.01.upd", "ccfdc48c0a45198d6e168b30d0740959" },
    // { sizeof(SaraNcpFwUpdateConfig), 990100010, 20600010, "SARA-R510S-00B-01_FW99.01_A00.01_SARA-R510S-00B-01_FW02.06_A00.01_IP.upd", "5fd6c0d3d731c097605895b86f28c2cf" },
};
const size_t SARA_NCP_FW_UPDATE_CONFIG_SIZE = sizeof(SARA_NCP_FW_UPDATE_CONFIG) / sizeof(SARA_NCP_FW_UPDATE_CONFIG[0]);

struct __attribute__((packed)) SaraNcpFwUpdateData {
    uint16_t size;                          // sizeof(SaraNcpFwUpdateData)
    SaraNcpFwUpdateState state;             // FW_UPDATE_STATE_IDLE;
    SaraNcpFwUpdateStatus status;           // FW_UPDATE_STATUS_IDLE;
    uint32_t firmwareVersion;               // 0;
    uint32_t startingFirmwareVersion;       // 0;
    uint32_t updateVersion;                 // 0;
    uint8_t updateAvailable;                // SYSTEM_NCP_FW_UPDATE_STATUS_UNKNOWN;
    uint8_t isUserConfig;                   // 0;
    SaraNcpFwUpdateConfig userConfigData;   // 0;
};

struct HTTPSresponse {
    volatile bool valid;
    int command;
    int result;
    int status_code;
    char md5_sum[40];
};

struct HTTPSerror {
    int err_class;
    int err_code;
};

class SaraNcpFwUpdate {
public:
    /**
     * Get the singleton instance of this class.
     */
    static SaraNcpFwUpdate* instance();

    void init(SaraNcpFwUpdateCallbacks callbacks);
    int process();
    int getStatusDiagnostics();
    int setConfig(const SaraNcpFwUpdateConfig* userConfigData = nullptr);
    int checkUpdate(uint32_t version = 0);
    int enableUpdates();
    int updateStatus();

protected:
    SaraNcpFwUpdate();
    ~SaraNcpFwUpdate() = default;

    SaraNcpFwUpdateState saraNcpFwUpdateState_;
    SaraNcpFwUpdateState saraNcpFwUpdateLastState_;
    SaraNcpFwUpdateStatus saraNcpFwUpdateStatus_;
    SaraNcpFwUpdateStatus saraNcpFwUpdateStatusDiagnostics_;
    uint32_t startingFirmwareVersion_;
    uint32_t firmwareVersion_;
    uint32_t updateVersion_;
    int updateAvailable_;
    int downloadRetries_;
    int finishedCloudConnectingRetries_;
    volatile int cgevDeactProfile_;
    system_tick_t startTimer_;
    system_tick_t atOkCheckTimer_;
    system_tick_t cooldownTimer_;
    system_tick_t cooldownTimeout_;
    bool isUserConfig_;
    bool initialized_;
    SaraNcpFwUpdateData saraNcpFwUpdateData_ = {};
    /**
     * Functional callbacks that provide key system services to this NCP FW Update class.
     */
    SaraNcpFwUpdateCallbacks saraNcpFwUpdateCallbacks_ = {};
    HTTPSresponse httpsResp_ = {};

    static int cbUHTTPER(int type, const char* buf, int len, HTTPSerror* data);
    static int cbULSTFILE(int type, const char* buf, int len, int* data);
    static int httpRespCallback(AtResponseReader* reader, const char* prefix, void* data);
    static int cgevCallback(AtResponseReader* reader, const char* prefix, void* data);
    uint32_t getNcpFirmwareVersion();
    int setupHTTPSProperties();
    void cooldown(system_tick_t timer);
    void updateCooldown();
    bool inCooldown();
    int validateSaraNcpFwUpdateData();
    int firmwareUpdateForVersion(uint32_t version);
    int getConfigData(SaraNcpFwUpdateConfig& configData);
    int saveSaraNcpFwUpdateData();
    int recallSaraNcpFwUpdateData();
    int deleteSaraNcpFwUpdateData();
    void logSaraNcpFwUpdateData(SaraNcpFwUpdateData& data);
};

#ifndef UNIT_TEST
class NcpFwUpdateDiagnostics: public AbstractUnsignedIntegerDiagnosticData {
public:
    NcpFwUpdateDiagnostics() :
            AbstractUnsignedIntegerDiagnosticData(DIAG_ID_NETWORK_NCP_FW_UPDATE_STATUS, DIAG_NAME_NETWORK_NCP_FW_UPDATE_STATUS) {
    }

    virtual int get(IntType& val) override {
        val = particle::services::SaraNcpFwUpdate::instance()->getStatusDiagnostics();
        return 0; // OK
    }
};
#endif // UNIT_TEST

} // namespace services

} // namespace particle

#endif // #if HAL_PLATFORM_NCP_FW_UPDATE
