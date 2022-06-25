#include "usbutil.h"

#include <windows.h>

#include <cfgmgr32.h>
#include <setupapi.h>
#include <string.h>
#include <tchar.h>
#include <usb100.h>
#include <usb200.h>
#include <usbioctl.h>

#define NUM_HCS_TO_CHECK 10
#define INITGUID

//#include "c:\WinDDK\7600.16385.1\inc\api\devpkey.h"

// include DEVPKEY_Device_BusReportedDeviceDesc from
// WinDDK\7600.16385.1\inc\api\devpropdef.h
#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
//typedef ULONG DEVPROPID, *PDEVPROPID;
//typedef ULONG DEVPROPTYPE, *PDEVPROPTYPE;
//typedef GUID DEVPROPGUID, *PDEVPROPGUID;
//struct DEVPROPKEY {
//    DEVPROPGUID fmtid;
//    DEVPROPID pid;
//};
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8,     \
                          pid)                                                 \
    EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = {                      \
        {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}, pid}
#else
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8,     \
                          pid)                                                 \
    EXTERN_C const DEVPROPKEY name
#endif // INITGUID

// include DEVPKEY_Device_BusReportedDeviceDesc from
// WinDDK\7600.16385.1\inc\api\devpkey.h
DEFINE_DEVPROPKEY(DEVPKEY_Device_Driver, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20,
                  0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0,
                  11); // DEVPROP_TYPE_STRING

typedef BOOL(WINAPI *FN_SetupDiGetDevicePropertyW)(
    HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData,
    const DEVPROPKEY *PropertyKey, DEVPROPTYPE *PropertyType,
    PBYTE PropertyBuffer, DWORD PropertyBufferSize, PDWORD RequiredSize,
    DWORD Flags);


PSTR WideStrToMultiStr(LPCWSTR WideStr);
PSTR GetDriverKeyName(LPCSTR devicePath, LPGUID lpGuid);
PSTR GetRootHubName(HANDLE HostController);
PSTR GetDriverKeyName(HANDLE Hub, ULONG ConnectionIndex);
PSTR GetExternalHubName(HANDLE Hub, ULONG ConnectionIndex);
BOOL AreThereStringDescriptors(PUSB_DEVICE_DESCRIPTOR DeviceDesc);
PSTRING_DESCRIPTOR_NODE
GetStringDescriptor(HANDLE hHubDevice, ULONG ConnectionIndex,
                    UCHAR DescriptorIndex, USHORT LanguageID);
PSTRING_DESCRIPTOR_NODE
GetStringDescriptors(HANDLE hHubDevice, ULONG ConnectionIndex,
                     UCHAR DescriptorIndex, ULONG NumLanguageIDs,
                     USHORT *LanguageIDs,
                     PSTRING_DESCRIPTOR_NODE StringDescNodeTail);
BOOL GetAllStringDescriptors(HANDLE hHubDevice, ULONG ConnectionIndex,
                             PUSB_DEVICE_DESCRIPTOR DeviceDesc,
                             USB_DEV_PROPS &descriptor);
BOOL GetDeviceDescFromHub(LPCSTR rootHubName, LPCSTR driverKeyName,
                          USB_DEV_PROPS &descriptor);
BOOL GetDeviceDescFromHubPorts(HANDLE hHubDevice, ULONG NumPorts,
                               LPCSTR driverKeyName, USB_DEV_PROPS &descriptor);
BOOL IsChildDevice(LPCSTR parentDriverKeyName, LPCSTR driverKeyName);

VOID getUsbDevicePath(UINT16 vid, UINT16 pid, LPGUID lpGuid, std::vector<std::string> &list) {
    char vid_buf[10];
    char pid_buf[10];
    sprintf_s(vid_buf, "vid_%04x", vid);
    sprintf_s(pid_buf, "pid_%04x", pid);

    HDEVINFO hInfo = SetupDiGetClassDevsA(
        lpGuid, NULL, NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE);
    if (hInfo == INVALID_HANDLE_VALUE) {
        return;
    }

    SP_INTERFACE_DEVICE_DATA interfaceInfo;
    ZeroMemory((PVOID)&interfaceInfo, sizeof(SP_INTERFACE_DEVICE_DATA));
    interfaceInfo.cbSize = sizeof(interfaceInfo);
    interfaceInfo.Flags = 0;

    char *devPath = (char *)malloc(MAX_DEVNAME_LEN);

    for (int index = 0; SetupDiEnumDeviceInterfaces(
             hInfo, NULL, lpGuid, index,
             &interfaceInfo);
         index++) {
        // clear out error list
        GetLastError();

        DWORD needed = 0;
        SetupDiGetInterfaceDeviceDetailA(hInfo, &interfaceInfo, NULL, 0,
                                         &needed, NULL);
        PSP_INTERFACE_DEVICE_DETAIL_DATA_A detail =
            (PSP_INTERFACE_DEVICE_DETAIL_DATA_A)malloc(needed);
        if (!detail) {
            free((PVOID)devPath);
            SetupDiDestroyDeviceInfoList(hInfo);
            return;
        }
        ZeroMemory(detail, sizeof(needed));
        detail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA_A);
        if (!SetupDiGetInterfaceDeviceDetailA(hInfo, &interfaceInfo, detail,
                                              needed, NULL, NULL)) {
            free((PVOID)detail);
            free((PVOID)devPath);
            SetupDiDestroyDeviceInfoList(hInfo);
            return;
        }

        ZeroMemory(devPath, MAX_DEVNAME_LEN);
        char *ptr = (char *)(detail->DevicePath);
        strcpy_s(devPath, MAX_DEVNAME_LEN - 1, ptr);

        free((PVOID)detail);

        if (strstr(devPath, vid_buf) && strstr(devPath, pid_buf)) {
            std::string path;
            path.append(devPath);
            list.push_back(path);
        } else {
            devPath[0] = '\0';
        }
    }

    free((PVOID)devPath);
    SetupDiDestroyDeviceInfoList(hInfo);
}

/*
Assuming you already have USB device handle first you need to get
DEVPKEY_Device_Driver property string from it (by means of
CM_Get_DevNode_PropertyW or SetupDiGetDevicePropertyW).

You'll receive string like {36fc9e60-c465-11cf-8056-444553540000}\0010.

Next you need to iterate over each USB hub in system (devices that have
GUID_DEVINTERFACE_USB_HUB interface) and for each:

Open it via CreateFile() call
Call DeviceIoControl(hubInterfaceHandle, IOCTL_USB_GET_NODE_INFORMATION, ...) to
get USB_NODE_INFORMATION structure that contains number of USB ports in its
hubInfo.u.HubInformation.HubDescriptor.bNumberOfPorts For each port from 1 (they
are one based!!!) to bNumberOfPorts call DeviceIoControl(hubInterfaceHandle,
IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, ...) to get unique DriverKey of
device connected to this port. Compare DriverKey string you have on previous
step with string you have from DEVPKEY_Device_Driver call. If they are same -
congratulations you have found USB hub and port that have your USB device
connected! Now you can call DeviceIoControl(usbHubInterfaceHandle,
IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, ...) to get
USB_NODE_CONNECTION_INFORMATION structure that contains USB_DEVICE_DESCRIPTOR!

Also you can additionally call DeviceIoControl(usbHubInterfaceHandle,
IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, ...) with USB_DESCRIPTOR_REQUEST
to get other USB descriptors in addition to basic USB_DEVICE_DESCRIPTOR.

For example code see EnumerateHubPorts() and GetDriverKeyName() in official
USBView sample.
*/

BOOL getUsbDeviceDescriptor(LPCSTR devicePath, LPGUID lpGuid, USB_DEV_PROPS &descriptor) {
    PSTR driverKeyName = NULL;
    PSTR rootHubName = NULL;
    BOOL ret = FALSE;

    CHAR HCName[16];
    int HCNum;
    HANDLE hHCDev;

    driverKeyName = GetDriverKeyName(devicePath, lpGuid);
    if (!driverKeyName) {
        return FALSE;
    }

    // Iterate over some Host Controller names and try to open them.
    //
    for (HCNum = 0; HCNum < NUM_HCS_TO_CHECK; HCNum++) {
        sprintf_s(HCName, sizeof(HCName) / sizeof(HCName[0]), "\\\\.\\HCD%d",
                  HCNum);

        hHCDev = CreateFileA(HCName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, 0, NULL);

        // If the handle is valid, then we've successfully opened a Host
        // Controller.  Display some info about the Host Controller itself,
        // then enumerate the Root Hub attached to the Host Controller.
        //
        if (hHCDev != INVALID_HANDLE_VALUE) {
            // Get the name of the root hub for this host
            // controller and then enumerate the root hub.
            //
            rootHubName = GetRootHubName(hHCDev);

            if (rootHubName != NULL) {
                if (GetDeviceDescFromHub(rootHubName, driverKeyName,
                                         descriptor)) {
                    ret = TRUE;
                } else {
                    // Can not find usb device.
                }
            } else {
                // Failure obtaining root hub name.
            }
            CloseHandle(hHCDev);
            if (ret) {
                break;
            }
        }
    }

    FREE(driverKeyName);
    FREE(rootHubName);
    return ret;
}

PSTR WideStrToMultiStr(LPCWSTR WideStr) {
    ULONG nBytes;
    PSTR MultiStr;

    // Get the length of the converted string
    //
    nBytes = WideCharToMultiByte(CP_ACP, 0, WideStr, -1, NULL, 0, NULL, NULL);

    if (nBytes == 0) {
        return NULL;
    }

    // Allocate space to hold the converted string
    //
    MultiStr = (PSTR)ALLOC(nBytes);

    if (MultiStr == NULL) {
        return NULL;
    }

    // Convert the string
    //
    nBytes = WideCharToMultiByte(CP_ACP, 0, WideStr, -1, MultiStr, nBytes, NULL,
                                 NULL);

    if (nBytes == 0) {
        FREE(MultiStr);
        return NULL;
    }
    return MultiStr;
}

PSTR GetDriverKeyName(LPCSTR DevicePath, LPGUID lpGuid) {
    DWORD dwSize;
    SP_DEVINFO_DATA devInfoData;
    DEVPROPTYPE ulPropertyType;
    WCHAR szBuffer[4096];
    PSTR driverKeyName = NULL;
    HDEVINFO hInfo = NULL;
    PSTR devPath = NULL;
    SP_INTERFACE_DEVICE_DATA interfaceInfo;
    DWORD needed;
    PSP_INTERFACE_DEVICE_DETAIL_DATA_A detail = NULL;
    FN_SetupDiGetDevicePropertyW fn_SetupDiGetDevicePropertyW =
        (FN_SetupDiGetDevicePropertyW)GetProcAddress(
            GetModuleHandle(TEXT("Setupapi.dll")), "SetupDiGetDevicePropertyW");

    hInfo = SetupDiGetClassDevsA(
        lpGuid, NULL, NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE);
    if (hInfo == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    ZeroMemory((PVOID)&interfaceInfo, sizeof(SP_INTERFACE_DEVICE_DATA));
    interfaceInfo.cbSize = sizeof(interfaceInfo);
    interfaceInfo.Flags = 0;

    devPath = (PSTR)ALLOC(MAX_DEVNAME_LEN);

    for (int index = 0; SetupDiEnumDeviceInterfaces(
             hInfo, NULL, lpGuid, index,
             &interfaceInfo);
         index++) {
        // clear out error list
        GetLastError();

        needed = 0;
        SetupDiGetInterfaceDeviceDetailA(hInfo, &interfaceInfo, NULL, 0,
                                         &needed, NULL);
        detail = (PSP_INTERFACE_DEVICE_DETAIL_DATA_A)ALLOC(needed);
        if (!detail) {
            FREE(devPath);
            SetupDiDestroyDeviceInfoList(hInfo);
            return NULL;
        }
        ZeroMemory(detail, sizeof(needed));
        detail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA_A);
        // The caller must set DeviceInfoData.cbSize to sizeof(SP_DEVINFO_DATA).
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiGetInterfaceDeviceDetailA(hInfo, &interfaceInfo, detail,
                                              needed, NULL, &devInfoData)) {
            FREE(detail);
            FREE(devPath);
            SetupDiDestroyDeviceInfoList(hInfo);
            return NULL;
        }

        ZeroMemory(devPath, MAX_DEVNAME_LEN);
        strcpy_s(devPath, MAX_DEVNAME_LEN - 1, detail->DevicePath);

        FREE(detail);

        if (strcmp(devPath, DevicePath) == 0) {
            // get driver key name
            if (fn_SetupDiGetDevicePropertyW &&
                fn_SetupDiGetDevicePropertyW(hInfo, &devInfoData,
                                             &DEVPKEY_Device_Driver,
                                             &ulPropertyType, (BYTE *)szBuffer,
                                             sizeof(szBuffer), &dwSize, 0)) {
                driverKeyName = WideStrToMultiStr(szBuffer);
            }
            break;
        } else {
            devPath[0] = '\0';
        }
    }

    FREE(devPath);
    SetupDiDestroyDeviceInfoList(hInfo);

    return driverKeyName;
}

PSTR GetRootHubName(HANDLE HostController) {
    BOOL success;
    ULONG nBytes;
    USB_ROOT_HUB_NAME rootHubName;
    PUSB_ROOT_HUB_NAME rootHubNameW;
    PSTR rootHubNameA;

    rootHubNameW = NULL;
    rootHubNameA = NULL;

    // Get the length of the name of the Root Hub attached to the
    // Host Controller
    //
    success = DeviceIoControl(HostController, IOCTL_USB_GET_ROOT_HUB_NAME, 0, 0,
                              &rootHubName, sizeof(rootHubName), &nBytes, NULL);

    if (!success) {
        goto GetRootHubNameError;
    }

    // Allocate space to hold the Root Hub name
    //
    nBytes = rootHubName.ActualLength;

    rootHubNameW = (PUSB_ROOT_HUB_NAME)ALLOC(nBytes);

    if (rootHubNameW == NULL) {
        goto GetRootHubNameError;
    }

    // Get the name of the Root Hub attached to the Host Controller
    //
    success = DeviceIoControl(HostController, IOCTL_USB_GET_ROOT_HUB_NAME, NULL,
                              0, rootHubNameW, nBytes, &nBytes, NULL);

    if (!success) {
        goto GetRootHubNameError;
    }

    // Convert the Root Hub name
    //
    rootHubNameA = WideStrToMultiStr(rootHubNameW->RootHubName);

    // All done, free the uncoverted Root Hub name and return the
    // converted Root Hub name
    //
    FREE(rootHubNameW);

    return rootHubNameA;

GetRootHubNameError:
    // There was an error, free anything that was allocated
    //
    if (rootHubNameW != NULL) {
        FREE(rootHubNameW);
        rootHubNameW = NULL;
    }

    return NULL;
}

PSTR GetDriverKeyName(HANDLE Hub, ULONG ConnectionIndex) {
    BOOL success;
    ULONG nBytes;
    USB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyName;
    PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW;
    PSTR driverKeyNameA;

    driverKeyNameW = NULL;
    driverKeyNameA = NULL;

    // Get the length of the name of the driver key of the device attached to
    // the specified port.
    //
    driverKeyName.ConnectionIndex = ConnectionIndex;

    success =
        DeviceIoControl(Hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                        &driverKeyName, sizeof(driverKeyName), &driverKeyName,
                        sizeof(driverKeyName), &nBytes, NULL);

    if (!success) {
        goto GetDriverKeyNameError;
    }

    // Allocate space to hold the driver key name
    //
    nBytes = driverKeyName.ActualLength;

    if (nBytes <= sizeof(driverKeyName)) {
        goto GetDriverKeyNameError;
    }

    driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)ALLOC(nBytes);

    if (driverKeyNameW == NULL) {
        goto GetDriverKeyNameError;
    }

    // Get the name of the driver key of the device attached to
    // the specified port.
    //
    driverKeyNameW->ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              driverKeyNameW, nBytes, driverKeyNameW, nBytes,
                              &nBytes, NULL);

    if (!success) {
        goto GetDriverKeyNameError;
    }

    // Convert the driver key name
    //
    driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName);

    // All done, free the uncoverted driver key name and return the
    // converted driver key name
    //
    FREE(driverKeyNameW);

    return driverKeyNameA;

GetDriverKeyNameError:
    // There was an error, free anything that was allocated
    //
    if (driverKeyNameW != NULL) {
        FREE(driverKeyNameW);
        driverKeyNameW = NULL;
    }

    return NULL;
}

PSTR GetExternalHubName(HANDLE Hub, ULONG ConnectionIndex) {
    BOOL success;
    ULONG nBytes;
    USB_NODE_CONNECTION_NAME extHubName;
    PUSB_NODE_CONNECTION_NAME extHubNameW;
    PSTR extHubNameA;

    extHubNameW = NULL;
    extHubNameA = NULL;

    // Get the length of the name of the external hub attached to the
    // specified port.
    //
    extHubName.ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub, IOCTL_USB_GET_NODE_CONNECTION_NAME,
                              &extHubName, sizeof(extHubName), &extHubName,
                              sizeof(extHubName), &nBytes, NULL);

    if (!success) {
        goto GetExternalHubNameError;
    }

    // Allocate space to hold the external hub name
    //
    nBytes = extHubName.ActualLength;

    if (nBytes <= sizeof(extHubName)) {
        goto GetExternalHubNameError;
    }

    extHubNameW = (PUSB_NODE_CONNECTION_NAME)ALLOC(nBytes);

    if (extHubNameW == NULL) {
        goto GetExternalHubNameError;
    }

    // Get the name of the external hub attached to the specified port
    //
    extHubNameW->ConnectionIndex = ConnectionIndex;

    success =
        DeviceIoControl(Hub, IOCTL_USB_GET_NODE_CONNECTION_NAME, extHubNameW,
                        nBytes, extHubNameW, nBytes, &nBytes, NULL);

    if (!success) {
        goto GetExternalHubNameError;
    }

    // Convert the External Hub name
    //
    extHubNameA = WideStrToMultiStr(extHubNameW->NodeName);

    // All done, free the uncoverted external hub name and return the
    // converted external hub name
    //
    FREE(extHubNameW);

    return extHubNameA;

GetExternalHubNameError:
    // There was an error, free anything that was allocated
    //
    if (extHubNameW != NULL) {
        FREE(extHubNameW);
        extHubNameW = NULL;
    }

    return NULL;
}

BOOL AreThereStringDescriptors(PUSB_DEVICE_DESCRIPTOR DeviceDesc) {
    //
    // Check Device Descriptor strings
    //

    if (DeviceDesc->iManufacturer || DeviceDesc->iProduct ||
        DeviceDesc->iSerialNumber) {
        return TRUE;
    } else {
        return FALSE;
    }
}

PSTRING_DESCRIPTOR_NODE GetStringDescriptor(HANDLE hHubDevice,
                                            ULONG ConnectionIndex,
                                            UCHAR DescriptorIndex,
                                            USHORT LanguageID) {
    BOOL success;
    ULONG nBytes;
    ULONG nBytesReturned;

    UCHAR stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
                           MAXIMUM_USB_STRING_LENGTH];

    PUSB_DESCRIPTOR_REQUEST stringDescReq;
    PUSB_STRING_DESCRIPTOR stringDesc;
    PSTRING_DESCRIPTOR_NODE stringDescNode;

    nBytes = sizeof(stringDescReqBuf);

    stringDescReq = (PUSB_DESCRIPTOR_REQUEST)stringDescReqBuf;
    stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq + 1);

    // Zero fill the entire request structure
    //
    memset(stringDescReq, 0, nBytes);

    // Indicate the port from which the descriptor will be requested
    //
    stringDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must inititialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    stringDescReq->SetupPacket.wValue =
        (USB_STRING_DESCRIPTOR_TYPE << 8) | DescriptorIndex;

    stringDescReq->SetupPacket.wIndex = LanguageID;

    stringDescReq->SetupPacket.wLength =
        (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(
        hHubDevice, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
        stringDescReq, nBytes, stringDescReq, nBytes, &nBytesReturned, NULL);

    //
    // Do some sanity checks on the return from the get descriptor request.
    //

    if (!success) {
        return NULL;
    }

    if (nBytesReturned < 2) {
        return NULL;
    }

    if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE) {
        return NULL;
    }

    if (stringDesc->bLength !=
        nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST)) {
        return NULL;
    }

    if (stringDesc->bLength % 2 != 0) {
        return NULL;
    }

    //
    // Looks good, allocate some (zero filled) space for the string descriptor
    // node and copy the string descriptor to it.
    //

    stringDescNode = (PSTRING_DESCRIPTOR_NODE)ALLOC(
        sizeof(STRING_DESCRIPTOR_NODE) + stringDesc->bLength);

    if (stringDescNode == NULL) {
        return NULL;
    }

    stringDescNode->DescriptorIndex = DescriptorIndex;
    stringDescNode->LanguageID = LanguageID;

    memcpy(stringDescNode->StringDescriptor, stringDesc, stringDesc->bLength);

    return stringDescNode;
}

PSTRING_DESCRIPTOR_NODE
GetStringDescriptors(HANDLE hHubDevice, ULONG ConnectionIndex,
                     UCHAR DescriptorIndex, ULONG NumLanguageIDs,
                     USHORT *LanguageIDs,
                     PSTRING_DESCRIPTOR_NODE StringDescNodeTail) {
    ULONG i;

    for (i = 0; i < NumLanguageIDs; i++) {
        StringDescNodeTail->Next = GetStringDescriptor(
            hHubDevice, ConnectionIndex, DescriptorIndex, *LanguageIDs);

        if (StringDescNodeTail->Next) {
            StringDescNodeTail = StringDescNodeTail->Next;
        }

        LanguageIDs++;
    }

    return StringDescNodeTail;
}

BOOL GetAllStringDescriptors(HANDLE hHubDevice, ULONG ConnectionIndex,
                             PUSB_DEVICE_DESCRIPTOR DeviceDesc,
                             USB_DEV_PROPS &descriptor) {
    PSTRING_DESCRIPTOR_NODE supportedLanguagesString;
    PSTRING_DESCRIPTOR_NODE stringDescNodeTail;
    PSTR pStrDesc;
    PWSTR pStrDescW;
    ULONG numLanguageIDs;
    USHORT *languageIDs;

    //
    // Get the array of supported Language IDs, which is returned
    // in String Descriptor 0
    //
    supportedLanguagesString =
        GetStringDescriptor(hHubDevice, ConnectionIndex, 0, 0);

    if (supportedLanguagesString == NULL) {
        return FALSE;
    }

    numLanguageIDs =
        (supportedLanguagesString->StringDescriptor->bLength - 2) / 2;

    languageIDs =
        (USHORT *)&supportedLanguagesString->StringDescriptor->bString[0];

    stringDescNodeTail = supportedLanguagesString;

    //
    // Get the Device Descriptor strings
    //

    descriptor.idVendor = DeviceDesc->idVendor;
    descriptor.idProduct = DeviceDesc->idProduct;

    if (DeviceDesc->iManufacturer) {
        stringDescNodeTail = GetStringDescriptors(
            hHubDevice, ConnectionIndex, DeviceDesc->iManufacturer,
            numLanguageIDs, languageIDs, stringDescNodeTail);
        pStrDescW =
            (PWSTR)ALLOC(stringDescNodeTail->StringDescriptor[0].bLength);
        memcpy(pStrDescW, stringDescNodeTail->StringDescriptor[0].bString,
               stringDescNodeTail->StringDescriptor[0].bLength - 2);
        pStrDesc = WideStrToMultiStr(pStrDescW);
        strcpy_s(descriptor.manufacturer, MAX_DEVNAME_LEN - 1, pStrDesc);
        FREE(pStrDesc);
        FREE(pStrDescW);
    }

    if (DeviceDesc->iProduct) {
        stringDescNodeTail = GetStringDescriptors(
            hHubDevice, ConnectionIndex, DeviceDesc->iProduct, numLanguageIDs,
            languageIDs, stringDescNodeTail);
        pStrDescW =
            (PWSTR)ALLOC(stringDescNodeTail->StringDescriptor[0].bLength);
        memcpy(pStrDescW, stringDescNodeTail->StringDescriptor[0].bString,
               stringDescNodeTail->StringDescriptor[0].bLength - 2);
        pStrDesc = WideStrToMultiStr(pStrDescW);
        strcpy_s(descriptor.product, MAX_DEVNAME_LEN - 1, pStrDesc);
        FREE(pStrDesc);
        FREE(pStrDescW);
    }

    if (DeviceDesc->iSerialNumber) {
        stringDescNodeTail = GetStringDescriptors(
            hHubDevice, ConnectionIndex, DeviceDesc->iSerialNumber,
            numLanguageIDs, languageIDs, stringDescNodeTail);
        pStrDescW =
            (PWSTR)ALLOC(stringDescNodeTail->StringDescriptor[0].bLength);
        memcpy(pStrDescW, stringDescNodeTail->StringDescriptor[0].bString,
               stringDescNodeTail->StringDescriptor[0].bLength - 2);
        pStrDesc = WideStrToMultiStr(pStrDescW);
        strcpy_s(descriptor.serialNumber, MAX_DEVNAME_LEN - 1, pStrDesc);
        FREE(pStrDesc);
        FREE(pStrDescW);
    }

    if (supportedLanguagesString != NULL) {
        PSTRING_DESCRIPTOR_NODE Next;
        do {
            Next = supportedLanguagesString->Next;
            FREE(supportedLanguagesString);
            supportedLanguagesString = Next;
        } while (supportedLanguagesString != NULL);
    }

    return TRUE;
}

BOOL GetDeviceDescFromHub(LPCSTR rootHubName, LPCSTR driverKeyName,
                          USB_DEV_PROPS &descriptor) {
    PSTR deviceName;
    size_t deviceNameSize;
    HANDLE hHubDevice = INVALID_HANDLE_VALUE;
    PUSB_NODE_INFORMATION hubInfo = NULL;
    BOOL success;
    ULONG nBytes;

    // Allocate some space for a USB_NODE_INFORMATION structure for this Hub,
    //
    hubInfo = (PUSB_NODE_INFORMATION)ALLOC(sizeof(USB_NODE_INFORMATION));

    if (hubInfo == NULL) {
        goto EnumerateHubError;
    }

    // Allocate a temp buffer for the full hub device name.
    //
    deviceNameSize = strlen(rootHubName) + strlen("\\\\.\\") + 1;
    deviceName = (PSTR)ALLOC(deviceNameSize * sizeof(CHAR));

    if (deviceName == NULL) {
        goto EnumerateHubError;
    }

    // Create the full hub device name
    //
    strcpy_s(deviceName, deviceNameSize, "\\\\.\\");
    strcat_s(deviceName, deviceNameSize, rootHubName);

    // Try to hub the open device
    //
    hHubDevice = CreateFileA(deviceName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, 0, NULL);

    // Done with temp buffer for full hub device name
    //
    FREE(deviceName);

    if (hHubDevice == INVALID_HANDLE_VALUE) {
        goto EnumerateHubError;
    }

    //
    // Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
    // This will tell us the number of downstream ports to enumerate, among
    // other things.
    //
    success = DeviceIoControl(hHubDevice, IOCTL_USB_GET_NODE_INFORMATION,
                              hubInfo, sizeof(USB_NODE_INFORMATION), hubInfo,
                              sizeof(USB_NODE_INFORMATION), &nBytes, NULL);

    if (!success) {
        goto EnumerateHubError;
    }

    if (!GetDeviceDescFromHubPorts(
            hHubDevice, hubInfo->u.HubInformation.HubDescriptor.bNumberOfPorts,
            driverKeyName, descriptor)) {
        goto EnumerateHubError;
    }

    CloseHandle(hHubDevice);
    FREE(hubInfo);
    return TRUE;

EnumerateHubError:
    //
    // Clean up any stuff that got allocated
    //

    if (hHubDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hHubDevice);
    }

    if (hubInfo) {
        FREE(hubInfo);
    }

    return FALSE;
}

BOOL GetDeviceDescFromHubPorts(HANDLE hHubDevice, ULONG NumPorts,
                               LPCSTR driverKeyName,
                               USB_DEV_PROPS &descriptor) {
    ULONG index;
    BOOL success;
    BOOL found = FALSE;

	PUSB_NODE_CONNECTION_INFORMATION_EX connectionInfoEx = { 0 };

    PSTR deviceDriverKeyName;

    // Loop over all ports of the hub.
    //
    // Port indices are 1 based, not 0 based.
    //
    for (index = 1; index <= NumPorts; index++) {
        ULONG nBytesEx;

        // Allocate space to hold the connection info for this port.
        // For now, allocate it big enough to hold info for 30 pipes.
        //
        // Endpoint numbers are 0-15.  Endpoint number 0 is the standard
        // control endpoint which is not explicitly listed in the Configuration
        // Descriptor.  There can be an IN endpoint and an OUT endpoint at
        // endpoint numbers 1-15 so there can be a maximum of 30 endpoints
        // per device configuration.
        //
        // Should probably size this dynamically at some point.
        //
        nBytesEx = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) +
                   sizeof(USB_PIPE_INFO) * 30;

        connectionInfoEx = (PUSB_NODE_CONNECTION_INFORMATION_EX)ALLOC(nBytesEx);

        if (connectionInfoEx == NULL) {
            break;
        }

        //
        // Now query USBHUB for the USB_NODE_CONNECTION_INFORMATION_EX structure
        // for this port.  This will tell us if a device is attached to this
        // port, among other things.
        //
        connectionInfoEx->ConnectionIndex = index;

        success = DeviceIoControl(hHubDevice,
                                  IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                  connectionInfoEx, nBytesEx, connectionInfoEx,
                                  nBytesEx, &nBytesEx, NULL);

        if (!success) {
            PUSB_NODE_CONNECTION_INFORMATION connectionInfo;
            ULONG nBytes;

            // Try using IOCTL_USB_GET_NODE_CONNECTION_INFORMATION
            // instead of IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX
            //
            nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION) +
                     sizeof(USB_PIPE_INFO) * 30;

            connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION)ALLOC(nBytes);

            connectionInfo->ConnectionIndex = index;

            success = DeviceIoControl(
                hHubDevice, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
                connectionInfo, nBytes, connectionInfo, nBytes, &nBytes, NULL);

            if (!success) {
                FREE(connectionInfo);
                FREE(connectionInfoEx);
                continue;
            }

            // Copy IOCTL_USB_GET_NODE_CONNECTION_INFORMATION into
            // IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX structure.
            //
            connectionInfoEx->ConnectionIndex = connectionInfo->ConnectionIndex;

            connectionInfoEx->DeviceDescriptor =
                connectionInfo->DeviceDescriptor;

            connectionInfoEx->CurrentConfigurationValue =
                connectionInfo->CurrentConfigurationValue;

            connectionInfoEx->Speed =
                connectionInfo->LowSpeed ? UsbLowSpeed : UsbFullSpeed;

            connectionInfoEx->DeviceIsHub = connectionInfo->DeviceIsHub;

            connectionInfoEx->DeviceAddress = connectionInfo->DeviceAddress;

            connectionInfoEx->NumberOfOpenPipes =
                connectionInfo->NumberOfOpenPipes;

            connectionInfoEx->ConnectionStatus =
                connectionInfo->ConnectionStatus;

            memcpy(&connectionInfoEx->PipeList[0], &connectionInfo->PipeList[0],
                   sizeof(USB_PIPE_INFO) * 30);

            FREE(connectionInfo);
        }

        if (connectionInfoEx->ConnectionStatus != NoDeviceConnected) {
            deviceDriverKeyName = GetDriverKeyName(hHubDevice, index);

            if (deviceDriverKeyName) {
                if (strcmp(deviceDriverKeyName, driverKeyName) == 0) {
                    FREE(deviceDriverKeyName);
                    break;
                }
                // If the device connected to the port is a usb
                // composite device, enum the child devices.

                // The device class field of the device descriptor
                // (bDeviceClass) must contain a value of zero, or the class
                // (bDeviceClass), subclass (bDeviceSubClass), and protocol
                // (bDeviceProtocol) fields of the device descriptor must have
                // the values 0xEF, 0x02 and 0x01 respectively, as explained in
                // USB Interface Association Descriptor.
                if (connectionInfoEx->DeviceDescriptor.bDeviceClass == 0x00 ||
                    (connectionInfoEx->DeviceDescriptor.bDeviceClass == 0xEF &&
                     connectionInfoEx->DeviceDescriptor.bDeviceSubClass ==
                         0x02 &&
                     connectionInfoEx->DeviceDescriptor.bDeviceProtocol ==
                         0x01)) {
                    if (IsChildDevice(deviceDriverKeyName, driverKeyName)) {
                        FREE(deviceDriverKeyName);
                        break;
                    }
                }
                FREE(deviceDriverKeyName);
            }
        }

        // If the device connected to the port is an external hub, get the
        // name of the external hub and recursively enumerate it.
        //
        if (connectionInfoEx->DeviceIsHub) {
            PSTR extHubName;

            extHubName = GetExternalHubName(hHubDevice, index);

            if (extHubName != NULL) {
                if (GetDeviceDescFromHub(extHubName, driverKeyName,
                                         descriptor)) {
                    found = TRUE;
                    break;
                }
            }
        }

        if (connectionInfoEx) {
            FREE(connectionInfoEx);
            connectionInfoEx = NULL;
        }
    }

    if (!found && connectionInfoEx) {
        if (AreThereStringDescriptors(&connectionInfoEx->DeviceDescriptor)) {
            if (GetAllStringDescriptors(hHubDevice, index,
                                        &connectionInfoEx->DeviceDescriptor,
                                        descriptor)) {
                found = TRUE;
            }
        }
    }

    if (connectionInfoEx) {
        FREE(connectionInfoEx);
        connectionInfoEx = NULL;
    }

    return found;
}

BOOL IsChildDevice(LPCSTR parentDriverKeyName, LPCSTR driverKeyName) {
    DEVINST devInst;
    DEVINST devInstNext;
    DEVINST childDevInst;
    CONFIGRET cr;
    BOOL hasNext;
    ULONG walkDone = 0;
    CHAR buf[MAX_DEVICE_ID_LEN];
    ULONG len = sizeof(buf) / sizeof(buf[0]);

    // Get Root DevNode
    //
    cr = CM_Locate_DevNodeA(&devInst, NULL, 0);

    if (cr != CR_SUCCESS) {
        return FALSE;
    }

    // Do a depth first search for the DevNode with a matching
    // parentDriverKeyName value
    //
    while (!walkDone) {
        // Get the DriverName value
        //
        cr = CM_Get_DevNode_Registry_PropertyA(devInst, CM_DRP_DRIVER, NULL,
                                               buf, &len, 0);
        // If the parentDriverKeyName value matches, enum child devices.
        //
        if (cr == CR_SUCCESS && _stricmp(parentDriverKeyName, buf) == 0) {
            cr = CM_Get_Child(&devInstNext, devInst, 0);
            if (cr == CR_SUCCESS) {
                cr = CM_Get_DevNode_Registry_PropertyA(
                    devInstNext, CM_DRP_DRIVER, NULL, buf, &len, 0);
                if (cr == CR_SUCCESS && _stricmp(driverKeyName, buf) == 0) {
                    return TRUE;
                }
                childDevInst = devInstNext;
                hasNext = TRUE;
                while (hasNext) {
                    cr = CM_Get_Sibling(&devInstNext, childDevInst, 0);
                    if (cr == CR_SUCCESS) {
                        cr = CM_Get_DevNode_Registry_PropertyA(
                            devInstNext, CM_DRP_DRIVER, NULL, buf, &len, 0);
                        if (cr == CR_SUCCESS &&
                            _stricmp(driverKeyName, buf) == 0) {
                            return TRUE;
                        }
                        childDevInst = devInstNext;
                        hasNext = TRUE;
                    } else {
                        hasNext = FALSE;
                    }
                }
            }
        }

        // This DevNode didn't match, go down a level to the first child.
        //
        cr = CM_Get_Child(&devInstNext, devInst, 0);

        if (cr == CR_SUCCESS) {
            devInst = devInstNext;
            continue;
        }

        // Can't go down any further, go across to the next sibling.  If
        // there are no more siblings, go back up until there is a sibling.
        // If we can't go up any further, we're back at the root and we're
        // done.
        //
        for (;;) {
            cr = CM_Get_Sibling(&devInstNext, devInst, 0);

            if (cr == CR_SUCCESS) {
                devInst = devInstNext;
                break;
            }

            cr = CM_Get_Parent(&devInstNext, devInst, 0);

            if (cr == CR_SUCCESS) {
                devInst = devInstNext;
            } else {
                walkDone = 1;
                break;
            }
        }
    }

    return FALSE;
}