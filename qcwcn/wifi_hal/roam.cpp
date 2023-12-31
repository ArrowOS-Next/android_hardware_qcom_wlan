/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Changes from Qualcomm Innovation Center are provided under the following license:

 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include "common.h"
#include "roamcommand.h"

#define WLAN_ROAM_MAX_NUM_WHITE_LIST 8
#define WLAN_ROAM_MAX_NUM_BLACK_LIST 16

RoamCommand::RoamCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
}

RoamCommand::~RoamCommand()
{
}

/* This function implements creation of Vendor command */
wifi_error RoamCommand::create() {
    wifi_error ret = mMsg.create(NL80211_CMD_VENDOR, 0, 0);
    if (ret != WIFI_SUCCESS)
        return ret;

    /* Insert the oui in the msg */
    ret = mMsg.put_u32(NL80211_ATTR_VENDOR_ID, mVendor_id);
    if (ret != WIFI_SUCCESS)
        return ret;
    /* Insert the subcmd in the msg */
    ret = mMsg.put_u32(NL80211_ATTR_VENDOR_SUBCMD, mSubcmd);
    if (ret != WIFI_SUCCESS)
        return ret;

     ALOGV("%s: mVendor_id = %d, Subcmd = %d.",
        __FUNCTION__, mVendor_id, mSubcmd);
    return ret;
}

wifi_error RoamCommand::requestResponse()
{
    return WifiCommand::requestResponse(mMsg);
}

wifi_error wifi_set_bssid_blacklist(wifi_request_id id,
                                    wifi_interface_handle iface,
                                    wifi_bssid_params params)
{
    wifi_error ret;
    int i;
    RoamCommand *roamCommand;
    struct nlattr *nlData, *nlBssids;
    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    if (!(info->supported_feature_set & WIFI_FEATURE_CONTROL_ROAMING)) {
        ALOGE("%s: Roaming is not supported by driver",
            __FUNCTION__);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    for (i = 0; i < params.num_bssid; i++) {
        ALOGV("BSSID: %d : " MACSTR, i, MAC2STR(params.bssids[i]));
    }

    roamCommand =
         new RoamCommand(wifiHandle,
                          id,
                          OUI_QCA,
                          QCA_NL80211_VENDOR_SUBCMD_ROAM);
    if (roamCommand == NULL) {
        ALOGE("%s: Error roamCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    /* Create the NL message. */
    ret = roamCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Set the interface Id of the message. */
    ret = roamCommand->set_iface_id(ifaceInfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Add the vendor specific attributes for the NL command. */
    nlData = roamCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nlData){
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = roamCommand->put_u32(QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD,
                          QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_BLACKLIST_BSSID);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = roamCommand->put_u32( QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID, id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = roamCommand->put_u32(
                  QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID,
                  params.num_bssid);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    nlBssids = roamCommand->attr_start(
            QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS);
    for (i = 0; i < params.num_bssid; i++) {
        struct nlattr *nl_ssid = roamCommand->attr_start(i);

        ret = roamCommand->put_addr(
                      QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID,
                      (u8 *)params.bssids[i]);
        if (ret != WIFI_SUCCESS)
            goto cleanup;

        roamCommand->attr_end(nl_ssid);
    }
    roamCommand->attr_end(nlBssids);

    roamCommand->attr_end(nlData);

    ret = roamCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("wifi_set_bssid_blacklist(): requestResponse Error:%d", ret);

cleanup:
    delete roamCommand;
    return ret;

}

wifi_error wifi_set_ssid_white_list(wifi_request_id id, wifi_interface_handle iface,
                                    int num_networks, ssid_t *ssid_list)
{
    wifi_error ret;
    int i;
    RoamCommand *roamCommand;
    struct nlattr *nlData, *nlSsids;
    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifiHandle = getWifiHandle(iface);
    char ssid[MAX_SSID_LENGTH + 1];

    ALOGV("%s: Number of SSIDs : %d", __FUNCTION__, num_networks);

    roamCommand = new RoamCommand(
                                wifiHandle,
                                id,
                                OUI_QCA,
                                QCA_NL80211_VENDOR_SUBCMD_ROAM);
    if (roamCommand == NULL) {
        ALOGE("%s: Failed to create object of RoamCommand class", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    /* Create the NL message. */
    ret = roamCommand->create();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to create NL message,  Error: %d", __FUNCTION__, ret);
        goto cleanup;
    }

    /* Set the interface Id of the message. */
    ret = roamCommand->set_iface_id(ifaceInfo->name);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to set interface Id of message, Error: %d", __FUNCTION__, ret);
        goto cleanup;
    }

    /* Add the vendor specific attributes for the NL command. */
    nlData = roamCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nlData) {
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = roamCommand->put_u32(QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD,
                              QCA_WLAN_VENDOR_ROAMING_SUBCMD_SSID_WHITE_LIST);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    ret = roamCommand->put_u32(QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID, id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    ret = roamCommand->put_u32(QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID_NUM_NETWORKS,
                               num_networks);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    nlSsids = roamCommand->attr_start(QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID_LIST);
    for (i = 0; i < num_networks; i++) {
        struct nlattr *nl_ssid = roamCommand->attr_start(i);

        memcpy(ssid, ssid_list[i].ssid_str, ssid_list[i].length);
        ssid[ssid_list[i].length] = '\0';
        ALOGV("ssid[%d] : %s", i, ssid);

        ret = roamCommand->put_bytes(QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID,
                                     ssid, (ssid_list[i].length + 1));
        if (ret != WIFI_SUCCESS) {
            ALOGE("%s: Failed to add ssid atribute, Error: %d", __FUNCTION__, ret);
            goto cleanup;
        }

        roamCommand->attr_end(nl_ssid);
    }
    roamCommand->attr_end(nlSsids);

    roamCommand->attr_end(nlData);

    ret = roamCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: Failed to send request, Error:%d", __FUNCTION__, ret);

cleanup:
    delete roamCommand;
    return ret;
}

wifi_error wifi_get_roaming_capabilities(wifi_interface_handle iface,
                                         wifi_roaming_capabilities *caps)
{
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    if (!caps) {
        ALOGE("%s: Invalid Buffer provided. Exit", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    if (!info) {
        ALOGE("%s: hal_info is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    // Per WiFi HAL design, roaming feature should have nothing to do with Gscan
    // But for current driver impl, roaming_capa is provided as part of
    // GSCAN_GET_CAPABILITY query, so if Gscan is not supported, roaming_capa
    // is not set (uses initial value 0).
    // To de-couple roaming with Gscan, set default values for roaming_capa
    // if Gscan is not supported.
    // TODO: removes below if driver has new API to get roaming_capa.
    if (!(info->supported_feature_set & WIFI_FEATURE_GSCAN)) {
        info->capa.roaming_capa.max_whitelist_size = WLAN_ROAM_MAX_NUM_WHITE_LIST;
        info->capa.roaming_capa.max_blacklist_size = WLAN_ROAM_MAX_NUM_BLACK_LIST;
    }
    memcpy(caps, &info->capa.roaming_capa, sizeof(wifi_roaming_capabilities));

    return WIFI_SUCCESS;
}

wifi_error wifi_configure_roaming(wifi_interface_handle iface, wifi_roaming_config *roaming_config)
{
    wifi_error ret;
    int requestId;
    wifi_bssid_params bssid_params;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    if (!roaming_config) {
        ALOGE("%s: Invalid Buffer provided. Exit", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    /* No request id from caller, so generate one and pass it on to the driver.
     * Generate it randomly.
     */
    requestId = get_requestid();

    /* Set bssid blacklist */
    if (roaming_config->num_blacklist_bssid > info->capa.roaming_capa.max_blacklist_size) {
        ALOGE("%s: Number of blacklist bssids(%d) provided is more than maximum blacklist bssids(%d)"
              " supported", __FUNCTION__, roaming_config->num_blacklist_bssid,
              info->capa.roaming_capa.max_blacklist_size);
        return WIFI_ERROR_NOT_SUPPORTED;
    }
    bssid_params.num_bssid = roaming_config->num_blacklist_bssid;

    memcpy(bssid_params.bssids, roaming_config->blacklist_bssid,
           (bssid_params.num_bssid * sizeof(mac_addr)));

    ret = wifi_set_bssid_blacklist(requestId, iface, bssid_params);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to configure blacklist bssids", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    /* Set ssid whitelist */
    if (roaming_config->num_whitelist_ssid > info->capa.roaming_capa.max_whitelist_size) {
        ALOGE("%s: Number of whitelist ssid(%d) provided is more than maximum whitelist ssids(%d) "
              "supported", __FUNCTION__, roaming_config->num_whitelist_ssid,
              info->capa.roaming_capa.max_whitelist_size);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    // Framework is always sending SSID length as 32 though null terminated lengths
    // are lesser. Thus update correct lengths before sending to driver.
    for (int i = 0; i < roaming_config->num_whitelist_ssid; i++) {
        int j;

        for (j = 0; j < roaming_config->whitelist_ssid[i].length; j++) {
            if (roaming_config->whitelist_ssid[i].ssid_str[j] == '\0')
                break;
        }

        if (roaming_config->whitelist_ssid[i].length == j)
            continue;

        ALOGI("%s: ssid_str %s reported length = %d , null terminated length = %d", __FUNCTION__,
              roaming_config->whitelist_ssid[i].ssid_str,
              roaming_config->whitelist_ssid[i].length, j);
        roaming_config->whitelist_ssid[i].length = j;
    }

    ret = wifi_set_ssid_white_list(requestId, iface, roaming_config->num_whitelist_ssid,
                                   roaming_config->whitelist_ssid);
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: Failed to configure whitelist ssids", __FUNCTION__);

    return ret;
}

/* Enable/disable firmware roaming */
wifi_error wifi_enable_firmware_roaming(wifi_interface_handle iface, fw_roaming_state_t state)
{
    int requestId;
    wifi_error ret;
    RoamCommand *roamCommand;
    struct nlattr *nlData;
    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifiHandle = getWifiHandle(iface);
    qca_roaming_policy policy;

    ALOGV("%s: set firmware roam state : %d", __FUNCTION__, state);

    if (state == ROAMING_ENABLE) {
        policy = QCA_ROAMING_ALLOWED_WITHIN_ESS;
    } else if(state == ROAMING_DISABLE) {
        policy = QCA_ROAMING_NOT_ALLOWED;
    } else {
        ALOGE("%s: Invalid state provided: %d. Exit \n", __FUNCTION__, state);
        return WIFI_ERROR_INVALID_ARGS;
    }

    /* No request id from caller, so generate one and pass it on to the driver.
     * Generate it randomly.
     */
    requestId = get_requestid();

    roamCommand =
         new RoamCommand(wifiHandle,
                          requestId,
                          OUI_QCA,
                          QCA_NL80211_VENDOR_SUBCMD_ROAMING);
    if (roamCommand == NULL) {
        ALOGE("%s: Failed to create object of RoamCommand class", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    /* Create the NL message. */
    ret = roamCommand->create();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to create NL message,  Error: %d", __FUNCTION__, ret);
        goto cleanup;
    }

    /* Set the interface Id of the message. */
    ret = roamCommand->set_iface_id(ifaceInfo->name);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to set interface Id of message, Error: %d", __FUNCTION__, ret);
        goto cleanup;
    }

    /* Add the vendor specific attributes for the NL command. */
    nlData = roamCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nlData) {
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = roamCommand->put_u32(QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY, policy);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to add roaming policy atribute, Error: %d", __FUNCTION__, ret);
        goto cleanup;
    }

    roamCommand->attr_end(nlData);

    ret = roamCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: Failed to send request, Error:%d", __FUNCTION__, ret);

cleanup:
    delete roamCommand;
    return ret;
}
