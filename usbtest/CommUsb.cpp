#include "CommUsb.h"
#include <thread>

CommUsb::CommUsb()
{
	m_rBuffer[64] = { 0 };
	m_handle = NULL;
	m_size = 0;
	m_dwResult = 0;
}

CommUsb::~CommUsb()
{

}

void CommUsb::ReadThread(CommUsb *usb)
{
	ReadFile(usb->m_handle, usb->m_rBuffer, usb->m_size, &usb->m_dwResult, NULL);
}

BOOL CommUsb::DeviceOpen(HANDLE&handle, WORD wVID, WORD wPID)
{
	BOOL bRet = FALSE;
	GUID hidGuid;
	HDEVINFO hardwareDeviceInfo;
	SP_INTERFACE_DEVICE_DATA deviceInfoData;
	PSP_INTERFACE_DEVICE_DETAIL_DATA functionClassDeviceData = NULL;
	ULONG predictedLength = 0;
	ULONG requiredLength = 0;
	CloseHandle(handle);
	handle = INVALID_HANDLE_VALUE;
	deviceInfoData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
	HidD_GetHidGuid(&hidGuid);
	hardwareDeviceInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
	for (int i = 0; i < 200000; i++)
	{
		if (!SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, 0, &hidGuid, i, &deviceInfoData))
		{
			std::cout << "Error : " << GetLastError() << std::endl;
			continue;
		}
		SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, &deviceInfoData, NULL, 0, &requiredLength, NULL);
		predictedLength = requiredLength;
		functionClassDeviceData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(predictedLength);
		if (!functionClassDeviceData) continue;
		functionClassDeviceData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
		if (!SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, &deviceInfoData, functionClassDeviceData, predictedLength, &requiredLength, NULL)) break;
		handle = CreateFile(functionClassDeviceData->DevicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);//   �����ڶ�����������ָ��Ϊ�첽FILE_FLAG_OVERLAPPED   0Ϊͬ��
																																// cout <<"devicePath"<<functionClassDeviceData->DevicePath<<endl;
		if (handle != INVALID_HANDLE_VALUE)
		{
			HIDD_ATTRIBUTES attri;
			HidD_GetAttributes(handle, &attri);
			if ((attri.VendorID == wVID) && (attri.ProductID == wPID))
			{

				m_handle = handle;   //��USB�豸����������Ա����
				bRet = TRUE;
				break;
			}
			CloseHandle(handle);
			handle = INVALID_HANDLE_VALUE;
		}


	}
	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	return bRet;
}

void CommUsb::DeviceClose()
{

	CloseHandle(m_handle);

	m_handle = INVALID_HANDLE_VALUE;
}

BOOL CommUsb::DeviceWrite(LPCVOID lpBuffer, DWORD dwSize)
{
	if (m_handle == INVALID_HANDLE_VALUE)
	{
		//MessageBox(NULL,"����д��ʧ��","ʧ�ܣ�",MB_OK);
		return 0;
	}

	DWORD dwRet;
	BOOL bRet;

	PHIDP_PREPARSED_DATA PreparsedData;
	HIDP_CAPS Capabilities;

	HidD_GetPreparsedData(m_handle, &PreparsedData);
	HidP_GetCaps(PreparsedData, &Capabilities);

	m_rBuffer[0] = 0x00;  //��һ���ֽ�Ϊreport Id,����ʡ��

	memcpy(m_rBuffer + 1, lpBuffer, min(20, dwSize));

	//����д�ĳ���Ҫ����Capabilities.OutputReportByteLength

	bRet = WriteFile(m_handle, m_rBuffer, Capabilities.OutputReportByteLength, &dwRet, NULL);
	if (bRet)
	{
		//MessageBox(NULL,"д�����ݳɹ�","�ɹ���",MB_OK);
	}
	return bRet;
}

void CommUsb::DeviceRead()
{
	if (m_handle == INVALID_HANDLE_VALUE)
	{
		//MessageBox(NULL, "���ݶ�ȡʧ��", "ʧ�ܣ�", MB_OK);
		//return 0;
	}

	PHIDP_PREPARSED_DATA PreparsedData;
	HIDP_CAPS Capabilities;
	HidD_GetPreparsedData(m_handle, &PreparsedData);
	HidP_GetCaps(PreparsedData, &Capabilities);
	//memcpy(&wBuffer[2], lpBuffer, min(6, dwSize));
	COMMTIMEOUTS timeout;
	timeout.ReadIntervalTimeout = 1000;
	timeout.ReadTotalTimeoutConstant = 1000;
	timeout.ReadTotalTimeoutMultiplier = 1000;
	SetCommTimeouts(m_handle, &timeout);

	//���ĳ���Ҳһ��Ҫ��Capabilities.OutputReportByteLength

	//�̺߳���������ʼ��
	m_size = Capabilities.OutputReportByteLength;

	thread Rthread(ReadThread, this);
	Rthread.join();


	//return bRet;
}

