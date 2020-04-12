
#include "ConcurrentFileCopy.h"


TCHAR* szTitle = _T("Concurrent File Copy");
TCHAR* szWndClass = _T("_cfc_wnd_");

DWORD dwReadByte = 0;  //已经读取的字节数
DWORD dwWriteByte = 0; //已经写入的字节数
DWORD dwBlockSize = 0; //单次读写默认字节数（只有最后一次可能不一样）
DWORD dwFileSize = 0;  //文件大小

HLOCAL pMemory = NULL;  //读写的共享空间资源，读写进程互斥的使用此区域

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPre,LPTSTR szCmdline,int nCmdshow)
{
	UNREFERENCED_PARAMETER(hPre);
	UNREFERENCED_PARAMETER(szCmdline);

	RegisterWndClass(hInstance); //注册窗口类

	HWND hWnd = NULL;
	HWND hWndPB = NULL;
	if(!(hWnd = InitInstance(hInstance,nCmdshow,hWndPB)))
	{
		return 1;
	}
	MSG msg = {0};
	TCHAR szReadFile[MAX_PATH];
	TCHAR szWriteFile[MAX_PATH];
	
	if(FileDlg(hWnd,szReadFile,FILE_OPEN)
	   && FileDlg(hWnd,szWriteFile,FILE_SAVE))
	{
		COPYDETAL copyDetails = {hInstance,hWndPB,szReadFile,szWriteFile};
		HANDLE hMutex = CreateMutex(NULL,FALSE,MUTEX_NAME);
		HANDLE hReadThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ReadRout,&copyDetails,0,NULL);//先创建读取线程

		while(GetMessage(&msg,NULL,0,0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		CloseHandle(hReadThread);
		CloseHandle(hMutex);
	}
	else
	{
		MessageBox(hWnd,_T("can't open file"),_T("error"),MB_OK);
	}
	LocalFree(pMemory);
	UnregisterClass(szWndClass,hInstance);
	
	return (int)msg.wParam;
}

ATOM RegisterWndClass(HINSTANCE hInstance)
{
	WNDCLASSEX wndEX;

	wndEX.cbSize = sizeof(WNDCLASSEX );
	wndEX.style = CS_HREDRAW | CS_VREDRAW;
	wndEX.lpfnWndProc = WndProc;
	wndEX.cbClsExtra = 0;
	wndEX.cbWndExtra = 0;
	wndEX.hInstance = hInstance;
	wndEX.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPLICATION));
	wndEX.hCursor = LoadCursor(NULL,IDC_ARROW);
	wndEX.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wndEX.lpszMenuName = NULL;
	wndEX.lpszClassName = szWndClass;
	wndEX.hIconSm = LoadIcon(wndEX.hInstance,MAKEINTRESOURCE(IDI_APPLICATION));

	return RegisterClassEx(&wndEX);
}

HWND InitInstance(HINSTANCE hInstance, int nCmdshow,HWND& hWndPB)
{
	HWND hWnd = CreateWindow(szWndClass,szTitle,WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |WS_MINIMIZEBOX,
		200,200,440,290,NULL,NULL,hInstance,NULL);
	
	if(!hWnd)
	{
		DWORD dwError = GetLastError();
		return NULL;
	}
	RECT rcClient = {0};
	int cyVscroll = 0;
	HFONT hFont = CreateFont(14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,BALTIC_CHARSET,OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_MODERN,_T("Microsoft Sans Serif"));

	HWND hButton = CreateWindow(_T("BUTTON"),_T("CLOSE"),WS_CHILD| WS_VISIBLE| BS_PUSHBUTTON|WS_TABSTOP,
		310,200,100,25,hWnd,(HMENU)BUTTON_CLOSE,hInstance,NULL);
	SendMessage(hButton,WM_SETFONT,(WPARAM)hFont,TRUE);

	GetClientRect(hWnd,&rcClient);
	cyVscroll = GetSystemMetrics(SM_CYVSCROLL);

	hWndPB = CreateWindow(PROGRESS_CLASS,NULL,WS_CHILD| WS_VISIBLE,rcClient.left,rcClient.bottom-cyVscroll,
		rcClient.right,cyVscroll,hWnd,0,hInstance,NULL);
	SendMessage(hWndPB,PBM_SETSTEP,(WPARAM)1,0);

	ShowWindow(hWnd,nCmdshow);
	UpdateWindow(hWnd);

	return hWnd;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case BUTTON_CLOSE:
				{
					DestroyWindow(hWnd);
					break;
				}
			default:
				break;
			}
		}
	case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
	default:
		{
			return DefWindowProc( hWnd, uMsg, wParam,lParam);
		}
	}
	return 0;
}

DWORD WINAPI ReadRout(LPVOID lpPara)
{
	PCOPYDETAL pCopyDetails = (PCOPYDETAL)lpPara;
	HANDLE hFile = CreateFile(pCopyDetails->szRName,GENERIC_READ,FILE_SHARE_READ,
		NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);  //获取已有文件句柄

	if (hFile == (HANDLE)INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	dwFileSize = GetFileSize(hFile,NULL);			//文件大小
	dwBlockSize = GetBlockSize(dwFileSize);			//单次读写分块大小
	
	HANDLE hWriteThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)WriteRout,
		pCopyDetails,0,NULL);						//创建写线程  存疑：为什么写线程在此处创建？
	size_t uBufferLength = (size_t)ceil((double)dwFileSize/(double)dwBlockSize);  //计算读写次数
	SendMessage(pCopyDetails->hWndPB,PBM_SETRANGE,0,MAKELPARAM(0,uBufferLength)); //设置进度条范围
	pMemory = LocalAlloc(LPTR,dwFileSize);			//创建共享资源--空间，原来想改成LocalAlloc(LPTR,dwBlockSize)，但是会失去多线程意义，退化为单线程
	void* pBuffer = LocalAlloc(LPTR,dwBlockSize);	//单次读缓存

	int nOffset = 0;
	DWORD dwByteRed = 0;	//单次实际读入字符计数
	do
	{
		ReadFile(hFile,pBuffer,dwBlockSize,&dwByteRed,NULL);
		if(!dwByteRed)		//实际读入为0，即读完了文件
		{
			break;
		}
		HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,MUTEX_NAME);
		WaitForSingleObject(hMutex, INFINITE);
		// 内存拷贝函数，从源source向目标dest拷贝num个字节内容，void* memcpy(void* destination, const void* source, size_t num);
		memcpy((char*)pMemory + nOffset,pBuffer,dwByteRed);
		dwReadByte += dwByteRed;
		ReleaseMutex(hMutex);
		nOffset += (int)dwBlockSize;
	}while(true);

	LocalFree(pBuffer);
	CloseHandle(hFile);
	CloseHandle(hWriteThread);
	return 0;
}

DWORD WINAPI WriteRout(LPVOID lpPara)
{
	PCOPYDETAL pCopyDetail = (PCOPYDETAL)lpPara;
	HANDLE hFile = CreateFile(pCopyDetail->szWName,GENERIC_WRITE,0,
		NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);

	if (hFile == (HANDLE)INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	DWORD dwByteWrite = 0;
	int nOffset = 0;

	do
	{
		int nRemainByte = (int)dwFileSize - nOffset;  //剩余字节数
		if(nRemainByte  <= 0)
		{
			break;
		}
		Sleep(10);
		if(dwWriteByte < dwReadByte)  //感觉这个条件使得互斥锁意义大大降低
		{
			DWORD dwByteToWrite = dwBlockSize;
			if(!(dwFileSize/dwBlockSize))
			{
				dwByteToWrite = (DWORD)nRemainByte;
			}
			
		
			HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE,MUTEX_NAME);
			WaitForSingleObject(hMutex,INFINITE);

			WriteFile(hFile,(char*)pMemory + nOffset,dwByteToWrite,&dwByteWrite,NULL);
			dwWriteByte += dwByteWrite;

			ReleaseMutex(hMutex);
			SendMessage(pCopyDetail->hWndPB,PBM_SETSTEP,1,0);
			SendMessage(pCopyDetail->hWndPB,PBM_STEPIT,0,0);          //源码进设置了步长，忘了更新
			nOffset += (int)dwBlockSize;
		}
	}while(true);
	CloseHandle(hFile);
	return 0;
}

BOOL FileDlg(HWND hWnd,LPTSTR szName,DWORD dwFileoper)
{
#ifdef _UNICODE
	OPENFILENAME ofn;
#else
	OPENFILENAME ofn;
#endif
	TCHAR szFile[MAX_PATH];

	ZeroMemory(&ofn,sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szFile;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = _T("ALL\0*.*\0Text\0*.TXT\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = dwFileoper == FILE_OPEN ? OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST 
		:OFN_SHOWHELP|OFN_OVERWRITEPROMPT;

	if(dwFileoper == FILE_OPEN)
	{
		if(GetOpenFileName(&ofn) == TRUE)
		{
			_tcscpy_s(szName,MAX_PATH - 1,szFile);
			return TRUE;
		}
	}
	else
	{
		if(GetSaveFileName(&ofn) == TRUE)
		{
			_tcscpy_s(szName,MAX_PATH - 1,szFile);
			return TRUE;
		}
	}
	return FALSE;
}

DWORD GetBlockSize(DWORD dwSize)
{
	return dwFileSize > 4096 ? 4096 : 512;
}






