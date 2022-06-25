#ifndef USBUTIL_H
#define USBUTIL_H 1

#include <windows.h>

#include <string>
#include <usb100.h>
#include <usb200.h>
#include <vector>

#define MAX_DEVNAME_LEN 2048

#define ALLOC(dwBytes) GlobalAlloc(GPTR, (dwBytes))
#define REALLOC(hMem, dwBytes)                                                 \
    GlobalReAlloc((hMem), (dwBytes), (GMEM_MOVEABLE | GMEM_ZEROINIT))
#define FREE(hMem) GlobalFree((hMem))
#define CHECKFORLEAKS()

static const GUID GUID_DEVINTERFACE_USBPRINT = {
    0x28D78FAD, 0x5A12, 0x11D1, 0xAE, 0x5B, 0x00, 0x00, 0xF8, 0x03, 0xA8, 0xC2};

static const GUID GUID_DEVINTERFACE_IMAGE = {
    0x6BDD1FC6, 0x810F, 0x11D0, 0xBE, 0xC7, 0x08, 0x00, 0x2B, 0xE2, 0x09, 0x2F};

#ifdef __cplusplus
extern "C" {
#endif

//
// Structure used to build a linked list of String Descriptors
// retrieved from a device.
//

typedef struct _STRING_DESCRIPTOR_NODE {
    struct _STRING_DESCRIPTOR_NODE *Next;
    UCHAR DescriptorIndex;
    USHORT LanguageID;
    USB_STRING_DESCRIPTOR StringDescriptor[0];
} STRING_DESCRIPTOR_NODE, *PSTRING_DESCRIPTOR_NODE;

typedef struct _USB_DEV_PROPS {
    UINT16 idVendor;
    UINT16 idProduct;
    CHAR manufacturer[MAX_DEVNAME_LEN];
    CHAR product[MAX_DEVNAME_LEN];
    CHAR serialNumber[MAX_DEVNAME_LEN];
} USB_DEV_PROPS, *PUSB_DEV_PROPS;

VOID getUsbDevicePath(UINT16 vid, UINT16 pid, LPGUID lpGuid, std::vector<std::string> &list);
BOOL getUsbDeviceDescriptor(LPCSTR devicePath, LPGUID lpGuid, USB_DEV_PROPS &descriptor);

#ifdef __cplusplus
}
#endif

#endif // USBUTIL_H