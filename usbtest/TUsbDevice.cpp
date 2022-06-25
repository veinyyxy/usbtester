#include "TUsbDevice.h"

using namespace std;

#define TIMEOUT 1000
#define READ_TIMEOUT TIMEOUT
#define WRITE_TIMEOUT TIMEOUT

TUsbDevice::TUsbDevice(uint16_t vid, uint16_t pid, LPGUID lpGuid, bool nonblocking,
                       bool buffered, bool readOverlapped, bool writeOverlapped,
                       uint32_t maxReadBlockSize, uint32_t maxWriteBlockSize)
    : vid_(vid), pid_(pid), lpGuid_(lpGuid), buffered_(buffered), readOverlapped_(readOverlapped),
      writeOverlapped_(writeOverlapped), readHandle_(INVALID_HANDLE_VALUE),
      writeHandle_(INVALID_HANDLE_VALUE), maxWriteBlockSize_(maxWriteBlockSize), maxReadBlockSize_(maxReadBlockSize){
    deviceName_[0] = '\0';
}

TUsbDevice::TUsbDevice(const char *deviceName, bool nonblocking, bool buffered,
                       bool readOverlapped, bool writeOverlapped,
                       uint32_t maxReadBlockSize, uint32_t maxWriteBlockSize)
    : vid_(0), pid_(0), buffered_(buffered), readOverlapped_(readOverlapped),
      writeOverlapped_(writeOverlapped), readHandle_(INVALID_HANDLE_VALUE),
      writeHandle_(INVALID_HANDLE_VALUE), maxWriteBlockSize_(maxWriteBlockSize), maxReadBlockSize_(maxReadBlockSize) {
    strcpy_s(deviceName_, MAX_DEVNAME_LEN - 1, deviceName);
}

TUsbDevice::~TUsbDevice() { close(); }

int32_t TUsbDevice::open() {
    if (isOpen()) {
        return 0;
    }

    // get usb device path with vid and pid if deviceName_ is empty.
    if (strlen(deviceName_) == 0) {
        vector<string> list;
        getUsbDevicePath(vid_, pid_, lpGuid_, list);
        if (!list.empty()) {
            // here use the first usb device we found.
            string path = list.at(0);
            strcpy_s(deviceName_, MAX_DEVNAME_LEN - 1, path.c_str());
        }
    }

    // open usb device.
    if (strlen(deviceName_) > 0) {
        readHandle_ = CreateFileA((LPCSTR)(deviceName_), GENERIC_READ,
                                  FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                  readOverlapped_ ? FILE_FLAG_OVERLAPPED
                                                  : FILE_ATTRIBUTE_NORMAL,
                                  NULL);
        if (readHandle_ != INVALID_HANDLE_VALUE) {
            writeHandle_ = CreateFileA((LPCSTR)(deviceName_), GENERIC_WRITE,
                                       FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                       writeOverlapped_ ? FILE_FLAG_OVERLAPPED
                                                        : FILE_ATTRIBUTE_NORMAL,
                                       NULL);
            if (writeHandle_ == INVALID_HANDLE_VALUE) {
                CloseHandle(readHandle_);
                readHandle_ = INVALID_HANDLE_VALUE;
            }
        }
    }

    return writeHandle_ != INVALID_HANDLE_VALUE ? 0 : -1;
}

int32_t TUsbDevice::close() {
    cancelIo();
    if (isOpen()) {
        CloseHandle(readHandle_);
        CloseHandle(writeHandle_);
    }
    readHandle_ = INVALID_HANDLE_VALUE;
    writeHandle_ = INVALID_HANDLE_VALUE;
    return 0;
}

bool TUsbDevice::isOpen() { return writeHandle_ != INVALID_HANDLE_VALUE; }

int32_t TUsbDevice::flush() { return isOpen() ? 0 : -1; }

long TUsbDevice::read(uint8_t *buf, uint32_t len) {
    if (!isOpen() || len > maxReadBlockSize_) {
        return -1;
    }
    if (!buffered_ && len < maxReadBlockSize_) {
        return -1;
    }

    DWORD bytes = 0;
    OVERLAPPED overlapped;
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    overlapped.hEvent = 0;
    LPOVERLAPPED ov = readOverlapped_ ? &overlapped : NULL;
    BOOL done = ReadFile(readHandle_, (LPVOID)buf, (DWORD)len, &bytes, ov);
    if (done == FALSE) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            printf("ReadFile error: %u\n", error);
            return -1;
        }
        if (GetOverlappedResult(readHandle_, ov, &bytes, TRUE) == FALSE) {
			printf("GetOverlappedResult error: %u\n", GetLastError());
            return -1;
        }
    }
    //CFG_LOG("read bytes = %u\n", bytes);
    assert(bytes <= len);

    return bytes;
}

long TUsbDevice::write(const uint8_t *buf, uint32_t len) {
    if (!isOpen() || len > maxWriteBlockSize_) {
		printf("Usb device does not open or buf is too large.\n");
        return -1;
    }
    DWORD bytes = 0;
    OVERLAPPED overlapped;
    overlapped.Offset = 0;
    overlapped.OffsetHigh = 0;
    overlapped.hEvent = 0;
    LPOVERLAPPED ov = writeOverlapped_ ? &overlapped : NULL;
    BOOL done =
        WriteFile(writeHandle_, (LPCVOID)buf, (DWORD)len, &bytes, ov);
    if (done == FALSE) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
			printf("WriteFileError: %u\n", error);
            return -1;
        }
        if (GetOverlappedResult(writeHandle_, ov, &bytes, TRUE) == FALSE) {
			printf("GetOverlappedResult error: %u\n", GetLastError());
            return -1;
        }
    }
    //CFG_LOG("write bytes = %u\n", bytes);
    assert(bytes == len);

    return bytes;
}

void TUsbDevice::cancelIo() {
    if (isOpen()) {
        if (readOverlapped_) {
            if (!CancelIoEx(readHandle_, NULL)) {
				printf("CancelIoEx error: %u\n", GetLastError());
            }
        }
        if (writeOverlapped_) {
            if (!CancelIoEx(writeHandle_, NULL)) {
				printf("CancelIoEx error: %u\n", GetLastError());
            }
        }
    }
}

//void TUsbDevice::cancelIo(std::thread::native_handle_type handle) {
//    if (isOpen()) {
//        if (!readOverlapped_ || !writeOverlapped_) {
//            if (!CancelSynchronousIo((HANDLE)handle)) {
//				printf("CancelSynchronousIo error: %u\n", GetLastError());
//            }
//        }
//    }
//    cancelIo();
//}