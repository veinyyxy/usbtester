#include <cstdio>
#include <iostream>
#include <vector>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
extern "C" {
#include <hidsdi.h> 
}
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")
using namespace std;

struct _VID_PID_
{
	USHORT V_ID;
	USHORT P_ID;
	_VID_PID_(USHORT vid, USHORT pid)
	{
		V_ID = vid;
		P_ID = pid;
	}
	bool operator==(const _VID_PID_& other)
	{
		if (V_ID == other.V_ID && P_ID == other.P_ID)
		{
			return true;
		}
		return false;
	}
};
std::vector<_VID_PID_> vecVidPid;

//��������vidpid
void HID_FindAllDevices()
{
	GUID                             HidGuid;
	// �豸��Ϣ���ľ��
	HDEVINFO                         hDevInfo = NULL;
	// HID ��Ϣ�ṹ��
	HIDD_ATTRIBUTES					 stDevAttributes;
	// �豸�ӿ���Ϣ�ṹ
	SP_DEVICE_INTERFACE_DATA         stDevData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDevDetail = NULL;
	HANDLE                           hDevHandle = NULL;
	ULONG                            Length = 0;

	//vecVidPid.clear();
	// ��ȡinitguid.hͷ�ļ��� GUID_DEVINTERFACE_HID 
	HidD_GetHidGuid(&HidGuid);
	// ��ȡ���е�ǰ�豸���豸��Ϣ���ľ��
	hDevInfo = SetupDiGetClassDevs(&HidGuid,    // ָ���豸��������豸�ӿ����GUID��ָ��
		NULL, // ���弴�ã�PnP��ö�����ı�ʶ����ȫ��Ψһ��ʶ����GUID�����������������ΪNULL
		NULL, // ���㴰�ڵľ�������������豸��Ϣ���а�װ�豸ʵ����������û����棬����ΪNULL
		(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE) // ������ϵͳ�е�ǰ���ڵ��豸,����֧��ָ���豸�ӿ�����豸�ӿڵ��豸
	);

	int Index = -1;
	/* Scan all Devices */
	while (true)
	{
		Index++;
		memset(&stDevData, 0x0, sizeof(stDevData));
		stDevData.cbSize = sizeof(stDevData);
		// ö���а������豸��Ϣ����豸
		BOOL bRet = SetupDiEnumDeviceInterfaces(hDevInfo,
			NULL,
			&HidGuid,   // ָ��������ӿڵ��豸�ӿ���
			Index,      // ��0��ʼ�����豸
			&stDevData
		);
		if (!bRet)
		{
			break;
		}
		// ��ȡ�豸��ϸ��Ϣ���ݳ���
		bRet = SetupDiGetDeviceInterfaceDetail(hDevInfo, &stDevData, NULL, 0, &Length, NULL);
		// Ϊ�豸��ϸ��Ϣ���ݷ����ڴ� 
		pDevDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Length);
		if (pDevDetail == NULL)
		{
			continue;
		}
		// ��DevDetail�ṹ������cbSize
		pDevDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		// ��ȡ�豸��ϸ��Ϣ����
		bRet = SetupDiGetDeviceInterfaceDetail(hDevInfo, &stDevData, pDevDetail, Length, NULL, NULL);
		if (!bRet)
		{
			free(pDevDetail);
			pDevDetail = NULL;
			continue;
		}
		/* Create File for Device Read/Write */
		hDevHandle = CreateFile(pDevDetail->DevicePath,
			0,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			(LPSECURITY_ATTRIBUTES)NULL,
			OPEN_EXISTING,
			0,
			NULL
		);
		if (hDevHandle == INVALID_HANDLE_VALUE)
		{
			free(pDevDetail);
			pDevDetail = NULL;
			continue;
		}
		memset(&stDevAttributes, 0x0, sizeof(stDevAttributes));
		stDevAttributes.Size = sizeof(stDevAttributes);
		bRet = HidD_GetAttributes(hDevHandle, &stDevAttributes);
		if (!bRet)
		{
			free(pDevDetail);
			pDevDetail = NULL;
			CloseHandle(hDevHandle);
			hDevHandle = NULL;
			continue;
		}

		if (find(vecVidPid.begin(), vecVidPid.end(), _VID_PID_(stDevAttributes.VendorID, stDevAttributes.ProductID)) == vecVidPid.end())
		{
			vecVidPid.push_back(_VID_PID_(stDevAttributes.VendorID, stDevAttributes.ProductID));
		}
		free(pDevDetail);
		pDevDetail = NULL;
		CloseHandle(hDevHandle);
		hDevHandle = NULL;
	};
	// ��������Ҫʹ��SetupDiGetClassDevs���ص��豸��Ϣ��ʱ��
	// ������� SetupDiDestroyDeviceInfoList ɾ���豸��Ϣ��
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return;
}

int main1() {
	HID_FindAllDevices();
	for (int i = 0; i < (int)vecVidPid.size(); i++)
	{
		std::cout << "VID: " << vecVidPid[i].V_ID << "    PID: " << vecVidPid[i].P_ID << std::endl;
	}
	system("pause");
	return 0;
}
