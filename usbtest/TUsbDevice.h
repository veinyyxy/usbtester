#ifndef TUSBDEVICE_H
#define TUSBDEVICE_H 1

#include <windows.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "usbutil.h"


class TUsbDevice{
  public:
    /**
     * Constructor
     *
     * @param vid
     * @param pid
     * @param blockSize
     */
    TUsbDevice(uint16_t vid, uint16_t pid, LPGUID lpGuid, bool nonblocking, bool buffered,
               bool readOverlapped, bool writeOverlapped,
               uint32_t maxReadBlockSize = 511,
               uint32_t maxWriteBlockSize = 511);

    TUsbDevice(const char *deviceName, bool nonblocking, bool buffered,
               bool readOverlapped, bool writeOverlapped,
               uint32_t maxReadBlockSize = 511,
               uint32_t maxWriteBlockSize = 511);

    /**
     * Destructor
     *
     */
    ~TUsbDevice();

    int32_t open();
    int32_t close() ;
    bool isOpen() ;
    int32_t flush() ;
    long read(uint8_t *buf, uint32_t len) ;
    long write(const uint8_t *buf, uint32_t len) ;
    void cancelIo() ;

  private:
    uint16_t vid_;
    uint16_t pid_;
    char deviceName_[MAX_DEVNAME_LEN];
    HANDLE readHandle_;
    HANDLE writeHandle_;
    bool buffered_;
    bool readOverlapped_;
    bool writeOverlapped_;
    LPGUID lpGuid_;
	uint32_t maxReadBlockSize_;
	uint32_t maxWriteBlockSize_;
};

#endif // #ifndef TUSBDEVICE_H
