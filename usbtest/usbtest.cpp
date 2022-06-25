// usbtest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "TUsbDevice.h"
#include <iostream>

extern BOOL GUIDFromStringW(
	_In_  LPCTSTR psz,
	_Out_ LPGUID  pguid
);

#define BUFFER_SIZE 511

#define CONVERT_STR_2_GUID(cstr, stGuid) do\
{\
    swscanf_s((const wchar_t*)cstr, L"{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}",\
    &(stGuid.Data1),&(stGuid.Data2),&(stGuid.Data3),\
    &(stGuid.Data4[0]),&(stGuid.Data4[1]),&(stGuid.Data4[2]),&(stGuid.Data4[3]),\
    &(stGuid.Data4[4]),&(stGuid.Data4[5]),&(stGuid.Data4[6]),&(stGuid.Data4[7]));\
}while(0);

int main3(int argc, char** argv)
{
	GUID pGUID = { 0 };
	//CONVERT_STR_2_GUID(TEXT("{28d78fad-5a12-11d1-ae5b-0000f803a8c2}"), pGUID);
	TUsbDevice tub(0x3308, 0x1331, &pGUID, false, false, true, true);
	HANDLE handle = 0;
	//USB\VID_3308&PID_1331&MI_00\6&7bd3e1&0&0000
	//28d78fad-5a12-11d1-ae5b-0000f803a8c2
	int32_t result = tub.open();
	if (result == 0)
	{
		uint8_t* pChar = new uint8_t[BUFFER_SIZE];
		memset(pChar, 1, BUFFER_SIZE);
		BOOL bResult = tub.write(pChar, BUFFER_SIZE);

		std::cout << bResult << std::endl;
	}
	tub.close();
    std::cout << "Hello World!\n";
	return 0;
}

int main(int argc, char** argv)
{
	int iUsbDeviceOpenID = 0;
	char* pCharUSBFile = NULL;
	int iNum = 0;
	int localBuffSize = 1;
	unsigned int iCount = 0;
	//const char* charJeson = "[1,\"UserService:login\",1,0,{\"1\":{\"rec\":{\"1\":{\"rec\":{\"1\":{\"str\":\"root\"},\"2\":{\"str\":\"2f07112d83547b976d283507c48b840d\"},\"3\":{\"str\":\"\"}}}}}}]";
	char* pCharModel = NULL;
	GUID pGUID = { 0 };
	UuidCreate(&pGUID);
	//CONVERT_STR_2_GUID(TEXT("{28d78fad-5a12-11d1-ae5b-0000f803a8c2}"), pGUID);

	TCHAR strUUID[] = TEXT("28d78fad-5a12-11d1-ae5b-0000f803a8c2");
	RPC_STATUS result = UuidFromString((RPC_WSTR)strUUID, &pGUID);
	//RpcStringFree();
	if (argc == 5)
	{
		pCharUSBFile = *(argv + 1);
		pCharModel = *(argv + 2);
		iNum = atoi(*(argv + 3));
		localBuffSize = atoi(*(argv + 4));
	}
	else
	{
		printf("The paramenter is error.\r\n");
		return -1;
	}

	TUsbDevice tub(0x3308, 0x1331, &pGUID, false, false, true, true, localBuffSize, localBuffSize);

	if (strcmp(pCharModel, ("r")) == 0)
	{
		int startThread = 1;
		uint8_t* readBuffer = (uint8_t*)malloc(localBuffSize + 1);
		memset(readBuffer, 0, localBuffSize + 1);
		iUsbDeviceOpenID = tub.open();

		int ret = 0;
		//ret = pthread_create(&id,NULL, closeThread, &iUsbDeviceOpenID); // ³É¹¦·µ»Ø0£¬´íÎó·µ»Ø´íÎó±àºÅ
		if (ret != 0)
		{
			printf("To create thread fault.");
		}

		if (iUsbDeviceOpenID < 0)
		{
			printf("Used O_RDONLY open fault.");
			free(readBuffer);
			return -1;
		}
		while (1)
		{
			/*if(startThread)
			{
				pthread_join(id, NULL);
				startThread = 0;
			}*/
			memset(readBuffer, 0, localBuffSize + 1);
			long len = tub.read(readBuffer, localBuffSize);
			printf("read return : %ld\n", len);
			//printf("error code : %d, error text: %s\n", errno, strerror(errno));
			if (len > 0) {
				++iCount;
				printf("string : %s\n", readBuffer);
				printf("mount of number : %d\n", iCount * localBuffSize);
				continue;
			}
			break;
		}
		printf("read packet %d\r\n", iCount);
		tub.close();
		free(readBuffer);
	}
	if (strcmp(pCharModel, ("w")) == 0)
	{
		/*-----------------------------------------*/
		//char* pbuff = 0;
		//ssize_t len = fileToBuff("/home/yang/projects/usbtest/UsbTester/UsbTester.pro.user", &pbuff);
		/*-----------------------------------------*/
		int i;
		uint8_t* writeBuffer = (uint8_t*)malloc(localBuffSize);
		memset(writeBuffer, '1', localBuffSize);

		iUsbDeviceOpenID = tub.open();
		if (iUsbDeviceOpenID < 0)
		{
			printf("Used O_WRONLY open fault.");
			free(writeBuffer);
			return -1;
		}

		for (i = 0; i < iNum; i++)
		{
			long writeNum = tub.write(writeBuffer, localBuffSize);
			//ssize_t writeNum = write(iUsbDeviceOpenID, pbuff, (size_t)len);
			if (writeNum != localBuffSize) break;
			++iCount;
		}
		printf("write packet %d\r\n", iCount);
		free(writeBuffer);
		tub.close();
	}
	return 0;
}
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
