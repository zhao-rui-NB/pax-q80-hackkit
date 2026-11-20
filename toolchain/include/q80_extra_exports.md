# Q80 額外匯出名稱

以下名稱存在於此 Q80 的 `.so`，但未列入 Prolin XUI 2.1.7／OSAL 2.5.3 的可用標頭。
沒有可靠的參數原型，因此不在 `xui.h` 或 `osal.h` 宣告；名稱存在不代表可安全呼叫。

## XUI：2 個手冊標頭外的匯出

- `XuiButtonRegisterKey`
- `XuiTsUpdate`

## OSAL：34 個手冊標頭外的匯出

- `OsBaseInfo`
- `OsBaseUpdate`
- `OsBootUpgrade`
- `OsEnableCrashReports`
- `OsIccGetFd`
- `OsIccLockCard`
- `OsIccUnlockCard`
- `OsModemGetUart`
- `OsMsrReadEx`
- `OsPedAdminInjectTLK`
- `OsPedAdminLogon`
- `OsPedAdminModifyPwd`
- `OsPedAdminOutputLog`
- `OsPedAdminUartDownload`
- `OsPedEraseKey`
- `OsPedExportIdKey`
- `OsPedGetIdKeyState`
- `OsPedGetPinLength`
- `OsPedIdKeyCalc`
- `OsPedMsrKeyEncDec`
- `OsPedSM2CertExport`
- `OsPedSetIdKeyState`
- `OsPedSymCalcDukpt`
- `OsPedWriteCipherSM2Key`
- `OsPedWriteKeyEx`
- `OsPiccGetParam`
- `OsPiccSetParam`
- `OsPppDirectCheck`
- `OsPppDirectLogin`
- `OsPppDirectLogout`
- `OsProfileConfig`
- `OsSlipAttachEx`
- `OsSlipDetachEx`
- `OsWlReAttachGprs`
