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

//查找所有vidpid
void HID_FindAllDevices()
{
	GUID                             HidGuid;
	// 设备信息集的句柄
	HDEVINFO                         hDevInfo = NULL;
	// HID 信息结构体
	HIDD_ATTRIBUTES					 stDevAttributes;
	// 设备接口信息结构
	SP_DEVICE_INTERFACE_DATA         stDevData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDevDetail = NULL;
	HANDLE                           hDevHandle = NULL;
	ULONG                            Length = 0;

	//vecVidPid.clear();
	// 获取initguid.h头文件的 GUID_DEVINTERFACE_HID 
	HidD_GetHidGuid(&HidGuid);
	// 获取所有当前设备的设备信息集的句柄
	hDevInfo = SetupDiGetClassDevs(&HidGuid,    // 指向设备设置类或设备接口类的GUID的指针
		NULL, // 即插即用（PnP）枚举器的标识符（全局唯一标识符（GUID）或符号名），可以为NULL
		NULL, // 顶层窗口的句柄，用于与在设备信息集中安装设备实例相关联的用户界面，可以为NULL
		(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE) // 仅返回系统中当前存在的设备,返回支持指定设备接口类的设备接口的设备
	);

	int Index = -1;
	/* Scan all Devices */
	while (true)
	{
		Index++;
		memset(&stDevData, 0x0, sizeof(stDevData));
		stDevData.cbSize = sizeof(stDevData);
		// 枚举中包含的设备信息组的设备
		BOOL bRet = SetupDiEnumDeviceInterfaces(hDevInfo,
			NULL,
			&HidGuid,   // 指定所请求接口的设备接口类
			Index,      // 从0开始遍历设备
			&stDevData
		);
		if (!bRet)
		{
			break;
		}
		// 获取设备详细信息数据长度
		bRet = SetupDiGetDeviceInterfaceDetail(hDevInfo, &stDevData, NULL, 0, &Length, NULL);
		// 为设备详细信息数据分配内存 
		pDevDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Length);
		if (pDevDetail == NULL)
		{
			continue;
		}
		// 在DevDetail结构中设置cbSize
		pDevDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		// 获取设备详细信息数据
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
	// 当不在需要使用SetupDiGetClassDevs返回的设备信息集时，
	// 必须调用 SetupDiDestroyDeviceInfoList 删除设备信息集
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
