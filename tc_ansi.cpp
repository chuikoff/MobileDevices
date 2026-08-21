// ANSI ABI shims for Total Commander.
// TC still GetProcAddress's the non-W names; missing exports crash the host.
// All file/device logic lives in the *W implementations.
#include <windows.h>
#include "fsplugin.h"
#include "cunicode.h"

extern int PluginNumber;
extern tProgressProc ProgressProc;
extern tLogProc LogProc;
extern tRequestProc RequestProc;

int __stdcall FsInit(int PluginNr,tProgressProc pProgressProc,tLogProc pLogProc,tRequestProc pRequestProc)
{
	PluginNumber=PluginNr;
	ProgressProc=pProgressProc;
	LogProc=pLogProc;
	RequestProc=pRequestProc;
	return 0;
}

HANDLE __stdcall FsFindFirst(char* Path,WIN32_FIND_DATA *FindData)
{
	WCHAR PathW[wdirtypemax];
	WIN32_FIND_DATAW FindDataW={0};
	HANDLE h=FsFindFirstW(awfilenamecopy(PathW,Path),&FindDataW);
	if (h!=INVALID_HANDLE_VALUE)
		copyfinddatawa(FindData,&FindDataW);
	return h;
}

BOOL __stdcall FsFindNext(HANDLE Hdl,WIN32_FIND_DATA *FindData)
{
	WIN32_FIND_DATAW FindDataW={0};
	if (!FsFindNextW(Hdl,&FindDataW))
		return FALSE;
	copyfinddatawa(FindData,&FindDataW);
	return TRUE;
}

BOOL __stdcall FsDisconnect(char* DisconnectRoot)
{
	WCHAR w[wdirtypemax];
	return FsDisconnectW(awfilenamecopy(w,DisconnectRoot?DisconnectRoot:""));
}

BOOL __stdcall FsMkDir(char* Path)
{
	WCHAR PathW[wdirtypemax];
	return FsMkDirW(awfilenamecopy(PathW,Path));
}

BOOL __stdcall FsDeleteFile(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsDeleteFileW(awfilenamecopy(RemoteNameW,RemoteName));
}

BOOL __stdcall FsRemoveDir(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsRemoveDirW(awfilenamecopy(RemoteNameW,RemoteName));
}

int __stdcall FsRenMovFile(char* OldName,char* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	WCHAR OldNameW[wdirtypemax],NewNameW[wdirtypemax];
	return FsRenMovFileW(awfilenamecopy(OldNameW,OldName),awfilenamecopy(NewNameW,NewName),Move,OverWrite,ri);
}

int __stdcall FsGetFile(char* RemoteName,char* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
	WCHAR RemoteNameW[wdirtypemax],LocalNameW[wdirtypemax];
	return FsGetFileW(awfilenamecopy(RemoteNameW,RemoteName),awfilenamecopy(LocalNameW,LocalName),CopyFlags,ri);
}

int __stdcall FsPutFile(char* LocalName,char* RemoteName,int CopyFlags)
{
	WCHAR LocalNameW[wdirtypemax],RemoteNameW[wdirtypemax];
	return FsPutFileW(awfilenamecopy(LocalNameW,LocalName),awfilenamecopy(RemoteNameW,RemoteName),CopyFlags);
}

int __stdcall FsExecuteFile(HWND MainWin,char* RemoteName,char* Verb)
{
	WCHAR RemoteNameW[wdirtypemax],VerbW[wdirtypemax];
	return FsExecuteFileW(MainWin,awfilenamecopy(RemoteNameW,RemoteName),awfilenamecopy(VerbW,Verb));
}

void __stdcall FsStatusInfo(char* RemoteDir,int InfoStartEnd,int InfoOperation)
{
	WCHAR dirW[wdirtypemax];
	FsStatusInfoW(awfilenamecopy(dirW,RemoteDir),InfoStartEnd,InfoOperation);
}

int __stdcall FsContentGetValue(char* FileName,int FieldIndex,int UnitIndex,void* FieldValue,int maxlen,int flags)
{
	WCHAR nameW[wdirtypemax];
	int r=FsContentGetValueW(awfilenamecopy(nameW,FileName),FieldIndex,UnitIndex,FieldValue,maxlen,flags);
	if (r==ft_stringw) {
		WCHAR tmp[1024];
		wcslcpy(tmp,(WCHAR*)FieldValue,1024);
		walcopy((char*)FieldValue,tmp,maxlen>0?maxlen-1:0);
		return ft_string;
	}
	return r;
}

void __stdcall FsContentStopGetValue(char* FileName)
{
	WCHAR nameW[wdirtypemax];
	FsContentStopGetValueW(awfilenamecopy(nameW,FileName));
}

BOOL __stdcall FsContentGetDefaultView(char* ViewContents,char* ViewHeaders,char* ViewWidths,char* ViewOptions,int maxlen)
{
	UNREFERENCED_PARAMETER(ViewContents);
	UNREFERENCED_PARAMETER(ViewHeaders);
	UNREFERENCED_PARAMETER(ViewWidths);
	UNREFERENCED_PARAMETER(ViewOptions);
	UNREFERENCED_PARAMETER(maxlen);
	return FsContentGetDefaultViewW(NULL,NULL,NULL,NULL,maxlen);
}

int __stdcall FsGetPreviewBitmap(char* RemoteName,int width,int height,HBITMAP* ReturnedBitmap)
{
	WCHAR nameW[wdirtypemax];
	return FsGetPreviewBitmapW(awfilenamecopy(nameW,RemoteName),width,height,ReturnedBitmap);
}

int __stdcall FsExtractCustomIcon(char* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	WCHAR nameW[wdirtypemax];
	return FsExtractCustomIconW(awfilenamecopy(nameW,RemoteName),ExtractFlags,TheIcon);
}
