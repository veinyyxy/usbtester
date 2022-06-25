#pragma once

#include <windows.h> //һ��Ҫ�����ͷ�ļ�
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

	//����usb��VID��PID���豸
	BOOL DeviceOpen(HANDLE&handle, WORD wVID, WORD wPID);

	//�ر��豸���ͷž������Դ
	void DeviceClose();

	//��usbд������
	BOOL DeviceWrite(LPCVOID lpBuffer, DWORD dwSize);

	//��usb��ȡ��Ӧ����
	void DeviceRead();

	//��ȡ�����߳�
	static void ReadThread(CommUsb *usb);
private:
	HANDLE m_handle;
	int m_size;
	DWORD m_dwResult;
public:
	BYTE m_rBuffer[64];

};