#pragma once

#include <windows.h> //一定要加入该头文件
#include <iostream>
#include <vector>

extern "C" {
#include <hidsdi.h> 
#include <setupapi.h>  
}
#pragma comment (lib, "setupapi.lib")
#pragma comment (lib, "hid.lib")
using namespace std;


class CommUsb
{
public:
	CommUsb();
	~CommUsb();

	//根据usb的VID与PID打开设备
	BOOL DeviceOpen(HANDLE&handle, WORD wVID, WORD wPID);

	//关闭设备，释放句柄类资源
	void DeviceClose();

	//向usb写入数据
	BOOL DeviceWrite(LPCVOID lpBuffer, DWORD dwSize);

	//从usb读取响应数据
	void DeviceRead();

	//读取数据线程
	static void ReadThread(CommUsb *usb);
private:
	HANDLE m_handle;
	int m_size;
	DWORD m_dwResult;
public:
	BYTE m_rBuffer[64];

};