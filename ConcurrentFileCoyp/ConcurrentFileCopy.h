#pragma once

#include <windows.h>
#include <commctrl.h>
#include <memory.h>
#include <tchar.h>
#include <math.h>


#pragma comment (lib ,"comctl32.lib")
#pragma comment (linker,"\"/manifestdependency:type = 'win32'\
name = 'Microsoft.Windows.Common-Controls'\
version = '6.0.0.0' processorArchitecture = '*'\
publicKeyToken = '6595b64144ccf1df' language = '*'\"")

HWND    InitInstance(HINSTANCE hInstance, int nCmdshow,HWND& hWndPB);
ATOM    RegisterWndClass(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg, WPARAM wParam,LPARAM lParam);
DWORD   WINAPI ReadRout(LPVOID lpPara);
DWORD   WINAPI WriteRout(LPVOID lpPara);
BOOL    FileDlg(HWND hWnd,LPTSTR szName,DWORD dwFileoper);
DWORD   GetBlockSize(DWORD dwSize);

#define BUTTON_CLOSE         100
#define FILE_SAVE            0X0001
#define FILE_OPEN            0X0002
#define MUTEX_NAME           _T("_RW_MUTEX_")

typedef struct _tagCopy //作为读写进程的参数传入
{
	HINSTANCE hInstance;
	HWND  hWndPB;
	LPTSTR szRName;
	LPTSTR szWName;
} COPYDETAL, *PCOPYDETAL;