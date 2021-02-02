#pragma warning(disable:4244)
#pragma warning(disable:4996 4005)
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#ifdef UNICODE
# undef UNICODE
# define _MBCS
#endif
#if defined(_MSVC_LANG) && (_MSVC_LANG>=201703L)
# define _HAS_STD_BYTE 0
# define _HAS_STD_BOOLEAN 0
#endif

#include "TaskExecutor.h"
#include "TimeUtils.h"
#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <tchar.h>
#include <strsafe.h>
using namespace std;

#define MAX_THREADS 3
#define BUF_SIZE 255

DWORD WINAPI MyThreadFunction(LPVOID lpParam);

typedef struct threadData
{
	string srcDir;
	string destDir;
	bool compress = false;
}MYDATA, * PMYDATA;

void TaskExecutor::execute(std::vector<BackupProperties> tasks)
{
	PMYDATA pDataArray[MAX_THREADS];
	DWORD   dwThreadIdArray[MAX_THREADS];
	HANDLE  hThreadArray[MAX_THREADS];

	for (int i = 0; i < MAX_THREADS; i++)
	{
		pDataArray[i] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MYDATA));

		if (pDataArray[i] == NULL)
		{
			ExitProcess(2);
		}

		for (BackupProperties t : tasks)
		{
			pDataArray[i]->srcDir = t.srcDir;
			pDataArray[i]->destDir = t.destDir;
			pDataArray[i]->compress = t.compress;
		}

		hThreadArray[i] = CreateThread(
			NULL,                  
			0,                      
			MyThreadFunction,      
			pDataArray[i],        
			0,                      
			&dwThreadIdArray[i]);  

		if (hThreadArray[i] == NULL)
		{
			ErrorHandler((LPTSTR)TEXT("error"));
			ExitProcess(3);
		}
	}

	WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);

	for (int i = 0; i < MAX_THREADS; i++)
	{
		CloseHandle(hThreadArray[i]);
		if (pDataArray[i] != NULL)
		{
			HeapFree(GetProcessHeap(), 0, pDataArray[i]);
			pDataArray[i] = NULL;  
		}
	}
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	HANDLE hStdout;
	PMYDATA pDataArray;

	TCHAR msgBuf[BUF_SIZE];
	size_t cchStringSize;
	DWORD dwChars;

	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout == INVALID_HANDLE_VALUE)
		return 1;

	pDataArray = (PMYDATA)lpParam;


	//Nie wiadomo czemu to sie odpala co 5 sekund
	StringCchPrintf(msgBuf, BUF_SIZE, TEXT("Tworze backup, kompresja: %d\n"), pDataArray->compress);
	StringCchLength(msgBuf, BUF_SIZE, &cchStringSize);
	WriteConsole(hStdout, msgBuf, (DWORD)cchStringSize, &dwChars, NULL);

	TaskExecutor::DoBackup(pDataArray->srcDir, pDataArray->destDir, pDataArray->compress);

	return 0;
}

void TaskExecutor::DoBackup(string source, string destination, bool compress)
{
	WIN32_FIND_DATAA findFileData;
	HANDLE findHandle = FindFirstFile(source.c_str(), &findFileData);
	if (findHandle != INVALID_HANDLE_VALUE)
	{
		bool success = false;
		if (findFileData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
		{
			FindClose(findHandle);
			if (source.back() != '\\')
				source += "\\";
			source += "*.*";
			source.append(1, '\0');
			if (destination.back() != '\\')
				destination += "\\";
			destination.append(1, '\0');
			std::cout << source << " " << destination << endl;
			SHFILEOPSTRUCT shFileOperationStructure = { 0 };
			shFileOperationStructure.wFunc = FO_COPY;
			shFileOperationStructure.fFlags = FOF_SILENT;
			shFileOperationStructure.pFrom = source.c_str();
			shFileOperationStructure.pTo = destination.c_str();
			shFileOperationStructure.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_SILENT;
			SHFileOperation(&shFileOperationStructure);
		}
		else
		{
			const int bufferSize = 256;
			HANDLE inputHandle, outputHandle;
			DWORD inputBytes, outputBytes;
			CHAR buffer[bufferSize];
			destination.append(findFileData.cFileName);
			FindClose(findHandle);
			inputHandle = CreateFile(source.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			outputHandle = CreateFile(destination.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (inputHandle != INVALID_HANDLE_VALUE && outputHandle != INVALID_HANDLE_VALUE)
				success = true;
			if (success == true)
				while (ReadFile(inputHandle, buffer, bufferSize, &inputBytes, NULL) && inputBytes > 0)
					WriteFile(outputHandle, buffer, inputBytes, &outputBytes, NULL);
			CloseHandle(outputHandle);
			CloseHandle(inputHandle);
		}


	}
}
void TaskExecutor::ErrorHandler(LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code.

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message.

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	// Free error-handling buffer allocations.

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}


