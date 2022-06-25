#include <windows.h>
#include <cstdio>
#include <iostream>
#include <algorithm>
#include <vector>
#include <atlstr.h>
#include <strmif.h>
#include <setupapi.h>
#include <initguid.h>

#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"strmiids.lib")
using namespace std;

DEFINE_GUID(CLSID_SystemDeviceEnum, 0x62be5d10, 0x60eb, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

//存放设备的PID VID DEVID信息;
typedef struct _DevInfo
{
	int nPID;
	int nVID;
	int nDevId;
}DevInfo, *pDevInfo;

std::vector<DevInfo> g_vecDevInfo;
//获取所有设备的PID_VID Begin
HRESULT EnumAllDevices()
{
	int nRet = -1;
	// 初始化当前线程上的COM库
	CoInitialize(NULL);
	ICreateDevEnum* pDevEnum = NULL;
	// 创建与指定的类标识符关联的类的单个未初始化对象。
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, // 用于创建对象的数据和代码关联的类标识符
		NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (LPVOID*)&pDevEnum);
	if (SUCCEEDED(hr))
	{
		IEnumMoniker* pEnumMon = NULL;
		hr = pDevEnum->CreateClassEnumerator(GUID_DEVINTERFACE_HID, &pEnumMon, 0);
		if (hr == S_OK)
		{
			pEnumMon->Reset();
			ULONG cFetched = 0;
			IMoniker* pMoniker = NULL;
			while (hr = pEnumMon->Next(1, &pMoniker, &cFetched), hr == S_OK)
			{
				IPropertyBag* pProBag = NULL;
				hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (LPVOID*)&pProBag);
				if (SUCCEEDED(hr))
				{
					OLECHAR* pName = NULL;
					hr = pMoniker->GetDisplayName(NULL, NULL, &pName);
					if (SUCCEEDED(hr))
					{
						std::wstring szTemp;
						BSTR bsTemp = SysAllocString(pName); // 分配内存
						szTemp = bsTemp;
						SysFreeString(bsTemp);
						pName = NULL;
						//解析字符串
						szTemp.erase(0, szTemp.find(_T("#")) + 1);
						szTemp.erase(szTemp.find(_T("#")), 1000);
						szTemp.erase(17, 1000);
						//获取PID VID 
						int VID = 0, PID = 0;
						//szTemp.MakeUpper();
						//sscanf(szTemp.mid(13, 4), _T("%X"), &PID);
						//sscanf(szTemp.Mid(4, 4), _T("%X"), &VID);
						DevInfo stDevinfo;
						stDevinfo.nPID = PID;
						stDevinfo.nVID = VID;
						//设备ID号，从0开始
						g_vecDevInfo.push_back(stDevinfo);
						nRet = 0;
					}
					pProBag->Release();
				}
				pMoniker->Release();
			}
			pEnumMon->Release();
		}
	}
	CoUninitialize();
	return nRet;
}

int main2() {
	EnumAllDevices();
	for (int i = 0; i < (int)g_vecDevInfo.size(); i++)
	{
		std::cout << "VID: " << g_vecDevInfo[i].nVID << "    PID: " << g_vecDevInfo[i].nPID << std::endl;
	}
	system("pause");
	return 0;
}