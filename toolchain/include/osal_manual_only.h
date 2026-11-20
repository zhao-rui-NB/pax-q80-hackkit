#ifndef Q80_OSAL_MANUAL_ONLY_H
#define Q80_OSAL_MANUAL_ONLY_H

/* Prolin API Guide 2.5.3 names absent from this Q80's libosal.so.
 * These declarations are for comparing API versions. Calls cannot link on this Q80.
 * Two names below come from headings whose PDF prototypes use different names.
 */
#include "osal.h"

int OsBaseCmd(int cmd, void *inparm, void *outparm);
int OsCameraDetectMotion(int threshold, int interval, int timeout);
int OsCheckBMSMode(int *CurrentMode, int *Capacity, int *FullCharge, int *Recharge);
int OsCheckPortStatus(int Channel);
int OsCheckPowerStatus(int *PowerStatus);
int OsDigitalIOGetStat(int *value);
int OsDigitalIOSetStat(int index, int value);
int OsGetBaseInfo(const char *Key, char *Value);
int OsIccTransfer(int Slot, int CtrlFlag, const unsigned char *pucTxBuff, int iTxLen,
                  unsigned char *pucRxBuff, int *piRxLen);
int OsMsrReadJIS(ST_MSR_DATA *Track1, ST_MSR_DATA *Track2, ST_MSR_DATA *Track3,
                 ST_MSR_DATA *Track4);
int OsNetNat(int DestLink);
int OsPedCalcAesDukpt(uchar GroupIdx, uchar KeyVarType, uchar *InitVector, uint DataInLen,
                      const uchar *DataIn, uchar KeyType, uchar Mode, uchar *Ksn, uchar *DataOut);
int OsPedCalcAesGcm(uchar KeyIdx, const uchar *InitVector, uint InitVectorLen, const uchar *DataIn,
                    uint DataLen, const uchar *Aad, uint AadLen, uchar *DataOut, uchar *AuthTag,
                    uint AuthTagLen, uchar Mode);
int OsPedDFAuthDiver(int SrcKeyIdx, int KeySetVer, int DivType, uchar Mode, uchar *Uid,
                     uchar *EncRndB, uchar *EncSessionKey);
int OsPedDFAuthMerge(uchar *EncRndA, int DataLen);
int OsPedEccPointMul(uchar EccType, uchar EccPrivateKeyIndex, uint EccPeerPublicKeyLen,
                     const uchar *EccPeerPublicKey, uchar *EccSharedKey, uchar Mode);
int OsPedEccSign(uchar EccType, uchar EccPrivateKeyIndex, uint DataLen, const uchar *Data,
                 uint *SignLen, uchar *Sign, uchar Mode);
int OsPedEccVerify(uchar EccType, uchar EccPublicKeyIndex, uint DataLen, const uchar *Data,
                   uint SignLen, const uchar *Sign, uchar Mode);
int OsPedEndPinEntry(void);
int OsPedGenerateEccKeyPair(uchar EccType, uchar EccUsage, uchar EccPrivateKeyIndex,
                            uint EccPublicKeyLen, uchar *EccPublicKey, uchar Mode);
int OsPedGetKsnAesDukpt(uchar GroupIdx, uchar *Ksn);
int OsPedGetMacAes(uchar KeyIndex, uchar *DataIn, uint DataInLen, uchar *Mac, uchar Mode);
int OsPedGetMacAesDukpt(uchar GroupIdx, const uchar *DataIn, uint DataInLen, uchar KeyType,
                        uchar Mode, uchar *Ksn, uchar *Mac);
int OsPedGetPinAesDukpt(uchar GroupIdx, const char *DataIn, const char *ExpPinLen, uchar KeyType,
                        uchar Mode, ulong TimeoutMs, uchar *Ksn, uchar *PinBlock);
int OsPedInjectKeyBlock(uchar *KeyBlock, uint KeyBlkLen);
int OsPedPinKeyNotify(int *KeyCacheCnt, uchar *KeyCache);
int OsPedReadEccKey(uchar EccType, uchar EccPublicKeyIndex, uint EccPublicKeyLen,
                    uchar *EccPublicKey, uchar Mode);
int OsPedRkiInjectKey(int KeyBlkLen, uchar *KeyBlk, uchar DstKeyIdx);
int OsPedWriteAesTIK(const uchar *KeyBlock);
int OsPedWriteEccKey(uchar EccType, uchar EccUsage, uchar EccKeyIndex, uint EccKeyLen,
                     const uchar *EccKey, uchar Mode);
int OsPiccInitIso15693(unsigned char ucDataCodeMode);
int OsPlayAudio(const char *Buf, int Len, int Volume);
int OsPortCheckRx(int Channel);
int OsPrnRawData(const char *data, int len);
int OsScanDecodeBuf(const unsigned char *DataIn, int DataInLen, unsigned char *DataOut, int MaxLen,
                    int Width, int Height, int Format);
int OsStopPlayAudio(void);
int OsTerminalConsumeInfo(const char *Key, int *Value);
void OsScrContrast(int Contrast);
void OsSetKeyTone(int OnOff, int DutyCycle);
int OsPedIncreaseKsnAesDukpt(uchar GroupIdx);
int OsPedReadEccPublicKey(uchar EccType, uchar EccPublicKeyIndex, uint EccPublicKeyLen,
                          uchar *EccPublicKey, uchar Mode);

#endif
