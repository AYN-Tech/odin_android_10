/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VENDOR_NXP_NQNFC_V1_0_UTILS_H
#define VENDOR_NXP_NQNFC_V1_0_UTILS_H

#define MAX_IOCTL_TRANSCEIVE_CMD_LEN  256
#define MAX_IOCTL_TRANSCEIVE_RESP_LEN 256
#define MAX_ATR_INFO_LEN              128

/*
 * Data structures provided below are used of Hal Ioctl calls
 */

/*
 * nfc_nci_ExtnCmd_t shall contain data for commands used for transceive command in ioctl
 */
typedef struct
{
    uint16_t    cmd_len;
    uint8_t     p_cmd[MAX_IOCTL_TRANSCEIVE_CMD_LEN];
} nfc_nci_ExtnCmd_t;

/*
 * nfc_nci_ExtnRsp_t shall contain response for command sent in transceive command
 */
typedef struct
{
    uint16_t    rsp_len;
    uint8_t     p_rsp[MAX_IOCTL_TRANSCEIVE_RESP_LEN];
} nfc_nci_ExtnRsp_t;

/*
 * InputData_t :ioctl has multiple subcommands
 * Each command has corresponding input data which needs to be populated in this
 */
typedef union {
    uint16_t            bootMode;
    uint8_t             halType;
    nfc_nci_ExtnCmd_t   nciCmd;
    uint32_t            timeoutMilliSec;
}InputData_t;

/*
 * nfc_nci_ExtnInputData_t :Apart from InputData_t, there are context data
 * which is required during callback from stub to proxy.
 * To avoid additional copy of data while propagating from libnfc to Adaptation
 * and Nfcstub to ncihal, common structure is used. As a sideeffect, context data
 * is exposed to libnfc (Not encapsulated).
 */
typedef struct {
    void*        context;
    InputData_t  data;
}nfc_nci_ExtnInputData_t;

/*
 * outputData_t :ioctl has multiple commands/responses
 * This contains the output types for each ioctl.
 */
typedef union{
    uint32_t            status;
    nfc_nci_ExtnRsp_t   nciRsp;
    uint8_t             nxpNciAtrInfo[MAX_ATR_INFO_LEN];
    uint32_t            p61CurrentState;
    uint16_t            fwUpdateInf;
    uint16_t            fwDwnldStatus;
    uint16_t            fwMwVerStatus;
}outputData_t;

/*
 * nfc_nci_ExtnOutputData_t :Apart from outputData_t, there are other information
 * which is required during callback from stub to proxy.
 * For ex (context, result of the operation , type of ioctl which was completed).
 * To avoid additional copy of data while propagating from libnfc to Adaptation
 * and Nfcstub to ncihal, common structure is used. As a sideeffect, these data
 * is exposed(Not encapsulated).
 */
typedef struct {
    uint64_t     ioctlType;
    uint32_t     result;
    void*        context;
    outputData_t data;
}nfc_nci_ExtnOutputData_t;

/*
 * nfc_nci_IoctlInOutData_t :data structure for input & output
 * to be sent for ioctl command. input is populated by client/proxy side
 * output is provided from server/stub to client/proxy
 */
typedef struct {
    nfc_nci_ExtnInputData_t  inp;
    nfc_nci_ExtnOutputData_t out;
}nfc_nci_IoctlInOutData_t;

#endif //VENDOR_NXP_NQNFC_V1_0_UTILS_H
