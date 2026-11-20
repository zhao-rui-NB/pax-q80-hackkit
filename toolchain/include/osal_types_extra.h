#ifndef Q80_OSAL_TYPES_EXTRA_H
#define Q80_OSAL_TYPES_EXTRA_H

/* Types used by the Prolin API Programming Guide 2.5.3 prototypes. */
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LOG_T;

typedef void (*RUNAPP_CB)(char *appid, char *str, void *data);

typedef struct {
  char Name[64];
  char Version[32];
} ST_OPT_INFO;

typedef struct {
  int ModulusLen;
  unsigned char Modulus[512];
  int ExponentLen;
  unsigned char Exponent[512];
  unsigned char KeyInfo[128];
} ST_RSA_KEY;

typedef struct {
  int ModulusLen;
  unsigned char Modulus[256];
  unsigned char Exponent[4];
  int IccRandomLen;
  unsigned char IccRandom[8];
} ST_RSA_PINKEY;

typedef struct {
  int CallMode, CommMode, CodeType, CodeDuration, CodeSpacing;
  int DetectLineVoltage, DetectDialTone, DialToneTimeout, CommaPauseTime;
  char ConnectRate[20];
  char ConnectFormat[20];
  int ConnectTimeout, DialTimes, IdleTimeout, Pppom;
  int Reserved[9];
} ST_MODEM_SETUP;

typedef struct {
  double latitude, longitude, altitude;
} GPS_LOCATION;

typedef struct _HandsetLinkInfo {
  int type;
  union {
    struct {
      char name[32], mac[32];
    } bt;
    unsigned char reserved[160];
  } u;
} ST_HandsetLinkInfo;

typedef struct _HandsetLinkSet {
  int type;
  union {
    struct {
      char mac[32];
    } bt;
    unsigned char reserved[1024];
  } u;
} ST_HandsetLinkSet;

typedef struct SCameraAttribute {
  int SupportResolution, CurrentResolution;
  int SupportPixelFormat, CurrentPixelFormat;
  char Reserved[128];
} CameraAttribute;

typedef struct _WepSecKey {
  char Key[4][40];
  int KeyLen, Idx;
} WEP_SEC_KEY;

typedef struct _WpaPskKey {
  char Key[64];
  int KeyLen;
} WPA_PSK_KEY;

typedef struct _WpaEapKey {
  int EapType;
  char Pwd[132], Id[132], CaCert[132], CliCert[132], PriKey[132], PriKeyPwd[132];
} WPA_EAP_KEY;

typedef struct _WifiApInfo {
  char Essid[33], Bssid[20];
  int Channel, Mode, Rssi, AuthMode, SecMode;
} ST_WifiApInfo;

typedef struct _WifiApSet {
  char Essid[33], Bssid[20];
  int Channel, Mode, AuthMode, SecMode;
  union KEY_UNION {
    WEP_SEC_KEY WepKey;
    WPA_PSK_KEY PskKey;
    WPA_EAP_KEY EapKey;
  } KeyUnion;
} ST_WifiApSet;

typedef enum { WPS_MODE_PBC = 1, WPS_MODE_PIN_CLIENT, WPS_MODE_PIN_AP } WPS_MODE;

typedef enum {
  PM_MSG_NO_EVENT,
  PM_MSG_ENTER_SLEEP,
  PM_MSG_EXIT_SLEEP,
  PM_MSG_ENTER_SCREENSAVER,
  PM_MSG_EXIT_SCREENSAVER,
  PM_MSG_ENTER_POWEROFF,
  PM_MSG_POWER_ABNORMAL,
  PM_MSG_BATTERY_DAMAGE
} PM_MSG_T;

typedef enum {
  PM_FORBID_SLEEP,
  PM_ALLOW_SLEEP,
  PM_FORBID_SCREENSAVER,
  PM_ALLOW_SCREENSAVER,
  PM_FORBID_POWEROFF,
  PM_ALLOW_POWEROFF
} PM_REQ_T;

struct IPv6Info {
  char LinkAddr[64];
  int NumSiteAddr;
  char SiteAddr[4][64];
  int NumGlobalAddr;
  char GlobalAddr[8][64];
  int NumDns;
  char Dns[2][64];
};

struct IPv6RouteTable {
  char RouteNum;
  struct {
    char DestAddr[64], DestAddrPrefixLen, NextHop[64];
  } RouteTable[16];
};

struct DnsAddrInfo {
  int NumIPv4Addr;
  char IPv4Addr[4][16];
  int NumIPv6Addr;
  char IPv6Addr[4][64];
};

struct IPv4RouteTable {
  int RouteNum;
  struct {
    char DestAddr[16], GenMask[16], GateWay[16];
  } RouteTable4[16];
};

#endif
