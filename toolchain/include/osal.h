/**
 * @file osal.h
 * @brief Q80 OSAL API declarations.
 *
 * Based on Prolin API Programming Guide 2.5.3 and the community header by:
 * - Milan Babuljak (hiimmilan.dev), 2024
 * - Richard Gráčik (Morc), 2024
 * Only functions exported by this Q80's libosal.so are declared here.
 * An exported name does not verify its ABI or hardware availability.
 */

#ifndef Q80_OSAL_H
#define Q80_OSAL_H

#include <stdint.h>

/*--------------------------------------------
 * Error Codes
 *-------------------------------------------*/
#define RET_OK 0
#define ERR_INVALID_HANDLE -1000
#define ERR_TIME_OUT -1001
#define ERR_APP_NOT_EXIST -1002
#define ERR_INVALID_PARAM -1003
#define ERR_DEV_NOT_EXIST -1004
#define ERR_DEV_BUSY -1005
#define ERR_DEV_NOT_OPEN -1006
#define ERR_ACCESS_DENY -1007
#define ERR_FILE_NOT_FOUND -2201
#define ERR_NO_SPACE -2205
#define ERR_NEED_ADMIN -2207

/* This Q80's OsPrnOpen routes mode 0 to /dev/printer. */
#define PRN_REAL 0
#define ERR_PRN_BUSY -3701
#define ERR_PRN_PAPEROUT -3702
#define ERR_PRN_WRONG_PACKAGE -3703
#define ERR_PRN_OVERHEAT -3704
#define ERR_PRN_OUTOFMEMORY -3705
#define ERR_PRN_OVERVOLTAGE -3706

// RF
#define PCD_ERR_PAR_FLAG -2901            // Parity error.
#define PCD_ERR_CRC_FLAG -2902            // CRC error.
#define PCD_ERR_WTO_FLAG -2903            // Timeout or no card.
#define PCD_ERR_COLL_FLAG -2904           // Multiple cards collision.
#define PCD_ERR_ECD_FLAG -2905            // Frame format error.
#define PCD_ERR_EMD_FLAG -2906            // Interference.
#define PCD_ERR_COM_FLAG -2907            // Chip error, fail to communicate correctly.
#define PCD_ERR_AUT_FLAG -2908            // M1 authentication error.
#define PCD_ERR_TRANSMIT_FLAG -2909       // Transmission error.
#define PCD_ERR_PROTOCOL_FLAG -2910       // Protocol error.
#define PCD_ERR_PARAMFILE_FLAG -2911      // Configuration file does not exist.
#define PCD_ERR_USER_CANCEL -2912         // Transaction is cancelled.
#define PCD_ERR_CARRIER_OBTAIN_FLAG -2913 // No obtained carrier.
#define PCD_ERR_CONFIG_FLAG -2914         // Fail to configure register.
#define PCD_ERR_RXLEN_EXCEED_FLAG                                                                  \
  -2915                                // The returned data length exceeds the set receiving length.
#define PCD_ERR_NOT_ALLOWED_FLAG -2951 // Parameter error or invalid value.
#define PCD_CHIP_ABNORMAL -2952        // Chip is abnormal or does not exist.
#define PCD_CHIP_NOT_OPENED -2953      // Module is not open.
#define PCD_CHIP_CARDEXIST -2954       // Card is not removed.
#define PCD_ERR_NOT_IDLE_FLAG -2955    // Card is not in idle state.
#define PCD_ERR_NOT_POLLING_FLAG -2956 // No polling.
#define PCD_ERR_NOT_WAKEUP_FLAG -2957  // Card does not wakeup.
#define PCD_ERR_NOT_ACTIVE_FLAG -2958  // Card is not activated.
#define PCD_ERR_NOT_SUPPORT -2959      // Chip is unsupported.
#define ERR_BATTERY_VOLTAGE_TOO_LOW -1024 // Battery voltage is too low.

// Battery
#define POWER_USB 2
#define POWER_BATTERY 3

/*--------------------------------------------
 * File Types
 *-------------------------------------------*/
#define FILE_TYPE_APP 1
#define FILE_TYPE_APP_PARAM 2
#define FILE_TYPE_SYS_LIB 3
#define FILE_TYPE_PUB_KEY 4
#define FILE_TYPE_AUP 5

/*--------------------------------------------
 * Structures
 *-------------------------------------------*/
typedef struct {
  int Year;
  int Month;
  int Day;
  int Hour;
  int Minute;
  int Second;
  int DayOfWeek;
} ST_TIME;

typedef struct {
  unsigned long Start;
  unsigned long Stop;
  unsigned long TimeLeft;
} ST_TIMER;

typedef struct {
  char Id[64];
  char Name[64];
  char Bin[64];
  char Artwork[64];
  char Desc[64];
  char Vender[32];
  char Version[32];
} ST_APP_INFO;

typedef struct {
  unsigned char Left;
  unsigned char Top;
  unsigned char Width;
  unsigned char Height;
  unsigned char Dot[3072];
} FT_DOT;

typedef struct {
  char FileName[64];
  char FontName[64];
} FT_FONT;

typedef struct {
  unsigned char TrackData[256]; /* Decoded bit stream*/
  int DataLen;                  /* Track data length*/
  int Status;                   /*Track data status, 0 means succeeded, other value means failed*/
} ST_MSR_DATA;

typedef struct pcd_user_t {
  unsigned char wait_retry_limit_w;  /* S(WTX) responds to write permission of sending times*/
  unsigned int wait_retry_limit_val; /*S(WTX)responds to the maximum repetition times.*/
  unsigned char check_cancel_key_w;  /*respond to write permission of the cancel key*/
  unsigned char
      check_cancel_key_val; /* 0: No response to the cancel key; 1: Respond to the cancel key */
  int (*check_cancel_key_function)(
      void); /*Check whether the cancel key is pressed. If set check_cancel_key_w=1 and
                check_cancel_key_val=1, check_cancel_key_function () will be called during RF card
                transaction process. If check_cancel_key_function () returns 0, it means the cancel
                key is not pressed. If the function returns non-zero, it means the cancel key is
                pressed and will exit transaction by force,*/
  unsigned char os_picc_transfer_set_w; /*1 means “os_picc_transfer_set_val” value is valid, 0 means
                                           “os_picc_transfer_set_val” value is invalid*/
  unsigned char os_picc_transfer_set_val; /* Set OsPiccTransfer receive/send: Bit0=0: send disable
                                             CRC; Bit0=1: send enable CRC; Bit1=0: receive disable
                                             CRC; Bit1=1: receive enable CRC*/
  unsigned char anti_interference_flag;   /*Anti-interference setting when searching the card; 1-
                                             enable anti-interference, 0- disable an-interference.*/
  unsigned char protocol_layer_fwt_set_w; /*1 indicates that the value of protocol_layer_fwt_set_val
                                             is valid, 0 indicates that the value of
                                             protocol_layer_fwt_set_val is invalid */
  unsigned int
      protocol_layer_fwt_set_val; /* Set the frame wait time for the protocol layer (value of FWT)*/
  unsigned char
      os_picc_transfer_rxlen_set_w; /*1 indicates that the value of os_picc_transfer_rxlen_set_val
                                       is vaild, 0 indicates that the value
                                       ofos_picc_transfer_rxlen_set_val is invalid */
  unsigned int os_picc_transfer_rxlen_set_val; /*Set the maximum allowable data length in the
                                                  half-duplex block (OsPiccTransfer) transfer*/
  unsigned char
      os_picc_transfer_direct_transmit_set_w; /* 1 means the data is transmitted in the way of data
                                                 stream, that is, the half-duplex block transmission
                                                 protocol is not applicable;0 represents data
                                                 transfer using the half duplex block transfer
                                                 protocol */
  unsigned char
      configure_technology_type_w; /* Set whether the configure_technology_type_val parameter can be
                                      written：1—allowed, other values--not allowed */
  unsigned char configure_technology_type_val; /* Configure the modulation technology type of the
                                                  data sent by OsPiccTransfer()*/
  /* Guide says reserved[34] and sizeof(PCD_USER_ST)=76, but these fields make
   * sizeof=72 on ARM. Do not call OsPiccSetUserConfig without ABI validation. */
  unsigned char reserved[34];
} PCD_USER_ST;

typedef struct {
  unsigned char Cmd[4];      /*CLA, INS, P1, P2*/
  int LC;                    /* The valid length of DataIn sent to IC card */
  unsigned char DataIn[512]; /* The data sent to ICC */
  int LE;                    /* The expected length of returned data*/
} ST_APDU_REQ;

typedef struct {
  int LenOut;                 /* Data length returned from ICC*/
  unsigned char DataOut[512]; /*Data pointer returned from ICC */
  unsigned char SWA;          /*status word 1 of ICC */
  unsigned char SWB;          /* status word 2 of ICC */
} ST_APDU_RSP;

#include "osal_types_extra.h"

void OsAES(const unsigned char *Input, unsigned char *Output, const unsigned char *AesKey,
           int KeyLen, int Mode);
int OsBaseCheck(void);
int OsBaseClose(void);
int OsBaseConnect(ST_HandsetLinkSet lk, int timeout);
int OsBaseDisconnect(void);
int OsBaseOpen(void);
int OsBaseScan(ST_HandsetLinkInfo *links, int n, int timeout);
void OsBeep(int ToneType, int DurationMs);
int OsCameraCapture(char *Buf, int Size);
void OsCameraClose(void);
int OsCameraGetParam(CameraAttribute *CameraParam);
int OsCameraOpen(int Index);
int OsCameraSetParam(CameraAttribute *CameraParam);
int OsCheckBattery(void);
int OsCheckPowerSupply(void);
void OsCloseFont(int Handle);
int OsCodeConvert(const char *FromCharset, const char *ToCharset, const char *InBuf, char *OutBuf,
                  unsigned int LenOut);
int OsCryptFormat(const char *Dev, const char *Pwd, const char *FsType);
int OsCryptMount(const char *Dev, const char *Pwd, const char *Target, const char *FsType);
int OsCryptUmount(const char *Target);
void OsDES(const unsigned char *Input, unsigned char *Output, const unsigned char *DesKey,
           int KeyLen, int Mode);
int OsEnumFont(FT_FONT **FontList);
int OsFirmwareGetVersion(char *Version, int Size);
int OsFirmwareUpgrade(const char *FwFileName);
int OsGetAppExitCode(void);
int OsGetAppInfo(ST_APP_INFO AppInfo[], int InfoCnt);
int OsGetFontDot(int Handle, const char *Utf8Code, const int Width, const int Height,
                 const int Style, FT_DOT *FtDot);
int OsGetOptInfo(ST_OPT_INFO OptInfo[], int InfoCnt);
void OsGetRandom(unsigned char *Random, int RandomLen);
void OsGetSysVer(int VerType, char *Version);
unsigned long OsGetTickCount(void);
void OsGetTime(ST_TIME *Time);
void OsGpsClose(void);
int OsGpsOpen(int Mode, const char *Attr);
int OsGpsRead(GPS_LOCATION *Location);
int OsIccClose(int Slot);
int OsIccDetect(int Slot);
int OsIccExchange(int Slot, int CtrlFlag, const ST_APDU_REQ *ApduReq, ST_APDU_RSP *ApduRsp);
int OsIccInit(int Slot, unsigned long Option, unsigned char *Atr);
int OsIccOpen(int Slot);
int OsInstallFile(const char *Name, const char *FileName, int FileType);
void OsKbBacklight(int OnOff);
int OsLed(int Id, unsigned int Color, const char *dev);
int OsLog(LOG_T Prio, const char *fmt, ...);
void OsLogSetTag(const char *Tag);
int OsMifareAuthority(unsigned char *uid, unsigned char blk_no, unsigned char group,
                      unsigned char *psw);
int OsMifareOperate(unsigned char ucOpCode, unsigned char ucSrcBlkNo, unsigned char *pucVal,
                    unsigned char ucDesBlkNo);
int OsModemCheck(void);
void OsModemClose(void);
int OsModemConnect(const ST_MODEM_SETUP *Setup, const unsigned char *TelNo);
int OsModemExCmd(const char *Cmd, char *Rsp, int *RespLen, int TimeoutMs);
void OsModemHangup(void);
int OsModemOpen(void);
int OsModemRecv(void *RecvBuf, int BufSize, int Timeout);
int OsModemSend(const void *SendBuf, int SendLen);
int OsModemSwitchPower(int OnOff);
int OsMount(const char *Source, const char *Target, const char *FileSystemType,
            unsigned long MountFlags, const void *Data);
void OsMsrClose(void);
int OsMsrOpen(void);
int OsMsrRead(ST_MSR_DATA *Track1, ST_MSR_DATA *Track2, ST_MSR_DATA *Track3);
void OsMsrReset(void);
int OsMsrSwiped(void);
int OsNetAddArp(int NetLink, const char *Addr, const char MAC[6]);
int OsNetCheckDhcp(int NetLink);
int OsNetCheckDhcp6(int NetLink);
int OsNetDelRouteTable(int NetLink, const char *DestAddr, const char *GenMask);
int OsNetDns(const char *Name, char *Addr, int TimeOutMs);
int OsNetDnsEx(const char *Name, struct DnsAddrInfo *Addr, int TimeOutMs);
int OsNetGetConfig(int NetLink, char *Addr, char *Mask, char *Gateway, char *DNSServer);
int OsNetGetIPv6Addr(int NetLink, struct IPv6Info *AddrInfo);
int OsNetGetRoute(void);
int OsNetGetRouteAdvertise6(int NetLink, int TimeOutMs, int *RouteMode);
int OsNetGetRouteTable(int NetLink, struct IPv4RouteTable *RouteTable);
int OsNetGetRouteTable6(int NetLink, struct IPv6RouteTable *RoutingTable);
int OsNetPing(const char *Addr, int TimeOutMs);
int OsNetPing6(const char *DestAddr, const char *SrcAddr, int TimeOutMs, int size);
int OsNetPingEx(const char *Addr, int TimeOutMs, int Size);
int OsNetSetConfig(int NetLink, const char *Addr, const char *Mask, const char *Gateway,
                   const char *DNSServer);
int OsNetSetDns(const char *Dns);
int OsNetSetIPv6Addr(int NetLink, const char *Addr, int PrefixLength);
int OsNetSetRoute(int NetLink);
int OsNetSetRouteTable(int NetLink, const char *DestAddr, const char *GenMask, const char *GateWay);
int OsNetSetRouteTable6(int NetLink, const char *DestAddr, int DestAddrPrefixLength,
                        const char *NextHop);
int OsNetStartDhcp(int NetLink);
int OsNetStartDhcp6(int NetLink, int state);
void OsNetStopDhcp(int NetLink);
int OsNetStopDhcp6(int NetLink);
int OsOnBase(void);
int OsOpenFont(const char *FileName);
int OsPedAes(int KeyIdx, unsigned char *InitVector, const unsigned char *DataIn, int DataInLen,
             unsigned char *DataOut, int Mode);
int OsPedCancelPinEntry(void);
void OsPedClose(void);
int OsPedCustomKeypad(char *IconPath, uchar *KeypadColor, uchar Mode);
int OsPedDeriveKey(int SrcKeyType, int SrcKeyIdx, int DstKeyType, int DstFromKeyIdx,
                   int DstToKeyIdx, int Mode);
int OsPedDes(int KeyIdx, unsigned char *InitVector, const unsigned char *DataIn, int DataInLen,
             unsigned char *DataOut, int Mode);
int OsPedDesDukpt(int GroupIdx, int KeyVarType, unsigned char *InitVector, int DataInLen,
                  unsigned char *DataIn, unsigned char *DataOut, unsigned char *Ksn, int Mode);
int OsPedEraseKeys(void);
int OsPedGenSM2Pair(uchar *PvtKey, uchar *PubKey, int KeyLenBit);
int OsPedGetKcv(int KeyType, int KeyIdx, int KcvMode, int KcvDataLen, unsigned char *KcvData,
                unsigned char *Kcv);
int OsPedGetKsnDukpt(int GroupIdx, unsigned char *Ksn);
int OsPedGetMac(int KeyIdx, const unsigned char *DataIn, int DataInLen, unsigned char *Mac,
                int Mode);
int OsPedGetMacDukpt(int GroupIdx, const unsigned char *DataIn, int DataInLen, unsigned char *Mac,
                     unsigned char *Ksn, int Mode);
int OsPedGetMacSM(int KeyIdx, uchar *InitVector, uchar *Input, int InputLen, uchar *MacOut,
                  int Mode);
int OsPedGetPinBlock(int KeyIdx, const unsigned char *DataIn, const char *ExpPinLen, int Mode,
                     unsigned long TimeoutMs, unsigned char *PinBlock);
int OsPedGetPinBlockSM4(int KeyIdx, const char *ExpPinLenIn, uchar *DataIn, uchar *PinBlockOut,
                        int Mode, ulong TimeoutMs);
int OsPedGetPinDukpt(int GroupIdx, const unsigned char *DataIn, const char *ExpPinLen, int Mode,
                     unsigned long TimeoutMs, unsigned char *Ksn, unsigned char *PinBlock);
int OsPedGetVer(unsigned char *PedVer);
int OsPedIncreaseKsnDukpt(int GroupIdx);
int OsPedOpen(void);
int OsPedReadCipherRsaKey(int RsaKeyIdx, unsigned char *CipherRsaKey);
int OsPedReadRsaKey(int RsaKeyIdx, ST_RSA_KEY *RsaKey);
int OsPedRsaRecover(int KeyIdx, int DataInLen, unsigned char *DataIn, unsigned char *DataOut,
                    unsigned char *KeyInfo);
int OsPedSM2Recover(int KeyIdx, uchar *Input, int InputLen, uchar *Output, int *OutputLen,
                    int Mode);
int OsPedSM2Sign(int PubKeyIdx, int PvtKeyIdx, uchar *Uid, int UidLen, uchar *Input, int InputLen,
                 uchar *Signature);
int OsPedSM2Verify(int PubKeyIdx, uchar *Uid, int UidLen, uchar *Input, int InputLen,
                   const uchar *Signature);
int OsPedSM3(uchar *Input, int InputLen, uchar *Output, int Mode);
int OsPedSM4(int KeyIdx, uchar *InitVector, uchar *Input, int InputLen, uchar *Output, int Mode);
int OsPedSetAsteriskLayout(int x, int y, int fontSize, int fontColor, uchar align);
int OsPedSetFunctionKey(int KeyFlag);
int OsPedSetInterval(unsigned long TpkIntervalMs);
int OsPedSetOfflinePin(uchar Mode, uchar TpkIdx, uchar *PinBlock, ushort PinBlockLen);
int OsPedSetPinBg(uchar Mode, const uchar *Bg, int BgLen);
int OsPedSetPinIconLayout(int X, int Y, int Interval, uchar IconNum, char *PinIcon, int PinIconLen,
                          char *PinIconBG, int PinIconBGLen);
int OsPedUpdatePinBlock(int UpdateFlag, const unsigned char *KeyInfo, const unsigned char *DataIn,
                        unsigned char *PinBlock, int Mode);
int OsPedVerifyCipherPin(int IccSlot, const ST_RSA_PINKEY *RsaPinKey, const char *ExpPinLen,
                         int Mode, unsigned long TimeoutMs, unsigned char *IccRsp);
int OsPedVerifyPlainPin(int IccSlot, const char *ExpPinLen, int Mode, unsigned long TimeoutMs,
                        unsigned char *IccRsp);
int OsPedWriteAesKey(const unsigned char *KeyBlock);
int OsPedWriteCipherRsaKey(int RsaKeyIdx, int CipherRsaKeyLen, unsigned char *CipherRsaKey);
int OsPedWriteKey(const unsigned char *KeyBlock);
int OsPedWriteKeyVar(int KeyType, int SrcKeyIdx, int DstKeyIdx, const unsigned char *KeyVar);
int OsPedWriteRsaKey(int RsaKeyIdx, ST_RSA_KEY *RsaKey);
int OsPedWriteSM2Key(int KeyIdx, int KeyType, uchar *KeyValue);
int OsPedWriteTIK(const unsigned char *KeyBlock);
int OsPiccActive(const char pcPiccType, unsigned char *pucRATS);
int OsPiccAntiSel(const char pcPiccType, unsigned char *pucUID, const unsigned char ucATQ0,
                  unsigned char *pucSAK);
int OsPiccApplePoll(char *pcPiccType, unsigned char *pucATQx, unsigned char *pucSendData);
int OsPiccClose(void);
int OsPiccGetUserConfig(PCD_USER_ST *pcd_user_config);
int OsPiccInitFelica(unsigned char ucSpeed, unsigned char ucModInvert);
int OsPiccIsoCommand(int cid, ST_APDU_REQ *ApduReq, ST_APDU_RSP *ApduRsp);
int OsPiccOffCarrier(void);
int OsPiccOpen(void);
int OsPiccPoll(char *pcPiccType, unsigned char *pucATQx);
int OsPiccRemove(void);
int OsPiccResetCarrier(void);
int OsPiccSetUserConfig(PCD_USER_ST *pcd_user_config);
int OsPiccTransfer(const unsigned char *pucTxBuff, int iTxLen, unsigned char *pucRxBuff,
                   int *piRxLen);
int OsPlayWave(const char *Buf, int Len, int Volume, int DurationMs);
PM_MSG_T OsPmGetEvent(int TimeoutMs);
int OsPmRequest(PM_REQ_T ReqType);
int OsPortCheckTx(int Channel);
void OsPortClose(int Channel);
int OsPortOpen(int Channel, const char *Attr);
int OsPortRecv(int Channel, void *RecvBuf, int RecvLen, int TimeoutMs);
int OsPortReset(int Channel);
int OsPortSend(int Channel, const void *SendBuf, int SendLen);
int OsPowerOff(void);
int OsPppoeCheck(void);
int OsPppoeLogin(const char *Name, const char *Password, int TimeOutMs);
void OsPppoeLogout(void);
int OsPppomCheck(void);
int OsPppomLogin(const char *Name, const char *Password, long Auth, int TimeOutMs);
int OsPppomLogout(void);
int OsPrnCheck(void);
void OsPrnClose(void);
void OsPrnClrBuf(void);
void OsPrnFeed(int Pixel);
int OsPrnGetDotLine(void);
int OsPrnOpen(unsigned int printertype, const char *targetname);
void OsPrnPrintf(const char *Str, ...);
void OsPrnPutImage(const unsigned char *Logo);
void OsPrnReset(void);
void OsPrnSelectFontSize(int SingleCodeWidth, int SingleCodeHeight, int MultiCodeWidth,
                         int MultiCodeHeight);
int OsPrnSetDirection(unsigned char Mode);
int OsPrnSetFont(const char *fontname);
void OsPrnSetGray(int Level);
int OsPrnSetIndent(unsigned int Left, unsigned int Right);
int OsPrnSetParam(unsigned int cmd);
void OsPrnSetReversal(int Attr);
int OsPrnSetSize(unsigned int Width, unsigned int Height);
void OsPrnSetSpace(int CharSpace, int LineSpace);
int OsPrnStart(void);
int OsRSA(const unsigned char *Modulus, int ModulusLen, const unsigned char *Exp, int ExpLen,
          const unsigned char *DataIn, unsigned char *DataOut);
int OsRSAKeyGen(unsigned char *Modulus, unsigned char *PriExp, int ModulusLen,
                const unsigned char *PubExp);
int OsReboot(void);
int OsRecordCheck(void);
void OsRecordClose(void);
int OsRecordOpen(void);
int OsRecordStart(const char *FileName, const char *Type, unsigned int Channel, const char *Format,
                  unsigned int Rate, unsigned int Duration);
int OsRecordStop(unsigned int *Time);
int OsRegGetValue(const char *Key, char *Value);
int OsRegSetValue(const char *Key, const char *Value);
int OsRunApp(char *AppId, char **Argv, void *Data, RUNAPP_CB CbOut, RUNAPP_CB CbErr);
void OsSHA(int Mode, const void *Data, int DataLen, unsigned char *ShaOut);
void OsSaveCrashReport(int sig);
void OsScanClose(void);
int OsScanOpen(void);
int OsScanRead(char *Buf, int Len, int TimeoutMs);
int OsScanSetParam(unsigned int Param);
void OsScrBrightness(int Brightness);
void OsScrGetSize(int *Width, int *Height);
int OsSetTime(const ST_TIME *Time);
void OsSleep(unsigned int Ms);
int OsStopPlayWave(void);
int OsSysSleep(void);
int OsSysSleepEx(int Level);
int OsSysSleepTime(int Time);
unsigned long OsTimerCheck(ST_TIMER *Timer);
int OsTimerSet(ST_TIMER *Timer, unsigned long Ms);
int OsUmount(const char *Target, int Flags);
int OsUninstallFile(const char *AppName, int FileType);
int OsVerifySign(const char *FileName, int PUKType);
int OsVerifySignExternal(const char *FileName, const void *SignData, int PUKType);
int OsWakeupSource(void);
int OsWifiCheck(char *Essid, char *Bssid, int *Rssi);
void OsWifiClose(void);
int OsWifiCmd(const char *Argv[], int Argc, char *Result, int Len);
int OsWifiConnect(const ST_WifiApSet *Ap, int TimeOutMs);
int OsWifiDisconnect(void);
int OsWifiOpen(void);
int OsWifiScan(ST_WifiApInfo **Aps);
int OsWifiSwitchPower(int Type);
int OsWifiWpsConnect(WPS_MODE Mode, char *WpsPin);
int OsWlCheck(void);
int OsWlGetSignal(void);
int OsWlInit(const char *SimPin);
int OsWlInitEx(const char *SimPin, int TimeOutMs, char *CmeString, int Size);
int OsWlLock(void);
int OsWlLogin(const char *APN, const char *Name, const char *Password, long Auth, int TimeOutMs,
              int KeepAlive, int ReserParam);
int OsWlLoginEx(const char *DialNum, const char *APN, const char *Name, const char *Password,
                long Auth, int TimeOutMs, int KeepAlive, int ReserParam);
int OsWlLogout(void);
int OsWlSelSim(int simno);
int OsWlSwitchPower(int OnOff);
int OsWlSwitchSleep(int OnOff);
void OsWlUnLock(void);
#endif
