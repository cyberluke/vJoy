// vJoyInterface.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crtdbg.h>

extern "C"
{
#include <setupapi.h>
#include <Hidsdi.h>
}

#include <Dbt.h>
#include <tchar.h>
#include "vJoyInterface.h"

#define OLD_PREPARSED 0

#pragma warning( disable : 4706)

#pragma comment(lib,"Hid.lib")
#pragma comment (lib, "Setupapi.lib")

// Structure that holds initial values of device controls
struct DEVICE_INIT_VALS {
    UCHAR cb;				// Size in bytes of this structure
    USHORT id;				// Device ID
    UCHAR InitValAxis[VJOY_NUMBER_OF_AXES];	// Initial Value of axes (X, Y, Z, RX, RY, RZ, SL1, SL2)  in %
    UCHAR InitValPov[VJOY_NUMBER_OF_HAT];	// Initial Value of POVs in % (0xFF means neutral point)
    UCHAR ButtonMask[VJOY_NUMBER_OF_BUTTONS/8];	// Each bit represents a button
};


typedef std::map<int, DeviceStat> vJoyDeviceMap;

// Prototypes (Helper Functions)
bool	ExtractNamespace(const char* SrcDevicePath, char* DestDevicePath, int* DestSize);
bool	CreateDevicePath(const char* SrcDevicePath, int Index, char* DestDevicePath, int DestSize);
bool	isRawDevice(const char* DevicePath, int Index);
HANDLE	OpenDeviceInterface(UINT iInterFace, DWORD* error);
HANDLE	GetGenControlHadle(void);
bool	GetDevStat(BYTE id, int* nbytes, BYTE* buffer);
bool	GetDrvStat(int* nbytes, BYTE* buffer);
BOOL	GetDeviceNameSpace(char** NameSpace, int* Size, BOOL Refresh, DWORD* error);
int		GetDeviceIndexById(USHORT VendorId, USHORT ProductId, int BaseIndex);
int		GetDeviceIndexByReportId(USHORT VendorId, USHORT ProductId, BYTE ReportId);
BOOL	GetDeviceVersionNumber(int Index, PUSHORT version);
BOOL	GetDeviceProductString(int Index, PWSTR* pProductString);
BOOL	GetDeviceManufacturerString(int Index, PWSTR* pManufacturerString);
BOOL	GetDeviceSerialNumberString(int Index, PWSTR* pSerialNumberString);
BOOL	GetDeviceAttributes(int Index, PUSHORT VendorID, PUSHORT ProductID, PUSHORT version);
int		GetvJoyReportId(int Index);
HANDLE	GetHandleByIndex(int index);
int		WINAPI CreateDummyWindow(void);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void	InitDll(void);
HDEVNOTIFY RegistervJoyNotification(HWND hWin);
HDEVNOTIFY RegisterHandleNotification(HWND hWin, HANDLE hDev);
BOOL	InitPosition(int Index);
void	CalcInitValue(USHORT id, DEVICE_INIT_VALS* data_buf);
UINT	GetInitValueFromRegistry(USHORT id, DEVICE_INIT_VALS* data_buf);
void	SavePosition(UINT rID, PVOID pData);
BOOL	GetDevPosition(BYTE id, PJOYSTICK_POSITION pPosition);
BOOL	Update(UINT rID);
BOOL	GetAxisCaps(UINT rID, UINT Axis, HIDP_VALUE_CAPS* ValCaps);
LONG	GetAxisLogMin(UINT rID, UINT Axis);
LONG	GetAxisLogMax(UINT rID, UINT Axis);
LONG	GetAxisLogMid(UINT rID, UINT Axis);
void	StartLogging(void);
bool	GetDevInfo(HANDLE h, PVOID data);
int		DbgGetCaps(void);
INT		GetControls(UINT rID);
BOOL	AreControlsInit(UINT rID);

BOOL vJoyDeviceEntry(int rID);
BOOL vJoyDeviceRemove(int rID);
void vJoyDeviceClear(void);
BOOL  Set_PreparsedData(int rID);
BOOL Get_PreparsedData(int rID, PHIDP_PREPARSED_DATA* pPPData);
void Set_h(int rID, HANDLE h);
void Sync_Position(int rID);
HANDLE 	Get_h(int rID);
void Set_stat(int rID, VjdStat status);
VjdStat  Get_stat(int rID);
void Set_hNotify(int rID, HDEVNOTIFY h);
HDEVNOTIFY 	Get_hNotify(int rID);


#if 0
bool	_FfbStart(HANDLE h);
bool	_FfbStop(HANDLE h);
#endif

#if 0
bool	FfbIsStarted(HANDLE h);
BOOL	FfbGetEffectState(void);
FFBEType FfbGetEffectType(void);

#endif // 0

/******* Global variables *******/
BOOL Init = FALSE; // DLL Initialized
vJoyDeviceMap vJoyDevices;
HANDLE h0 = INVALID_HANDLE_VALUE; // Handle to device 0

//HANDLE h[17] = { // Array of handles to VJDs (h[0] is for general control)
//	INVALID_HANDLE_VALUE,
//	INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
//	INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
//	INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
//	INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE
//};

//enum VjdStat stat[17] = { // Array of VJD statuse (stat[0] not used)
//	VJD_STAT_UNKN,
//	VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN,
//	VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN,
//	VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN,
//	VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN, VJD_STAT_UNKN
//};

// JOYSTICK_POSITION_V2 position[17] = {0}; // Array of saved positions for VJDs (position[0] not used)

// The main window class name.
static TCHAR szWindowClass[] = _T("win32app_vJoyInterface_DLL");
// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("This should be seen only for debug");
HINSTANCE hInst;
HWND hWnd = NULL;
HDEVNOTIFY hDeviceNotifyInterFace;
HDEVNOTIFY hDeviceNotifyHandle0 = NULL;
//HDEVNOTIFY hDeviceNotifyHandle[17] = { NULL };
HANDLE hEvent;
PVOID ppDisable = NULL;
PVOID ppEnable = NULL;
LPTSTR LogFileName = NULL;
INT Logging = 0;
FILE* LogStream = NULL;
DWORD ProcessId = 0;

// Removal CB
RemovalCB	RemovalFunc = NULL;
PVOID		RemovalData = NULL;

/// FFB ///
std::map<int, FFB_EFFECTS> mFfbEffect;
FFB_DATA	FfbDataPacket;
FfbGenCB	FfbGebFunc = NULL;
PVOID		FfbUserData = NULL;

#ifdef PRINT
FILE* fOut;
#endif
BOOL FfbEffectState = FALSE;
FFBEType FfbEffectType = ET_NONE;
HANDLE hFfbEvent;

#ifdef STATIC
#define SET_NS(x) vJoyNS::x
#else
#define SET_NS(x) x
#endif

void __cdecl FfbFunction(PVOID data)
{
    _tprintf("\nIn vJoy interface!!!");
    FFB_DATA* FfbData = (FFB_DATA*)data;
    int size = FfbData->size;
    _tprintf("\nFFB Size %d\n", size);

    _tprintf("Cmd:%08.8X ", FfbData->cmd);
    _tprintf("ID:%02.2X ", FfbData->data[0]);
    _tprintf("Size:%02.2d ", static_cast<int>(FfbData->size - 8));
    _tprintf(" - ");
    for (UINT i = 0; i < FfbData->size - 8; i++)
        _tprintf(" %02.2X", (UINT)FfbData->data[i]);
    _tprintf("\n");
}


extern "C" {
    void	DeviceChange(WPARAM wParam, LPARAM lParam);
    bool	FfbStartThread(HANDLE h);
    void	FfbProcessData(WPARAM wParam, LPARAM lParam);
    int		WINAPI FfbWaitForData(HANDLE*);
    void	FfbSendData(UCHAR* FfbData, DWORD	nBytesTranss);

    void	DeviceChange(WPARAM wParam, LPARAM lParam)
        /*
        This function is called from WndProc when message WM_DEVICECHANGE is received
        */
    {
        //UNREFERENCED_PARAMETER(lParam);
        static int cnt_complete, cnt_arrive;
        static DEV_BROADCAST_HDR hdr_complete, hdr_query, hdr_arrive;

        if (wParam == DBT_DEVICEREMOVECOMPLETE || wParam == DBT_DEVICEQUERYREMOVE || wParam == DBT_DEVICEREMOVEPENDING) {
            // Callback removal functions
            if (wParam == DBT_DEVICEREMOVECOMPLETE && RemovalFunc) {
                cnt_complete++;
                if (cnt_complete == 1)
                    RemovalFunc(TRUE, TRUE, RemovalData);
                else if (cnt_complete == VJOY_MAX_N_DEVICES) {
                    RemovalFunc(TRUE, FALSE, RemovalData);
                    cnt_complete = 0;
                }

            }

            // Request to disable/remove Raw PDO
            // First, it sends a message that the open handle should be released
            // When the handle to the device is released,
            // it sends a message for each of the 16 interfaces that are released
            // Unregister handle notification handles, finally close devices
            for (int i = 1; i<17; i++) {
                UnregisterDeviceNotification(Get_hNotify(i));
                Set_hNotify(i, NULL);
                SET_NS(RelinquishVJD(i));
                vJoyDeviceRemove(i);
            }

            // Releases the handle to the control file
            UnregisterDeviceNotification(hDeviceNotifyHandle0);
            hDeviceNotifyHandle0 = NULL;
            CloseHandle(h0);
            h0 = INVALID_HANDLE_VALUE;

            vJoyDeviceClear();
            mFfbEffect.clear();
            return;
        }

        if (wParam == DBT_DEVICEARRIVAL) {
            cnt_arrive++;
            if (cnt_arrive == 1 && RemovalFunc)
                RemovalFunc(FALSE, TRUE, RemovalData);
            else if (cnt_arrive == VJOY_MAX_N_DEVICES  && RemovalFunc) {
                RemovalFunc(FALSE, FALSE, RemovalData);
                cnt_arrive = 0;
            }
            return;
            // Request to enable/install Raw PDO
            // It sends a message for each of the 16 interfaces that are enabled
            // This is how we know that it has been installed
        }

    }
    ///// Force Feedback related functions //////////////////////////////////////////

    bool  FfbStartThread(HANDLE h)
    {

        // Start a thread that waits for FFB data
        DWORD dwFfbThreadId;
        hFfbEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!hFfbEvent)
            return false;

        HANDLE hFfbThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&FfbWaitForData, h, 0, &dwFfbThreadId);

        // Wait for the FFB thread to be created
        DWORD waitRes = WaitForSingleObject(hFfbEvent, 10000);
        if (waitRes != WAIT_OBJECT_0) {
            MessageBox(NULL, _T("Creation of Ffb data-loop failed!"), _T("vJoyInterface DLL"), NULL);
            return false;
        }


        if (dwFfbThreadId)
            return true;
        else
            return false;
    }




    bool	FfbIsStarted(HANDLE h)
    {
        ULONG	bytes = 0;
        BYTE	Stat = 0;
        HANDLE	hIoctlEvent;
        OVERLAPPED OverLapped = { 0 };

        // Preparing
        hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
        memset(&OverLapped, 0, sizeof(OVERLAPPED));
        OverLapped.hEvent = hIoctlEvent;
        OverLapped.Internal = STATUS_PENDING;

        // Test  FFB queues in the device
        BOOL ok = DeviceIoControl(h, GET_FFB_STAT, NULL, NULL, &Stat, 1, &bytes, &OverLapped);

        // Imedeate Return
        if (ok) {
            CloseHandle(OverLapped.hEvent);
            if (bytes && Stat) {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbIsStarted() - Returns (Immediatly) TRUE"), ProcessId);
                return true;
            } else {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbIsStarted() - Returns (Immediatly) FALSE (bytes=%u stat=%u)"), ProcessId, bytes, Stat);
                return false;
            }
        }

        // Pending or Error
        else {
            // Error
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbIsStarted() - error (0x%X) Returns FALSE"), ProcessId, err);
                CloseHandle(OverLapped.hEvent);
                return false;
            }

            // Wait until data ready
            DWORD nBytesTranss = 0;
            BOOL gotdata = GetOverlappedResult(h, &OverLapped, &nBytesTranss, TRUE);
            CloseHandle(OverLapped.hEvent);
            if (gotdata && nBytesTranss) {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbIsStarted() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
                return true;
            } else {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbIsStarted() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
                return false;
            }
        }
    }

    int	  WINAPI FfbWaitForData(HANDLE* h)
    {
        // Loop on read FFB data from vJoy

#define BUFFERSIZE 256 // TODO: Change to correct number
        UINT	IoCode = GET_FFB_DATA;
        UINT	IoSize = BUFFERSIZE;
        ULONG	bytes;
        UCHAR	FfbData[BUFFERSIZE];
        BOOL	gotdata;
        DWORD	nBytesTranss = 1;
        HANDLE hIoctlEvent;
        OVERLAPPED FfbOverlapped = { 0 };

        // Signal the parent thread that this thread was created
        SetEvent(hFfbEvent);


        // Send joystick position structure to vJoy device
        hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

        // Prepare container for incoming data
        FfbDataPacket.size = 0;
        FfbDataPacket.cmd = 0;
        FfbDataPacket.data = new UCHAR[BUFFERSIZE];

        BOOL res;
        DWORD err;

        do {
            // This is an async (overlapped) transaction
            memset(&FfbOverlapped, 0, sizeof(OVERLAPPED));
            FfbOverlapped.hEvent = hIoctlEvent;
            res = DeviceIoControl(h, IoCode, NULL, 0, &FfbData, IoSize, &bytes, &FfbOverlapped);
            /*
                        _tprintf("In vJoy interface!!!\n");
                        FFB_DATA* FfbData2 = (FFB_DATA*)&FfbData;
                        int size = FfbData2->size;
                        _tprintf("FFB Size %d\n", size);

                        _tprintf("Cmd:%08.8X \n", FfbData2->cmd);
            */
            // Imedeate Return
            if (res) {
                //CloseHandle(FfbOverlapped.hEvent);
                if (bytes) {
                    if (LogStream)
                        _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbWaitForData() - Returns (Immediatly) TRUE"), ProcessId);
                    return true;
                } else {
                    if (LogStream)
                        _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbWaitForData() - Returns (Immediatly) FALSE"), ProcessId);
                    return false;
                }
            }

            // Delayed/Error
            else {
                // Error getting the data
                err = GetLastError();
                if (err != ERROR_IO_PENDING) {
                    if (LogStream)
                        _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbWaitForData() - error (0x%X) Returns FALSE"), ProcessId, err);
                    //CloseHandle(FfbOverlapped.hEvent);
                    return false;
                }

                // Wait until data ready
                nBytesTranss = 0;
                gotdata = GetOverlappedResult(h, &FfbOverlapped, &nBytesTranss, TRUE);
                //CloseHandle(FfbOverlapped.hEvent);
                if (gotdata && nBytesTranss && nBytesTranss <= BUFFERSIZE && nBytesTranss>0) {
                    if (LogStream)
                        _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbWaitForData() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
                    FfbSendData(FfbData, nBytesTranss);
                } else {
                    if (LogStream)
                        _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbWaitForData() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
                    return false;
                }
            }
        } while (nBytesTranss);

        return 0;
    }

    void FfbSendData(UCHAR* FfbData, DWORD	nBytesTranss)
        // This is where the FFB data is first processed
        // The incoming data is copied to a global structure
        // The emessage is posted to inform the main window loop that new data has arrived
    {
#ifdef PRINT
        _tprintf("\nIn vJoy interface!!!");
        _tprintf("\nbytes trans %d\n", nBytesTranss);
        for (UINT i = 0; i < nBytesTranss; i++)
            _tprintf(" %02.2X", (UINT)FfbData[i]);
#endif
        FfbDataPacket.size = *(ULONG*)(&FfbData[0]);
        FfbDataPacket.cmd = *(ULONG*)(&FfbData[4]);
        ULONG offset = 2 * sizeof(ULONG);
#ifdef PRINT
        _tprintf("\nsize offset %d %d", FfbDataPacket.size, offset);
#endif
        if (FfbDataPacket.size < offset)
            return;
#ifdef PRINT
        _tprintf("\nCopying %d", FfbDataPacket.size - offset);
#endif
        for (UINT i = 0; i<FfbDataPacket.size - offset; i++) {
            FfbDataPacket.data[i] = FfbData[i + offset];
#ifdef PRINT
            _tprintf(" %02.2X", (UINT)FfbDataPacket.data[i]);
#endif
        }
#ifdef PRINT
        _tprintf("\nFFB data ptr=%p", FfbDataPacket.data);
#endif

        SendMessage(hWnd, FFB_DATA_READY, 0, 0);
    }

    void FfbProcessData(WPARAM wParam, LPARAM lParam)
    {
#ifdef PRINT
        FfbFunction((PVOID)&FfbDataPacket);
#endif
        if (FfbGebFunc)
            FfbGebFunc((PVOID)(&FfbDataPacket), FfbUserData);

#ifdef PRINT
        // Condition to start effect
        FfbEffectState = FfbGetEffectState();
        FfbEffectType = FfbGetEffectType();
        // Print data to file
        fprintf(fOut, "Cmd:%08.8X ", FfbDataPacket.cmd);
        fprintf(fOut, "ID:%02.2X ", FfbDataPacket.data[0]);
        fprintf(fOut, "Size:%02.2d ", FfbDataPacket.size - 8);
        fprintf(fOut, " - ");
        for (UINT i = 0; i<FfbDataPacket.size - 8; i++)
            fprintf(fOut, " %02.2X", FfbDataPacket.data[i]);
        if (FfbEffectState)
            fprintf(fOut, "*\n");
        else
            fprintf(fOut, "\n");
        fflush(fOut);
#endif

    }

} // extern "C"


/// Interface functions
#ifndef STATIC
extern "C" {
#else
namespace vJoyNS {
#endif
    VJOYINTERFACE_API BOOL	__cdecl	AcquireVJD(UINT rID)
        /*
            Open handle to VJD for writing position data
            Input:
                rID: Report ID (range 1-16)
            Output:
                TRUE if successful, False otherwise

            Comments:
                Don't forget to close handle when done
                An open handle prevents the system from disabling/removing Raw PDO
        */
    {
        DWORD error;

        if (rID < 1 || rID>16)
            return FALSE;

        InitDll();

        if (Get_stat(rID) == VJD_STAT_OWN)
            return TRUE;

        HANDLE hTmp = OpenDeviceInterface(rID, &error);
        Set_h(rID, hTmp);
        if (hTmp != INVALID_HANDLE_VALUE) {
            Set_stat(rID, VJD_STAT_OWN);
            Set_hNotify(rID, RegisterHandleNotification(hWnd, hTmp));
            Sync_Position(rID);
            if (IsDeviceFfb(rID))
                FfbStartThread(hTmp);
            return TRUE;
        } else {
            vJoyDeviceRemove(rID);
            return FALSE;
        }
    }


    VJOYINTERFACE_API VOID	__cdecl	RelinquishVJD(UINT rID)
    {
        if (rID < 1 || rID>16 || Get_h(rID) == INVALID_HANDLE_VALUE)
            return;

        //	FfbStop(Get_h(rID));
        Set_h(rID, INVALID_HANDLE_VALUE);
        //	CloseHandle(Get_h(rID));
        UnregisterDeviceNotification(Get_hNotify(rID));
        Set_stat(rID, VJD_STAT_FREE);
    }

    /**
        First, the saved position is updated.
        Then,
        This function writes the position data to the specified VJD
        The VJD should be open for writing. If not the function returns FALSE
        If the data is NULL or if the Report ID (rID) is out of range then the function returns FALSE.
        The function will return TRUE only if the writing was successful
    **/
    VJOYINTERFACE_API BOOL	__cdecl	UpdateVJD(UINT rID, JOYSTICK_POSITION* pPosition)
    {
        // Make sure the the ID is set
        pPosition->bDevice = (BYTE)rID;

        // Update saved position
        SavePosition(rID, pPosition);

        // Send joystick position structure to vJoy device
        return Update(rID);
    }

    /*
     Read current positions vJoy device
    */
    VJOYINTERFACE_API DWORD	__cdecl	GetPosition(UINT rID, JOYSTICK_POSITION* pPosition)
    {
        // Make sure the the ID is set
        pPosition->bDevice = (BYTE)rID;

        // Update position
        if (GetDevPosition(rID, pPosition)!=TRUE)
            return ERROR_DEVICE_NOT_AVAILABLE;
        return ERROR_SUCCESS;
    }


    VJOYINTERFACE_API BOOL	__cdecl	isVJDExists(UINT rID)
    {
        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        bool ok = GetDevStat(rID, &nbytes, buf);

        // If output is undefined then this state is unknown
        if (!ok) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDStatus(%u) - return VJD_STAT_UNKN"), ProcessId, rID);
            return FALSE;
        }

        // Device  exists?
        if (buf[0] & 0x01)
            return TRUE;
        else
            return FALSE;

#ifdef VERSION205
        DWORD e;

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: isVJDExists(%d) - Started"), ProcessId, rID);

        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        bool ok = GetDevStat(rID, &nbytes, buf);
        if (!ok || !nbytes || !buf)
            return false;

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: isVJDExists(%d) - buf[0]=%02d; buf[1]=%02d; buf[2]=%02d; buf[3]=%02d; buf[4]=%02d "), ProcessId, rID, buf[0], buf[1], buf[2], buf[3], buf[4]);

        if (buf[0] & 0x01)
            return true;
        else
            return false;


        HANDLE h = OpenDeviceInterface(rID, &e);
        if (h != INVALID_HANDLE_VALUE) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: isVJDExists(%d) - Exit OK"), ProcessId, rID);
            CloseHandle(h);
            return TRUE;
        }

        if (e == ERROR_ACCESS_DENIED) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: isVJDExists(%d) - Exit OK (Access denied - probably busy)"), ProcessId, rID);
            return TRUE;
        }
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: isVJDExists(%d) - Exit (Could not access this device)"), ProcessId, rID);
        return FALSE;
#endif // VERSION205
    }

    // Get the Process ID of the process that ownes a vJoy Device indicated by rID
    // Valid Process ID is a positive number
    // Negative numbers indicate a problem
    // Posible negative numbers:
    //  [NO_FILE_EXIST]: Usually indicates a FREE device (No owner)
    //  [NO_DEV_EXIST]: Usually indicates a MISSING device
    //  [BAD_DEV_STAT]: Indicates some internal problem
    VJOYINTERFACE_API int __cdecl	GetOwnerPid(UINT rID)
    {
        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        bool ok = GetDevStat(rID, &nbytes, buf);

        // If output is undefined then this state is unknown
        if (!ok) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetOwnerPid(%u) - GetDevStat Failed"), ProcessId, rID);
            return BAD_DEV_STAT;
        }

        // Device  exists?
        if (!(buf[0] & 0x01)) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetOwnerPid(%u) - vJoy Device does not exist"), ProcessId, rID);
            return NO_DEV_EXIST;
        }

        // Device File object is not ready
        if (!(buf[0] & 0x04)) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetOwnerPid(%u) - vJoy Device file object does not exist"), ProcessId, rID);
            return NO_FILE_EXIST;
        }

        // Get the PID
        return ((int)*(WORD*)&buf[1]);
    }


    VJOYINTERFACE_API enum VjdStat	__cdecl	GetVJDStatus(UINT rID)
        /**
            Get the status of a specified vJoy Device (VJD)
            Here are the possible statuses and how they are obtained:
            1. VJD_STAT_OWN:	The  vJoy Device is owned by this application.
                An owned VJD is marked as one when acqired in the corresponding stat[] entry.
            2. VJD_STAT_FREE:	The  vJoy Device is NOT owned by any application (including this one).
                First it is checked that the VJD is not OWNED by the application.
                Then it this function tries to open a handle to it. If succesful then it is FREE (the handle is then closed)
            3. VJD_STAT_BUSY:	The  vJoy Device is owned by another application. It cannot be acquired by this application.
                First it is checked that the VJD is not OWNED by the application.
                Then it this function tries to open a handle to it.
                If failes with error  ERROR_ACCESS_DENIED then it is BUSY.
            4. VJD_STAT_MISS:	The  vJoy Device is missing. It either does not exist or the driver is down.
                First it is checked that the VJD is not OWNED by the application.
                Then it this function tries to open a handle to it.
                If failes with error other than ERROR_ACCESS_DENIED then it is MISSing.
            5. VJD_STAT_UNKN: Unknown state.
        **/
    {

        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        bool ok = GetDevStat(rID, &nbytes, buf);

        // If output is undefined then this state is unknown
        if (!ok) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDStatus(%u) - return VJD_STAT_UNKN"), ProcessId, rID);
            return VJD_STAT_UNKN;
        }

        // Device does not exists?
        if (!(buf[0] & 0x01)) {
            Set_stat(rID, VJD_STAT_MISS);
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDStatus(%u) - return VJD_STAT_MISS"), ProcessId, rID);
            return Get_stat(rID);
        }

        // Device not associated with a file object?
        if (!(buf[0] & 0x04)) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDStatus(%u) - return VJD_STAT_FREE"), ProcessId, rID);
            Set_stat(rID, VJD_STAT_FREE);
            return Get_stat(rID);
        }

        // If Process ID of the process that created the file object is the same as this
        // then the device is owned by this process
        DWORD CurrPid = GetCurrentProcessId();
        DWORD DevPid = *(DWORD*)(&(buf[1]));
        if (CurrPid == DevPid)
            Set_stat(rID, VJD_STAT_OWN);
        else
            Set_stat(rID, VJD_STAT_BUSY);

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDStatus(%u) - return %u"), ProcessId, rID, (UCHAR)Get_stat(rID));

        return Get_stat(rID);

    }


    VJOYINTERFACE_API BOOL	__cdecl isVJDOpen(UINT rID)
    {
        DWORD e;

        HANDLE h = OpenDeviceInterface(rID, &e);
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        if (e == ERROR_ACCESS_DENIED)
            return TRUE;
        return FALSE;
    }

    VJOYINTERFACE_API SHORT	__cdecl GetvJoyVersion(void)
        /*
            Get the version number of the installed vJoy driver
            Returns 0 if fails
        */
    {

        USHORT version = 0;
        int res = 1;

        int i = GetDeviceIndexById(VENDOR_N_ID, PRODUCT_N_ID, 0);
        if (i < 0)
            return 0;

        res = GetDeviceVersionNumber(i, &version);
        if (res < 0)
            return 0;

        return version;
    }

    VJOYINTERFACE_API PVOID	__cdecl GetvJoyProductString(void)
        /*
            Get the Product String of the installed vJoy driver
            Returns NULL if fails
        */
    {
        PWSTR ProductString;
        BOOL res = TRUE;

        int i = GetDeviceIndexById(VENDOR_N_ID, PRODUCT_N_ID, 0);
        if (i < 0)
            return NULL;

        res = GetDeviceProductString(i, &ProductString);
        if (!res)
            return NULL;

        return (PVOID)ProductString;
    }



    VJOYINTERFACE_API PVOID	__cdecl GetvJoyManufacturerString(void)
        /*
            Get the Manufacturer String of the installed vJoy driver
            Returns NULL if fails
        */
    {
        PWSTR ManufacturerString;
        BOOL res = TRUE;

        int i = GetDeviceIndexById(VENDOR_N_ID, PRODUCT_N_ID, 0);
        if (i < 0)
            return NULL;

        res = GetDeviceManufacturerString(i, &ManufacturerString);
        if (!res)
            return NULL;

        return (PVOID)ManufacturerString;
    }



    VJOYINTERFACE_API PVOID	__cdecl GetvJoySerialNumberString(void)
        /*
            Get the Manufacturer String of the installed vJoy driver
            Returns NULL if fails
        */
    {
        PWSTR SerialNumberString;
        BOOL res = TRUE;

        int i = GetDeviceIndexById(VENDOR_N_ID, PRODUCT_N_ID, 0);
        if (i < 0)
            return NULL;

        res = GetDeviceSerialNumberString(i, &SerialNumberString);
        if (!res)
            return NULL;

        return (PVOID)SerialNumberString;
    }

    VJOYINTERFACE_API BOOL	__cdecl	DriverMatch(WORD* DllVer, WORD* DrvVer)
        /*
            Compare the version of this DLL to the driver's
            Return TRUE if identical, otherwise return FALSE
            If DllVer a valid pointer - sets the version of this DLL file	(e.g. 0x0205)
            If DrvVer a valid pointer - sets the version of driver			(e.g. 0x0205)
            */
    {

        WORD vJoyVersion = GetvJoyVersion();
        // Only 3 digits for now, same as driver when we built it
        WORD DLLVersion = VERSION_N;
        // 4 digits version will be used when bugfix will be needed.
        // See CreateVersion.cpp, VERSION_N in public.h and vjoy.inx
        //WORD DLLVersion = (VER_X_ << 12) + (VER_H_ << 8) + (VER_M_ << 4) + VER_L_;

        if (DllVer)
            *DllVer = DLLVersion;
        if (DrvVer)
            *DrvVer = vJoyVersion;

        return (DLLVersion == vJoyVersion);
    }

    VJOYINTERFACE_API BOOL	__cdecl GetVJDAxisExist(UINT rID, UINT Axis)
        /*
            This function returns TRUE if Axis exists
            Otherwise FALSE.
            Axis can be in the range 0x30-0x39 (HID_USAGE_X - HID_USAGE_POV)
            as defined in header file public.h
        */
    {
        HIDP_VALUE_CAPS pValCaps;
        return GetAxisCaps(rID, Axis, &pValCaps);

        if (!AreControlsInit(rID))
            GetControls(rID);

        switch (Axis) {
            case HID_USAGE_X:
                return vJoyDevices[rID].DeviceControls.AxisX;
            case HID_USAGE_Y:
                return vJoyDevices[rID].DeviceControls.AxisY;
            case HID_USAGE_Z:
                return vJoyDevices[rID].DeviceControls.AxisZ;
            case HID_USAGE_RX:
                return vJoyDevices[rID].DeviceControls.AxisXRot;
            case HID_USAGE_RY:
                return vJoyDevices[rID].DeviceControls.AxisYRot;
            case HID_USAGE_RZ:
                return vJoyDevices[rID].DeviceControls.AxisZRot;
            case HID_USAGE_SL0:
                return vJoyDevices[rID].DeviceControls.Slider;
            case HID_USAGE_SL1:
                return vJoyDevices[rID].DeviceControls.Dial;

            case HID_USAGE_WHL:
                return vJoyDevices[rID].DeviceControls.Wheel;
            case HID_USAGE_ACCELERATOR:
                return vJoyDevices[rID].DeviceControls.Accelerator;
            case HID_USAGE_BRAKE:
                return vJoyDevices[rID].DeviceControls.Brake;
            case HID_USAGE_CLUTCH:
                return vJoyDevices[rID].DeviceControls.Clutch;
            case HID_USAGE_STEERING:
                return vJoyDevices[rID].DeviceControls.Steering;
            case HID_USAGE_AILERON:
                return vJoyDevices[rID].DeviceControls.Aileron;
            case HID_USAGE_RUDDER:
                return vJoyDevices[rID].DeviceControls.Rudder;
            case HID_USAGE_THROTTLE:
                return vJoyDevices[rID].DeviceControls.Throttle;
        };

        return FALSE;

    }
    VJOYINTERFACE_API BOOL	__cdecl GetVJDAxisMax(UINT rID, UINT Axis, LONG* Max)
    {
        // Get logical Maximum value for a given axis defined in the specified VDJ
        HIDP_VALUE_CAPS ValCaps;
        if (GetAxisCaps(rID, Axis, &ValCaps) < 0)
            return FALSE;
        *Max = ValCaps.LogicalMax;
        return TRUE;
    }
    VJOYINTERFACE_API BOOL	__cdecl GetVJDAxisMin(UINT rID, UINT Axis, LONG* Min)
    {
        // Get logical Maximum value for a given axis defined in the specified VDJ
        HIDP_VALUE_CAPS ValCaps;
        if (GetAxisCaps(rID, Axis, &ValCaps) < 0)
            return FALSE;
        *Min = ValCaps.LogicalMin;
        return TRUE;
    }


    VJOYINTERFACE_API int	__cdecl GetVJDButtonNumber(UINT rID)
        /*
            This function returns number of buttons for the specified device
            If fales: Negative number
        */
    {
        NTSTATUS stat = HIDP_STATUS_SUCCESS;
        PHIDP_PREPARSED_DATA PreparsedData = NULL;
        HIDP_CAPS Capabilities;

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDButtonNumber(rID=%u) - Starting"), ProcessId, rID);

        if (!AreControlsInit(rID))
            GetControls(rID);
        return 	 vJoyDevices[rID].DeviceControls.nButtons;

#if OLD_PREPARSED
        int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
        HANDLE h = GetHandleByIndex(Index);
        if (!h || h == INVALID_HANDLE_VALUE)
            return NO_HANDLE_BY_INDEX;

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDButtonNumberfprintf_s - Using Index=%d"), ProcessId, rID, Index);

        BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
        HANDLE h = INVALID_HANDLE_VALUE;
        BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

        if (!ok) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetVJDButtonNumber(rID=%u) - HidD_GetPreparsedData() failed"), ProcessId, rID);
            CloseHandle(h);
            return BAD_PREPARSED_DATA;
        } else
            stat = HidP_GetCaps(PreparsedData, &Capabilities);
        if (stat != HIDP_STATUS_SUCCESS) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetVJDButtonNumber(rID=%u) - HidP_GetCaps() failed"), ProcessId, rID);
            CloseHandle(h);
            return NO_CAPS;
        }

        if (LogStream) {
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDButtonNumber(rID=%u) - Capabilities: "), ProcessId, rID);
            _ftprintf_s(LogStream, _T("\t Usage=0x%x;"), Capabilities.Usage);
            _ftprintf_s(LogStream, _T("\t UsagePage=0x%x;"), Capabilities.UsagePage);
            _ftprintf_s(LogStream, _T("\t InputReportByteLength=%u;"), Capabilities.InputReportByteLength);
            _ftprintf_s(LogStream, _T("\t NumberLinkCollectionNodes=%u;"), Capabilities.NumberLinkCollectionNodes);
            _ftprintf_s(LogStream, _T("\t NumberInputButtonCaps=%u;"), Capabilities.NumberInputButtonCaps);
            _ftprintf_s(LogStream, _T("\t NumberInputValueCaps=%u;"), Capabilities.NumberInputValueCaps);
            _ftprintf_s(LogStream, _T("\t NumberInputDataIndices=%u;"), Capabilities.NumberInputDataIndices);
        }

        // Get Button data
        int ButtonBaseIndex, nButtons = 0;
        USHORT n = Capabilities.NumberInputButtonCaps;
        if (n < 1) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetVJDButtonNumber(rID=%u) - Number of button Caps is %u"), ProcessId, rID, n);
            CloseHandle(h);
            return BAD_N_BTN_CAPS;
        }
        HIDP_BUTTON_CAPS* bCaps = new HIDP_BUTTON_CAPS[n];
        SecureZeroMemory(bCaps, sizeof(HIDP_BUTTON_CAPS)*n);
        stat = HidP_GetButtonCaps(HidP_Input, bCaps, &n, PreparsedData);
        if (stat != HIDP_STATUS_SUCCESS) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetVJDButtonNumber(rID=%u) - HidP_GetButtonCaps() failed"), ProcessId, rID);
            CloseHandle(h);
            delete[] 	bCaps;
            return BAD_BTN_CAPS;
        }

        if (LogStream) {
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDButtonNumber(rID=%u) - Button Capabilities: "), ProcessId, rID);
            _ftprintf_s(LogStream, _T("\t UsagePage=0x%x;"), bCaps[0].UsagePage);
            _ftprintf_s(LogStream, _T("\t ReportID=%u;"), bCaps[0].ReportID);
            _ftprintf_s(LogStream, _T("\t UsageMax=%u;"), (bCaps[0].Range).UsageMax);
            _ftprintf_s(LogStream, _T("\t UsageMin=%u;"), (bCaps[0].Range).UsageMin);
            _ftprintf_s(LogStream, _T("\t DataIndexMin=%u;"), (bCaps[0].Range).DataIndexMin);
        }

        // Assuming one button range, get the number of buttons
        if (bCaps[0].IsRange) {
            nButtons += (bCaps[0].Range).UsageMax - (bCaps[0].Range).UsageMin + 1;
            ButtonBaseIndex = (bCaps[0].Range).DataIndexMin;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetVJDButtonNumber(rID=%u) - Bad Range"), ProcessId, rID);
            CloseHandle(h);
            delete[] 	bCaps;
            return BAD_BTN_RANGE;
        }

        delete[] 	bCaps;
        //	HidD_FreePreparsedData(PreparsedData);
        CloseHandle(h);

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetVJDButtonNumber(rID=%u) - Return(nButtons=%d)"), ProcessId, rID, nButtons);
        return nButtons;
    }


    VJOYINTERFACE_API int	__cdecl GetVJDDiscPovNumber(UINT rID)
        /*
            This function returns the number of discrete POV Hat switch on the specified vJoy device
            The function returns -1 if error.
        */
    {
        if (!AreControlsInit(rID))
            GetControls(rID);
        return 	 vJoyDevices[rID].DeviceControls.nDescHats;

        NTSTATUS stat = HIDP_STATUS_SUCCESS;
        PHIDP_PREPARSED_DATA PreparsedData = NULL;
        HIDP_CAPS Capabilities;
        SecureZeroMemory(&Capabilities, sizeof(HIDP_CAPS));
        int res = 0;

#if OLD_PREPARSED
        int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
        HANDLE h = GetHandleByIndex(Index);
        if (!h || h == INVALID_HANDLE_VALUE)
            return 0;

        BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
        HANDLE h = INVALID_HANDLE_VALUE;
        BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

        if (!ok) {
            CloseHandle(h);
            return 0;
        };
        stat = HidP_GetCaps(PreparsedData, &Capabilities);
        if (stat != HIDP_STATUS_SUCCESS) {
            CloseHandle(h);
            return 0;
        }

        // Get data related to values (axes/POVs)
        int Usage, DataIndex;
        USHORT n = Capabilities.NumberInputValueCaps;
        if (n < 1) {
            CloseHandle(h);
            return 0;
        }

        PHIDP_VALUE_CAPS vCaps = new HIDP_VALUE_CAPS[1 + n]; // Added 1 just to make the Analyzer happy
        stat = HidP_GetValueCaps(HidP_Input, vCaps, &n, PreparsedData);
        if (stat == HIDP_STATUS_SUCCESS) {
            for (int i = 0; i < n; i++) // Loop on all values
            {
                Usage = ((vCaps[i]).NotRange).Usage; // Usage is the code of the axis (0x30="X", 0x39="POV etc.)
                if ((HID_USAGE_POV == Usage) && (vCaps[i].LogicalMax == 3)) {
                    res++;
                }
                DataIndex = ((vCaps[i]).NotRange).DataIndex; // Every control has an index
            }
        }

        //HidD_FreePreparsedData(PreparsedData);
        delete[](vCaps);
        CloseHandle(h);

        return res;
    }

    VJOYINTERFACE_API int	__cdecl GetVJDContPovNumber(UINT rID)
    {
        /*
            This function returns number of continous POV switches if it succeeds
            or negative number if fails
        */
        if (!AreControlsInit(rID))
            GetControls(rID);
        return 	 vJoyDevices[rID].DeviceControls.nContHats;

        int res = 0;
        NTSTATUS stat = HIDP_STATUS_SUCCESS;
        PHIDP_PREPARSED_DATA PreparsedData = NULL;
        HIDP_CAPS Capabilities;

#if OLD_PREPARSED
        int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
        HANDLE h = GetHandleByIndex(Index);
        if (!h || h == INVALID_HANDLE_VALUE)
            return 0;

        BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
        HANDLE h = INVALID_HANDLE_VALUE;
        BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

        if (!ok) {
            CloseHandle(h);
            return 0;
        }
        stat = HidP_GetCaps(PreparsedData, &Capabilities);
        if (stat != HIDP_STATUS_SUCCESS) {
            CloseHandle(h);
            return 0;
        }

        // Get data related to values (axes/POVs)
        int Usage, DataIndex;
        USHORT n = Capabilities.NumberInputValueCaps;
        if (n < 1) {
            CloseHandle(h);
            return 0;
        }

        PHIDP_VALUE_CAPS vCaps = new HIDP_VALUE_CAPS[1 + n]; // Added 1 just to make the Analyzer happy
        stat = HidP_GetValueCaps(HidP_Input, vCaps, &n, PreparsedData);
        if (stat == HIDP_STATUS_SUCCESS) {
            for (int i = 0; i < n; i++) // Loop on all values
            {
                Usage = ((vCaps[i]).NotRange).Usage; // Usage is the code of the axis (0x30="X", 0x39="POV1 etc.)
                if ((HID_USAGE_POV == Usage) && (vCaps[i].LogicalMax > 3)) {
                    //CloseHandle(h);
                    res++;
                }
                DataIndex = ((vCaps[i]).NotRange).DataIndex; // Every control has an index
            }
        }

        //HidD_FreePreparsedData(PreparsedData);
        delete[](vCaps);
        CloseHandle(h);

        return res;
    }

    VJOYINTERFACE_API BOOL	__cdecl	ResetVJD(UINT rID)
    {

#if 0
        // Reset all controls to predefined values in the specified VDJ
        BOOL res = InitPosition(rID);
        if (!res)
            return res;

        // Now, sent it to the device
        return UpdateVJD(rID, &(vJoyDevices[rID].position));
#else
        UINT	IoCode = RESET_DEV;
        HANDLE	h = NULL;
        HANDLE	hIoctlEvent;
        OVERLAPPED OverLapped = { 0 };
        ULONG	bytes = 0;

        // Handle to device
        if (rID)
            h = Get_h(rID);
        else
            h = GetGenControlHadle();

        // Preparing
        hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
        memset(&OverLapped, 0, sizeof(OVERLAPPED));
        OverLapped.hEvent = hIoctlEvent;

        // Sending RESET message to device
        BOOL	res = DeviceIoControl(h, IoCode, NULL, 0, NULL, 0, &bytes, &OverLapped);
        if (!res) {
            // The transaction was not completed.
            // If it is just because it is pending then wait otherwise it is an error
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: UpdateVJD() - DeviceIoControl was not completed"), ProcessId);
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                CloseHandle(OverLapped.hEvent);
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: UpdateVJD() - DeviceIoControl failed with error 0x%X"), ProcessId, err);
                return FALSE;
            } else {	// Wait for write to complete
                DWORD WaitRet = WaitForSingleObject(OverLapped.hEvent, 500);
                if (WAIT_OBJECT_0 != WaitRet) {
                    CloseHandle(OverLapped.hEvent);
                    if (LogStream)
                        _ftprintf_s(LogStream, _T("\n[%05u]Error: UpdateVJD() - WaitForSingleObject returned 0x%X"), ProcessId, WaitRet);
                    return FALSE;
                }
            }
        }
        CloseHandle(OverLapped.hEvent);
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: UpdateVJD() - DeviceIoControl successful"), ProcessId);
        return TRUE;
#endif // 0
    }

    VJOYINTERFACE_API VOID	__cdecl	ResetAll(void)
    {
#if 0

        for (int i = 1; i <= 16; i++) {
            if (!isVJDExists(i))
                continue;
            InitPosition(i);
            UpdateVJD(i, &(vJoyDevices[i].position));
        }
#else
        // Reset all controls to predefined values in all VDJ
        ResetVJD(0);
#endif // 0
    }

    VJOYINTERFACE_API BOOL	__cdecl	ResetButtons(UINT rID)
    {
        // Reset all buttons (To Default values) in the specified VDJ
        if (rID < 1 || rID > 16)
            return FALSE;

        // Initialize
        DEVICE_INIT_VALS data_buf;
        size_t s = sizeof(DEVICE_INIT_VALS);
        data_buf.cb = s;
        data_buf.id = rID;


        // Calculate default position
        CalcInitValue(rID, &data_buf);

        // Reset only buttons
        vJoyDevices[rID].position.lButtons = ((DWORD*)(data_buf.ButtonMask))[0];
        vJoyDevices[rID].position.lButtonsEx1 = ((DWORD*)(data_buf.ButtonMask))[1];
        vJoyDevices[rID].position.lButtonsEx2 = ((DWORD*)(data_buf.ButtonMask))[2];
        vJoyDevices[rID].position.lButtonsEx3 = ((DWORD*)(data_buf.ButtonMask))[3];

        // Send data to the device
        UpdateVJD(rID, &(vJoyDevices[rID].position));

        return TRUE;
    }

    VJOYINTERFACE_API BOOL	__cdecl	ResetPovs(UINT rID)
    {
        // Reset all POV Switches (To -1) in the specified VDJ
        if (rID < 1 || rID > 16)
            return FALSE;

        // Initialize
        DEVICE_INIT_VALS data_buf;
        size_t s = sizeof(DEVICE_INIT_VALS);
        data_buf.cb = s;
        data_buf.id = rID;


        // Calculate default position
        CalcInitValue(rID, &data_buf);

        if (data_buf.InitValPov[0] == 0xFF)
            vJoyDevices[rID].position.bHats = (DWORD)-1;
        else
            vJoyDevices[rID].position.bHats = data_buf.InitValPov[0] * 0x8C9F / 100 + 1;

        if (data_buf.InitValPov[1] == 0xFF)
            vJoyDevices[rID].position.bHatsEx1 = (DWORD)-1;
        else
            vJoyDevices[rID].position.bHatsEx1 = data_buf.InitValPov[1] * 0x8C9F / 100 + 1;

        if (data_buf.InitValPov[2] == 0xFF)
            vJoyDevices[rID].position.bHatsEx2 = (DWORD)-1;
        else
            vJoyDevices[rID].position.bHatsEx2 = data_buf.InitValPov[2] * 0x8C9F / 100 + 1;

        if (data_buf.InitValPov[3] == 0xFF)
            vJoyDevices[rID].position.bHatsEx3 = (DWORD)-1;
        else
            vJoyDevices[rID].position.bHatsEx3 = data_buf.InitValPov[3] * 0x8C9F / 100 + 1;


        UpdateVJD(rID, &(vJoyDevices[rID].position));
        return TRUE;
    }

    VJOYINTERFACE_API BOOL	__cdecl	SetAxis(LONG Value, UINT rID, UINT Axis)
    {
        /* Write Value to a given axis defined in the specified VDJ
            Limited to the following axes:
            HID_USAGE_X		0x30
            HID_USAGE_Y		0x31
            HID_USAGE_Z		0x32
            HID_USAGE_RX	0x33
            HID_USAGE_RY	0x34
            HID_USAGE_RZ	0x35
            HID_USAGE_SL0	0x36
            HID_USAGE_SL1	0x37
            HID_USAGE_WHL	0x38
            HID_USAGE_ACCELERATOR   0xC4
            HID_USAGE_BRAKE         0xC5
            HID_USAGE_CLUTCH        0xC6
            HID_USAGE_STEERING      0xC8
            HID_USAGE_AILERON       0xB0
            HID_USAGE_RUDDER        0xBA
            HID_USAGE_THROTTLE      0xBB
        */
        if (rID < 1 || rID>16)
            return FALSE;

        switch (Axis) {
            case HID_USAGE_X:
                vJoyDevices[rID].position.wAxisX = Value;
                break;
            case HID_USAGE_Y:
                vJoyDevices[rID].position.wAxisY = Value;
                break;
            case HID_USAGE_Z:
                vJoyDevices[rID].position.wAxisZ = Value;
                break;
            case HID_USAGE_RX:
                vJoyDevices[rID].position.wAxisXRot = Value;
                break;
            case HID_USAGE_RY:
                vJoyDevices[rID].position.wAxisYRot = Value;
                break;
            case HID_USAGE_RZ:
                vJoyDevices[rID].position.wAxisZRot = Value;
                break;
            case HID_USAGE_SL0:
                vJoyDevices[rID].position.wSlider = Value;
                break;
            case HID_USAGE_SL1:
                vJoyDevices[rID].position.wDial = Value;
                break;

            case HID_USAGE_WHL:
                vJoyDevices[rID].position.wWheel = Value;
                break;
            case HID_USAGE_ACCELERATOR:
                vJoyDevices[rID].position.wAccelerator = Value;
                break;
            case HID_USAGE_BRAKE:
                vJoyDevices[rID].position.wBrake = Value;
                break;
            case HID_USAGE_CLUTCH:
                vJoyDevices[rID].position.wClutch = Value;
                break;
            case HID_USAGE_STEERING:
                vJoyDevices[rID].position.wSteering = Value;
                break;
            case HID_USAGE_THROTTLE:
                vJoyDevices[rID].position.wThrottle = Value;
                break;
            case HID_USAGE_RUDDER:
                vJoyDevices[rID].position.wRudder = Value;
                break;
            case HID_USAGE_AILERON:
                vJoyDevices[rID].position.wAileron = Value;
                break;
            default:
                return FALSE;
        }

        return UpdateVJD(rID, &(vJoyDevices[rID].position));
    }

    VJOYINTERFACE_API BOOL	__cdecl	SetBtn(BOOL Value, UINT rID, UCHAR nBtn)
    {
        LONG Mask = 0x00000001;
        LONG* Target;

        // Write Value to a given button defined in the specified VDJ
        if (rID < 1 || rID>16 || nBtn < 1 || nBtn>128)
            return FALSE;

        int Shift = (nBtn - 1) % 32;

        switch ((nBtn - 1) / 32) {
            case 0:
                Target = &(vJoyDevices[rID].position.lButtons);
                break;
            case 1:
                Target = &(vJoyDevices[rID].position.lButtonsEx1);
                break;
            case 2:
                Target = &(vJoyDevices[rID].position.lButtonsEx2);
                break;
            case 3:
                Target = &(vJoyDevices[rID].position.lButtonsEx3);
                break;
            default:
                Target = &(vJoyDevices[rID].position.lButtons);
                break;
        };


        // If Value=TRUE the the given button is set to 1
        if (Value) {
            Mask = Mask << (Shift);
            *Target |= Mask;
        } else {
            Mask = Mask << (Shift);
            Mask = ~Mask;
            *Target &= Mask;
        }


        return UpdateVJD(rID, &(vJoyDevices[rID].position));
    }

    VJOYINTERFACE_API BOOL	__cdecl	SetDiscPov(int Value, UINT rID, UCHAR nPov)
    {
        // Write Value to a given descrete POV defined in the specified VDJ
        // nPov: POV serial number (1-4)
        // Write Value to a given continuous POV defined in the specified VDJ
        DWORD Mask = 0x0F;
        DWORD input;

        if (rID < 1 || rID>16 || nPov < 1 || nPov>4)
            return FALSE;

        //input = (DWORD)Value;
        Value &= 0x0F;


        input = Value << (4 * (nPov - 1));
        Mask = Mask << (4 * (nPov - 1));
        Mask = ~Mask;


        vJoyDevices[rID].position.bHats &= Mask;
        vJoyDevices[rID].position.bHats |= input;
        return UpdateVJD(rID, &(vJoyDevices[rID].position));
    }

    VJOYINTERFACE_API BOOL	__cdecl	SetContPov(DWORD Value, UINT rID, UCHAR nPov)
    {
        // Write Value to a given continuous POV defined in the specified VDJ
        // Value: POV position (North=0, East=1, South=2, West=3, Neutral=0x0F

        if (rID < 1 || rID>16)
            return FALSE;

        switch (nPov) {
            case 1:
                vJoyDevices[rID].position.bHats = Value;
                return UpdateVJD(rID, &(vJoyDevices[rID].position));
            case 2:
                vJoyDevices[rID].position.bHatsEx1 = Value;
                return UpdateVJD(rID, &(vJoyDevices[rID].position));
            case 3:
                vJoyDevices[rID].position.bHatsEx2 = Value;
                return UpdateVJD(rID, &(vJoyDevices[rID].position));
            case 4:
                vJoyDevices[rID].position.bHatsEx3 = Value;
                return UpdateVJD(rID, &(vJoyDevices[rID].position));
            default:
                break;
        };

        return FALSE;
    }

    VJOYINTERFACE_API VOID		__cdecl	FfbRegisterGenCB(FfbGenCB cb, PVOID data)
        // Registers a Generic FFB callback function
        // This function is called with every chunk of data
    {
        FfbGebFunc = cb;
        FfbUserData = data;
    }

    /*
    Test if a given device supports a specific FFB Effect
    Indicate device by Device ID
    Indicate effect by its usage
    If Device supports the FFB effect then return TRUE
    Else return FALSE
    */
    VJOYINTERFACE_API BOOL	__cdecl		IsDeviceFfbEffect(UINT rID, UINT Effect)
    {
        NTSTATUS stat = HIDP_STATUS_SUCCESS;
        PHIDP_PREPARSED_DATA PreparsedData = NULL;
        HIDP_CAPS Capabilities;

        // Get the Value Capabilities of a given axis in a given device
#if OLD_PREPARSED
        int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
        HANDLE h = GetHandleByIndex(Index);
        if (!h || h == INVALID_HANDLE_VALUE)
            return FALSE;
        // Get Preparsed data
        BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
        HANDLE h = INVALID_HANDLE_VALUE;
        BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

        if (!ok) {
            //HidD_FreePreparsedData(PreparsedData);
            CloseHandle(h);
            return FALSE;
        }

        // returns a top-level collection's HIDP_CAPS structure.
        stat = HidP_GetCaps(PreparsedData, &Capabilities);
        if (stat != HIDP_STATUS_SUCCESS) {
            //HidD_FreePreparsedData(PreparsedData);
            CloseHandle(h);
            return FALSE;
        }

        // Get output buttons
        USHORT nb_bu;
        USHORT nb = Capabilities.NumberOutputButtonCaps;
        if (nb < 4)
            return FALSE;


        HIDP_BUTTON_CAPS* bCaps = new HIDP_BUTTON_CAPS[nb];
        if (!bCaps)
            return FALSE;
        SecureZeroMemory(bCaps, sizeof(HIDP_BUTTON_CAPS)*nb);
        nb_bu = nb;
        stat = HidP_GetButtonCaps(HidP_Output, bCaps, &nb, PreparsedData);
        if (FAILED(stat))
            return FALSE;

        if (nb > nb_bu)
            return FALSE;

        BOOL Out = FALSE;

        if (nb < 1)
            return FALSE;

        if (stat == HIDP_STATUS_SUCCESS) {
            for (int i = 0; i < nb; i++) // Loop on all values
                if ((bCaps[i].ReportID == (HID_ID_EFFREP + 0x10 * rID))     //    HID_ID_EFFREP + 0x10 * TLID	(This is for Device #1)
                    && (bCaps[i].UsagePage == 0x0F) //    Usage Page Physical Interface
                    && (bCaps[i].LinkUsage == 0x25) //    Usage Effect Type
                    && (bCaps[i].NotRange.Usage == Effect) 	//    Usage Effect Type
                    ) {
                    Out = TRUE;
                    break;
                }
        }


        //HidD_FreePreparsedData(PreparsedData);
        delete[](bCaps);
        CloseHandle(h);
        return Out;
    }


    VJOYINTERFACE_API FFBEType		__cdecl	FfbGetEffect()
        // Returns effect serial number if active, 0 if inactive
    {
        // If There's an effect then return the type
        if (FfbEffectState)
            return FfbEffectType;
        else
            return ET_NONE;
    }

    // Start the FFB queues of the specified vJoy Device.
    VJOYINTERFACE_API BOOL		__cdecl	FfbStart(UINT rID)
    {
        // Test that device exists and owned by this process
        if (rID < 1 || rID>16 || Get_h(rID) == INVALID_HANDLE_VALUE || Get_stat(rID) != VJD_STAT_OWN)
            return FALSE;
#if 0
        return 	_FfbStart(Get_h((int)rID));
#endif
        return TRUE;
    }

    // Stop the FFB queues of the specified vJoy Device.
    VJOYINTERFACE_API VOID		__cdecl	FfbStop(UINT rID)	// Obsolete
    {
        // Test that device exists and owned by this process
        if (rID<1 || rID>16 || Get_h(rID) == INVALID_HANDLE_VALUE || Get_stat(rID) != VJD_STAT_OWN)
            return;
#if 0
        _FfbStop(Get_h((int)rID));
#endif
    }



    VJOYINTERFACE_API BOOL		__cdecl vJoyEnabled(void)
    {
        // Returns true if  VJD #0 is confugured
        // which means that the Raw PDO exists
        DWORD error = 0;
        int Size;

        if (GetDeviceNameSpace(NULL, &Size, FALSE, &error))
            return true;
        return false;

    }

    VJOYINTERFACE_API BOOL	__cdecl	vJoyFfbCap(BOOL* Supported)
    {
        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        if (!Supported)
            return FALSE;

        bool ok = GetDrvStat(&nbytes, buf);
        if (!ok)
            return FALSE;

        *Supported = buffer[0] & 0x01;
        return (TRUE);
    }

    VJOYINTERFACE_API BOOL	__cdecl	GetNumberExistingVJD(int* n)
    {
        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        if (!n)
            return FALSE;

        bool ok = GetDrvStat(&nbytes, buf);
        if (!ok || nbytes < 3)
            return FALSE;

        *n = buffer[2];
        return (TRUE);
    }


    VJOYINTERFACE_API BOOL	__cdecl	GetvJoyMaxDevices(int* n)
    {
        int nbytes = 10;
        BYTE buffer[10] = { 0 };
        BYTE* buf = buffer;

        if (!n)
            return FALSE;

        bool ok = GetDrvStat(&nbytes, buf);
        if (!ok || nbytes < 2)
            return FALSE;

        *n = buffer[1];
        return (TRUE);
    }


    VJOYINTERFACE_API VOID		__cdecl	RegisterRemovalCB(RemovalCB cb, PVOID data)
        // Registers a Generic FFB callback function
        // This function is called with every chunk of data
    {
        InitDll();
        RemovalFunc = cb;
        RemovalData = data;
    }

    VJOYINTERFACE_API BOOL __cdecl IsDeviceFfb(UINT rID)
    {
        NTSTATUS stat = HIDP_STATUS_SUCCESS;
        PHIDP_PREPARSED_DATA PreparsedData = NULL;
        HIDP_CAPS Capabilities;

        // Get the Value Capabilities of a given axis in a given device
#if OLD_PREPARSED
        int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
        HANDLE h = GetHandleByIndex(Index);
        if (!h || h == INVALID_HANDLE_VALUE)
            return FALSE;
        // Get Preparsed data
        BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
        HANDLE h = INVALID_HANDLE_VALUE;
        BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

        if (!ok) {
            //HidD_FreePreparsedData(PreparsedData);
            CloseHandle(h);
            return FALSE;
        }

        // returns a top-level collection's HIDP_CAPS structure.
        stat = HidP_GetCaps(PreparsedData, &Capabilities);
        if (stat != HIDP_STATUS_SUCCESS) {
            //HidD_FreePreparsedData(PreparsedData);
            CloseHandle(h);
            return FALSE;
        }

        // Get data related to output values
        UINT Usage;
        ULONG n = Capabilities.NumberLinkCollectionNodes;
        if (n<1) {
            CloseHandle(h);
            //HidD_FreePreparsedData(PreparsedData);
            return FALSE;
        }

        // Get array of of link collections
        PHIDP_LINK_COLLECTION_NODE vLinks = new   HIDP_LINK_COLLECTION_NODE[1 + n];
        stat = HidP_GetLinkCollectionNodes(vLinks, &n, PreparsedData);
        if (FAILED(stat))
            return FALSE;

        // Loop on every link
        BOOL Out = FALSE;
        for (UINT cnt = 0; cnt < n; cnt++) {
            HIDP_LINK_COLLECTION_NODE Link = vLinks[cnt];

            // This collection is:
            //  Usage Set Effect Report(0x21),
            //  Usage Page Physical Interface (0x0F),
            //	Type= Output (2)
            if (Link.LinkUsage == 0x21 && Link.LinkUsagePage == 0xf && Link.CollectionType == 2) {
                // Found
                Out = TRUE;
                break;
            }
        }

        // Cleanuo & Exit
        //HidD_FreePreparsedData(PreparsedData);
        delete[](vLinks);
        CloseHandle(h);
        return Out;

    }

#pragma region FFB Functions for reading out of inner data packet only

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_DeviceID(const UCHAR* data, UINT* DeviceID)
		// If valid device ID was found then returns ERROR_SUCCESS and sets the ID (Range 1-15) in DeviceID.
		// If Packet is NULL then returns ERROR_INVALID_PARAMETER. DeviceID is undefined.
		// If Packet is malformed or Device ID is out of range then returns ERROR_INVALID_DATA. DeviceID is undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		*DeviceID = (((data[0]) & 0xF0) >> 4);
		if (*DeviceID<1)
			return ERROR_INVALID_DATA;
		else
			return ERROR_SUCCESS;
	}
	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_Type(const UCHAR* data, FFBPType* Type, int cmd)
		//If valid Type was found then returns ERROR_SUCCESS and sets Type.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Feature  is undefined.
		//If Packet is malformed then returns ERROR_INVALID_DATA. Feature is undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Get the type
		int tp = (data[0]) & 0x0F;

		// This is a feature then mark it as such
		if (cmd == IOCTL_HID_SET_FEATURE)
			tp += 0x10;

		(*Type) = (FFBPType)tp;
		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_EffectBlockIndex(const UCHAR* data, UINT* effectId, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and sets Index to the value of Effect Block Index (if applicable). Expected value is '1'.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed or does not contain an Effect Block Index then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type == PT_CTRLREP ||
			Type == PT_SMPLREP ||
			Type == PT_GAINREP ||
			Type == PT_POOLREP ||
			Type == PT_STATEREP)
			return ERROR_INVALID_DATA;

		if (Type == PT_NEWEFREP) {
			// Special case for new effect, ID is third byte
			*effectId = data[2];
			return ERROR_SUCCESS;
		}
		// The Effect Block Index is the second byte (After the Report ID)
		*effectId = data[1];
		return ERROR_SUCCESS;
	}
	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_EBI(const UCHAR* data, UINT* effectId, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and sets Index to the value of Effect Block Index (if applicable). Expected value is '1'.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed or does not contain an Effect Block Index then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		return Ffb_dp_EffectBlockIndex(data, effectId, cmd);
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_Eff_Report(const UCHAR* data, FFB_EFF_REPORT* Effect, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index and Magnitude.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;
		
		// @TODO: NOT NOW
		//if (Packet->size <(8 + 2 + 16))   // Header = 8+2, packet = 16+(2), (2) is optionnal, depends on direction
		//	return ERROR_INVALID_DATA;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_EFFREP)
			return ERROR_INVALID_DATA;

		Effect->EffectBlockIndex = data[1];
		Effect->EffectType = (FFBEType)(data[2]);
		Effect->Duration = (WORD)((data[4] << 8) + (data[3]));
		Effect->TrigerRpt = (WORD)((data[6] << 8) + (data[5]));
		Effect->SamplePrd = (WORD)((data[8] << 8) + (data[7]));
		Effect->StartDelay = (WORD)((data[10] << 8) + (data[9]));
		Effect->Gain = data[11];
		Effect->TrigerBtn = data[12];
		Effect->AxesEnabledDirection = data[13];
		Effect->Polar = (data[13] == 0x04);
		if (Effect->Polar)
			Effect->Direction = (WORD)((data[15] << 8) + (data[14]));
		else {
			//if (Packet->size < (8 + 2 + 18)) return ERROR_INVALID_DATA; // the extra (2) is no longer optional

			Effect->DirX = (WORD)((data[15] << 8) + (data[14]));
			Effect->DirY = (WORD)((data[17] << 8) + (data[16]));
		}
		return ERROR_SUCCESS;
	}
	VJOYINTERFACE_API DWORD		__cdecl Ffb_dp_Eff_Constant(const UCHAR* data, FFB_EFF_CONSTANT* ConstantEffect, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index and Magnitude.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_CONSTREP)
			return ERROR_INVALID_DATA;

		ConstantEffect->EffectBlockIndex = data[1];
		ConstantEffect->Magnitude = (LONG)((data[3] << 8) + (data[2]));

		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_Eff_Ramp(const UCHAR* data, FFB_EFF_RAMP* RampEffect, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index and Magnitude.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_RAMPREP)
			return ERROR_INVALID_DATA;

		RampEffect->EffectBlockIndex = data[1];
		RampEffect->Start = (LONG)(data[3] << 8) + (data[2]);
		RampEffect->End = (LONG)((data[5] << 8) + (data[4]));

		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_EffOp(const UCHAR* data, FFB_EFF_OP* Operation, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Operation- this structure holds Effect Block Index, Operation(Start, Start Solo, Stop) and Loop Count.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_EFOPREP)
			return ERROR_INVALID_DATA;

		Operation->EffectBlockIndex = data[1];
		Operation->EffectOp = (FFBOP)data[2];
		Operation->LoopCount = data[3];
		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_CreateNewEffect(const UCHAR* data, FFBEType* effectType, UINT* newEffectId, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and sets the new Effect type
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_NEWEFREP)
			return ERROR_INVALID_DATA;

		if (effectType != nullptr) {
			*effectType = static_cast <FFBEType>(data[1]);
		}
		if (newEffectId != nullptr) {
			*newEffectId = static_cast <BYTE>(data[2]);
		}
		return ERROR_SUCCESS;
	}
	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_EffNew(const UCHAR* data, FFBEType* Effect, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and sets the new Effect type
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		return Ffb_dp_CreateNewEffect(data, Effect, nullptr, cmd);
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_DevCtrl(const UCHAR* data, FFB_CTRL* Control, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and gets enum Control - this enum holds PID Device Control (Enable Actuators, Device Reset etc).
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_CTRLREP)
			return ERROR_INVALID_DATA;

		*Control = (FFB_CTRL)data[1];
		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_DevGain(const UCHAR* data, BYTE* Gain, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and gets the device global gain.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_GAINREP)
			return ERROR_INVALID_DATA;

		*Gain = data[1];
		return ERROR_SUCCESS;

	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_Eff_Period(const UCHAR* data, FFB_EFF_PERIOD* Effect, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index, Magnitude, Offset, Phase and period.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_PRIDREP)
			return ERROR_INVALID_DATA;

		Effect->EffectBlockIndex = data[1];
		Effect->Magnitude = (DWORD)((data[3] << 8) + (data[2]));
		Effect->Offset = (LONG)((data[5] << 8) + (data[4]));
		Effect->Phase = (DWORD)((data[7] << 8) + (data[6]));
		Effect->Period = (DWORD)((data[11] << 24) + (data[10] << 16) + (data[9] << 8) + (data[8]));
		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_Eff_Cond(const UCHAR* data, FFB_EFF_COND* Condition, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Condition - this structure holds Effect Block Index, Direction (X/Y), Center Point Offset, Dead Band and other conditions.
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_CONDREP)
			return ERROR_INVALID_DATA;

		Condition->EffectBlockIndex = data[1];
		Condition->isY = data[2];
		Condition->CenterPointOffset = (LONG)((data[4] << 8) + (data[3]));
		Condition->PosCoeff = (LONG)((data[6] << 8) + (data[5]));
		Condition->NegCoeff = (LONG)((data[8] << 8) + (data[7]));
		Condition->PosSatur = (DWORD)((data[10] << 8) + (data[9]));
		Condition->NegSatur = (DWORD)((data[12] << 8) + (data[11]));
		Condition->DeadBand = (LONG)((data[14] << 8) + (data[13]));
		return ERROR_SUCCESS;
	}

	VJOYINTERFACE_API DWORD __cdecl Ffb_dp_Eff_Envlp(const UCHAR* data, FFB_EFF_ENVLP* Envelope, int cmd)
		//If valid Packet was found then returns ERROR_SUCCESS and fills structure Envelope -
		//If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
		//If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
	{
		// Routine validity checks
		if (!data)
			return ERROR_INVALID_PARAMETER;

		// Some types don't carry Effect Block Index
		FFBPType Type;
		if (Ffb_dp_Type(data, &Type, cmd) != ERROR_SUCCESS)
			return ERROR_INVALID_DATA;
		if (Type != PT_ENVREP)
			return ERROR_INVALID_DATA;

		Envelope->EffectBlockIndex = data[1];
		Envelope->AttackLevel = (DWORD)((data[3] << 8) + (data[2]));
		Envelope->FadeLevel = (DWORD)((data[5] << 8) + (data[4]));
		Envelope->AttackTime = (DWORD)((data[9] << 24) + (data[8] << 16) + (data[7] << 8) + (data[6]));
		Envelope->FadeTime = (DWORD)((data[13] << 24) + (data[12] << 16) + (data[11] << 8) + (data[10]));
		return ERROR_SUCCESS;
	}


#pragma endregion

#pragma region FFB Functions



    // FFB Helper functions

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_DeviceID(const FFB_DATA* Packet, UINT* DeviceID)
        // If valid device ID was found then returns ERROR_SUCCESS and sets the ID (Range 1-15) in DeviceID.
        // If Packet is NULL then returns ERROR_INVALID_PARAMETER. DeviceID is undefined.
        // If Packet is malformed or Device ID is out of range then returns ERROR_INVALID_DATA. DeviceID is undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <10)
            return ERROR_INVALID_DATA;

        *DeviceID = (((Packet->data[0]) & 0xF0) >> 4);
        if (*DeviceID<1)
            return ERROR_INVALID_DATA;
        else
            return ERROR_SUCCESS;
    }
    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Type(const FFB_DATA* Packet, FFBPType* Type)
        //If valid Type was found then returns ERROR_SUCCESS and sets Type.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Feature  is undefined.
        //If Packet is malformed then returns ERROR_INVALID_DATA. Feature is undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <10)
            return ERROR_INVALID_DATA;

        // Get the type
        int tp = (Packet->data[0]) & 0x0F;

        // This is a feature then mark it as such
        if (Packet->cmd == IOCTL_HID_SET_FEATURE)
            tp += 0x10;

        (*Type) = (FFBPType)tp;
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Packet(const FFB_DATA* Packet, WORD* Type, UINT* DataSize, BYTE* Data[])
        //If valid Packet was found then returns ERROR_SUCCESS and sets Type to IOCTRL value (Expected values are IOCTL_HID_WRITE_REPORT and IOCTL_HID_SET_FEATURE).
        //DataSize is set to the size (in bytes) of the payload data (FFB_DATA.data ).
        //Data holds the payload data (FFB_DATA.data ).
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <10)
            return ERROR_INVALID_DATA;

        *Type = (WORD)Packet->cmd;
        *DataSize = Packet->size;
        *Data = Packet->data;
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_EffectBlockIndex(const FFB_DATA* packet, UINT* effectId)
        //If valid Packet was found then returns ERROR_SUCCESS and sets Index to the value of Effect Block Index (if applicable). Expected value is '1'.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed or does not contain an Effect Block Index then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!packet)
            return ERROR_INVALID_PARAMETER;
        if (packet->size <10)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type == PT_CTRLREP ||
            Type == PT_SMPLREP ||
            Type == PT_GAINREP ||
            Type == PT_POOLREP ||
            Type == PT_STATEREP)
            return ERROR_INVALID_DATA;

        if (Type == PT_NEWEFREP) {
            // Special case for new effect, ID is third byte
            *effectId = packet->data[2];
            return ERROR_SUCCESS;
        }
        // The Effect Block Index is the second byte (After the Report ID)
        *effectId = packet->data[1];
        return ERROR_SUCCESS;
    }
    VJOYINTERFACE_API DWORD __cdecl Ffb_h_EBI(const FFB_DATA* Packet, UINT* effectId)
        //If valid Packet was found then returns ERROR_SUCCESS and sets Index to the value of Effect Block Index (if applicable). Expected value is '1'.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed or does not contain an Effect Block Index then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        return Ffb_h_EffectBlockIndex(Packet, effectId);
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Eff_Report(const FFB_DATA* Packet, FFB_EFF_REPORT* Effect)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index and Magnitude.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <(8+2+16))   // Header = 8+2, packet = 16+(2), (2) is optionnal, depends on direction
            return ERROR_INVALID_DATA;
        
        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_EFFREP)
            return ERROR_INVALID_DATA;

        Effect->EffectBlockIndex = Packet->data[1];
        Effect->EffectType = (FFBEType)(Packet->data[2]);
        Effect->Duration = (WORD)((Packet->data[4] << 8) + (Packet->data[3]));
        Effect->TrigerRpt = (WORD)((Packet->data[6] << 8) + (Packet->data[5]));
        Effect->SamplePrd = (WORD)((Packet->data[8] << 8) + (Packet->data[7]));
        Effect->StartDelay = (WORD)((Packet->data[10] << 8) + (Packet->data[9]));
        Effect->Gain = Packet->data[11];
        Effect->TrigerBtn = Packet->data[12];
        Effect->AxesEnabledDirection = Packet->data[13];
        Effect->Polar = (Packet->data[13] == 0x04);
        if (Effect->Polar)
            Effect->Direction = (WORD)((Packet->data[15] << 8) + (Packet->data[14]));
        else {
            if (Packet->size < (8+2+18)) return ERROR_INVALID_DATA; // the extra (2) is no longer optional

            Effect->DirX = (WORD)((Packet->data[15] << 8) + (Packet->data[14]));
            Effect->DirY = (WORD)((Packet->data[17] << 8) + (Packet->data[16]));
        }
        return ERROR_SUCCESS;
    }
    VJOYINTERFACE_API DWORD		__cdecl Ffb_h_Eff_Constant(const FFB_DATA* Packet, FFB_EFF_CONSTANT* ConstantEffect)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index and Magnitude.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <12)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_CONSTREP)
            return ERROR_INVALID_DATA;

        ConstantEffect->EffectBlockIndex = Packet->data[1];
        ConstantEffect->Magnitude = (LONG)((Packet->data[3] << 8) + (Packet->data[2]));

        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Eff_Ramp(const FFB_DATA* Packet, FFB_EFF_RAMP* RampEffect)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index and Magnitude.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <14)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_RAMPREP)
            return ERROR_INVALID_DATA;

        RampEffect->EffectBlockIndex = Packet->data[1];
        RampEffect->Start = (LONG)((Packet->data[3] << 8) + (Packet->data[2]));
        RampEffect->End = (LONG)((Packet->data[5] << 8) + (Packet->data[4]));

        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_EffOp(const FFB_DATA* Packet, FFB_EFF_OP* Operation)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Operation- this structure holds Effect Block Index, Operation(Start, Start Solo, Stop) and Loop Count.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <12)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_EFOPREP)
            return ERROR_INVALID_DATA;

        Operation->EffectBlockIndex = Packet->data[1];
        Operation->EffectOp = (FFBOP)Packet->data[2];
        Operation->LoopCount = Packet->data[3];
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_CreateNewEffect(const FFB_DATA* packet, FFBEType* effectType, UINT* newEffectId)
        //If valid Packet was found then returns ERROR_SUCCESS and sets the new Effect type
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!packet)
            return ERROR_INVALID_PARAMETER;
        if (packet->size <12)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_NEWEFREP)
            return ERROR_INVALID_DATA;

        if (effectType!=nullptr) {
            *effectType = static_cast <FFBEType>(packet->data[1]);
        }
        if (newEffectId!=nullptr) {
            *newEffectId = static_cast <BYTE>(packet->data[2]);
        }
        return ERROR_SUCCESS;
    }
    VJOYINTERFACE_API DWORD __cdecl Ffb_h_EffNew(const FFB_DATA* Packet, FFBEType* Effect)
        //If valid Packet was found then returns ERROR_SUCCESS and sets the new Effect type
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        return Ffb_h_CreateNewEffect(Packet, Effect, nullptr);
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_DevCtrl(const FFB_DATA* Packet, FFB_CTRL* Control)
        //If valid Packet was found then returns ERROR_SUCCESS and gets enum Control - this enum holds PID Device Control (Enable Actuators, Device Reset etc).
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <10)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_CTRLREP)
            return ERROR_INVALID_DATA;

        *Control = (FFB_CTRL)Packet->data[1];
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_DevGain(const FFB_DATA* Packet, BYTE* Gain)
        //If valid Packet was found then returns ERROR_SUCCESS and gets the device global gain.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <10)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_GAINREP)
            return ERROR_INVALID_DATA;

        *Gain = Packet->data[1];
        return ERROR_SUCCESS;

    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Eff_Period(const FFB_DATA* Packet, FFB_EFF_PERIOD* Effect)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Effect - this structure holds Effect Block Index, Magnitude, Offset, Phase and period.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <20)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_PRIDREP)
            return ERROR_INVALID_DATA;

        Effect->EffectBlockIndex = Packet->data[1];
        Effect->Magnitude = (DWORD)((Packet->data[3] << 8) + (Packet->data[2]));
        Effect->Offset = (LONG)((Packet->data[5] << 8) + (Packet->data[4]));
        Effect->Phase = (DWORD)((Packet->data[7] << 8) + (Packet->data[6]));
        Effect->Period = (DWORD)((Packet->data[11] << 24) + (Packet->data[10] << 16) + (Packet->data[9] << 8) + (Packet->data[8]));
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Eff_Cond(const FFB_DATA* Packet, FFB_EFF_COND* Condition)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Condition - this structure holds Effect Block Index, Direction (X/Y), Center Point Offset, Dead Band and other conditions.
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <23)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_CONDREP)
            return ERROR_INVALID_DATA;

        Condition->EffectBlockIndex = Packet->data[1];
        Condition->isY = Packet->data[2];
        Condition->CenterPointOffset = (LONG)((Packet->data[4] << 8) + (Packet->data[3]));
        Condition->PosCoeff = (LONG)((Packet->data[6] << 8) + (Packet->data[5]));
        Condition->NegCoeff = (LONG)((Packet->data[8] << 8) + (Packet->data[7]));
        Condition->PosSatur = (DWORD)((Packet->data[10] << 8) + (Packet->data[9]));
        Condition->NegSatur = (DWORD)((Packet->data[12] << 8) + (Packet->data[11]));
        Condition->DeadBand = (LONG)((Packet->data[14] << 8) + (Packet->data[13]));
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD __cdecl Ffb_h_Eff_Envlp(const FFB_DATA* Packet, FFB_EFF_ENVLP* Envelope)
        //If valid Packet was found then returns ERROR_SUCCESS and fills structure Envelope -
        //If Packet is NULL then returns ERROR_INVALID_PARAMETER. Output parameters are undefined.
        //If Packet is malformed  then returns ERROR_INVALID_DATA. Output parameters are undefined.
    {
        // Routine validity checks
        if (!Packet)
            return ERROR_INVALID_PARAMETER;
        if (Packet->size <22)
            return ERROR_INVALID_DATA;

        // Some types don't carry Effect Block Index
        FFBPType Type;
        if (Ffb_h_Type(Packet, &Type) != ERROR_SUCCESS)
            return ERROR_INVALID_DATA;
        if (Type != PT_ENVREP)
            return ERROR_INVALID_DATA;

        Envelope->EffectBlockIndex = Packet->data[1];
        Envelope->AttackLevel = (DWORD)((Packet->data[3] << 8) + (Packet->data[2]));
        Envelope->FadeLevel = (DWORD)((Packet->data[5] << 8) + (Packet->data[4]));
        Envelope->AttackTime = (DWORD)((Packet->data[9] << 24) + (Packet->data[8] << 16) + (Packet->data[7] << 8) + (Packet->data[6]));
        Envelope->FadeTime = (DWORD)((Packet->data[13] << 24) + (Packet->data[12] << 16) + (Packet->data[11] << 8) + (Packet->data[10]));
        return ERROR_SUCCESS;
    }

    VJOYINTERFACE_API DWORD		__cdecl FfbWritePID(UINT rID, FFB_DEVICE_PID* PIDData)
        // Update the PID Block load of the specified vJoy Device.
    {
        PVOID pData = PIDData;
        if (!pData || (vJoyDevices.find(rID) == vJoyDevices.end()) || Get_h(rID) == INVALID_HANDLE_VALUE || Get_stat(rID) != VJD_STAT_OWN)
            return FALSE;

        UINT	IoCode = SET_FFB_PID_DATA;
        UINT	IoSize = sizeof(FFB_DEVICE_PID);
        ULONG	bytes;
        HANDLE	hIoctlEvent;
        OVERLAPPED OverLapped = { 0 };

        // Preparing
        hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
        memset(&OverLapped, 0, sizeof(OVERLAPPED));
        OverLapped.hEvent = hIoctlEvent;

        // Send FFB_DEVICE_PID structure to vJoy device
        BOOL res = DeviceIoControl(Get_h(rID), IoCode, pData, IoSize, NULL, 0, &bytes, &OverLapped);

        // Test Results
        if (!res) {
            // The transaction failed.
            // If it is just because it is pending then wait otherwise it is an error
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING)
                res = FALSE;
            else {	// Wait for write to complete
                DWORD WaitRet = WaitForSingleObject(OverLapped.hEvent, 500);
                if (WAIT_OBJECT_0 == WaitRet)
                    res = TRUE;
                else
                    res = FALSE;
            }
        }
        CloseHandle(OverLapped.hEvent);
        return res;
    }

    VJOYINTERFACE_API DWORD		__cdecl FfbReadPID(UINT rID, FFB_DEVICE_PID* PIDData)
        // Update the PID Block load of the specified vJoy Device.
    {
        PVOID pData = PIDData;
        if (!pData || (vJoyDevices.find(rID) == vJoyDevices.end()) || Get_h(rID) == INVALID_HANDLE_VALUE || Get_stat(rID) != VJD_STAT_OWN)
            return FALSE;

        UINT	IoCode = GET_FFB_PID_DATA;
        UINT	IoSize = sizeof(FFB_DEVICE_PID);
        ULONG	bytes;
        HANDLE	hIoctlEvent;
        OVERLAPPED OverLapped = { 0 };

        // Preparing
        hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
        memset(&OverLapped, 0, sizeof(OVERLAPPED));
        OverLapped.hEvent = hIoctlEvent;

        // Get FFB_DEVICE_PID structure from vJoy device
        BOOL res = DeviceIoControl(Get_h(rID), IoCode, NULL, 0, pData, IoSize, &bytes, &OverLapped);
        // immediate Return
        if (res) {
            CloseHandle(OverLapped.hEvent);
            if (bytes) {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Info: Ffb_h_ReadPID() - Returns (Immediatly) TRUE"), ProcessId);
                return TRUE;
            } else {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: Ffb_h_ReadPID() - Returns (Immediatly) FALSE"), ProcessId);
                return FALSE;
            }
        }

        // Delayed/Error
        else {
            // Error getting the data
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: Ffb_h_ReadPID() - error (0x%X) Returns FALSE"), ProcessId, err);
                CloseHandle(OverLapped.hEvent);
                return FALSE;
            }

            // Wait until data ready
            DWORD nBytesTranss = 0;
            BOOL gotdata = GetOverlappedResult(Get_h(rID), &OverLapped, &nBytesTranss, TRUE);
            CloseHandle(OverLapped.hEvent);

            // Data received and it is not empty
            if (gotdata && nBytesTranss) {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Info: Ffb_h_ReadPID() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
                return TRUE;
            } else {
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Info: Ffb_h_ReadPID() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
                return FALSE;
            }
        }

        return res;
    }

    VJOYINTERFACE_API DWORD __cdecl FfbUpdateEffectState(UINT rID, UINT effectId, UINT effectState)
    // Update the Ffb state report (bitfield) of the specified effect in given vJoy Device.
    {
        FFB_DEVICE_PID PIDBlockLoad;
        if (effectId<VJOY_FFB_FIRST_EFFECT_ID || effectId>VJOY_FFB_MAX_EFFECTS_BLOCK_INDEX)
            return ERROR_INVALID_PARAMETER;

        DWORD stt = FfbReadPID(rID, &PIDBlockLoad);
        if (stt != ERROR_SUCCESS) return stt;

        PIDBlockLoad.EffectStates[effectId-1].PIDEffectStateReport = (BYTE)effectState;
        
        stt = FfbWritePID(rID, &PIDBlockLoad);
        if (stt != ERROR_SUCCESS) return stt;

        return ERROR_SUCCESS;
    }

#pragma endregion

#ifndef STATIC
} // extern "C"
#else
}// Namespace vJoyNS
#endif


//VJOYINTERFACE_API BOOL		__cdecl	RegisterDisableCB(CallbackType pFuncDisable, PVOID pParam)
//{
//	pfDisable = pFuncDisable;
//	ppDisable = pParam;
//	return TRUE;
//}
//
//VJOYINTERFACE_API BOOL		__cdecl	RegisterEnableCB(CallbackType pFuncEnable, PVOID pParam)
//{
//	pfEnable = pFuncEnable;
//	ppEnable = pParam;
//	return TRUE;
//}
//VJOYINTERFACE_API VOID		__cdecl	UnRegisterDisableCB(VOID)
//{
//	pfDisable = NULL;
//	ppDisable = NULL;
//}
//
//VJOYINTERFACE_API VOID		__cdecl	UnRegisterEnableCB(VOID)
//{
//	pfEnable = NULL;
//	ppEnable = NULL;
//}
//
//
///////////////// Helper Functions ////////////////////////////////////////////
HANDLE	GetHandleByIndex(int index)
{
    /*
        Given index (Zero-based) this function returns a handle to
        the corresponding HID device (not necessarily vJoy)
        if index is out of range it return NULL
    */

    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: GetHandleByIndex(index=%d) - Starting"), ProcessId, index);

    // Get  the device interfaceGUID for HIDClass devices.
    GUID HidGuid;
    HidD_GetHidGuid(&HidGuid);

    //
    // Open a handle to the plug and play dev node.
    HDEVINFO hardwareDeviceInfo = SetupDiGetClassDevs(&HidGuid,
        NULL, // Define no enumerator (global)
        NULL, // Define no
        (DIGCF_PRESENT | // Only Devices present
            DIGCF_DEVICEINTERFACE)); // Function class devices.

    if (INVALID_HANDLE_VALUE == hardwareDeviceInfo) {
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: GetHandleByIndex(index=%d) - Failed SetupDiGetClassDevs()"), ProcessId, index);
        return NULL;
    }


    DWORD lasterror = 0;
    SP_DEVICE_INTERFACE_DATA            deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    BOOL isOK;

    // Get Interface data for Device[index]
    isOK = SetupDiEnumDeviceInterfaces(hardwareDeviceInfo,
        0, // No care about specific PDOs
        &HidGuid,
        index, // Running index
        &deviceInfoData);

    // Return NULL if there are no more device interfaces
    // Return INVALID_HANDLE_VALUE for all other failures
    if (!isOK) {
        lasterror = GetLastError();
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Warning: GetHandleByIndex(index=%d) - Failed SetupDiEnumDeviceInterfaces() with error 0x%x"), ProcessId, index, lasterror);
        if (lasterror == ERROR_NO_MORE_ITEMS) // 0x103
            return NULL;
        else
            return INVALID_HANDLE_VALUE;	 //
    }

    // allocate a function class device data structure to receive the
    // goods about this particular device.
    DWORD requiredLength = 16000;
#if 0			  // I assume 16K buffer will be enough
    BOOL bRes = SetupDiGetDeviceInterfaceDetail(
        hardwareDeviceInfo,
        &deviceInfoData,
        NULL, // probing so no output buffer yet
        0, // probing so output buffer length of zero
        &requiredLength,
        NULL); // not interested in the specific dev-node

    DWORD  err = GetLastError();

    if ((err != ERROR_INSUFFICIENT_BUFFER) || !requiredLength) {
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: GetHandleByIndex(index=%d) - Failed SetupDiGetDeviceInterfaceDetail() with requiredLength=0"), ProcessId, index);
        return INVALID_HANDLE_VALUE;
    }

#endif // 0			  // I assume 16K buffer will be enough

    // Retrieve the information from Plug and Play.
    //  First, prepare the output buffer
    DWORD predictedLength = requiredLength;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    functionClassDeviceData = NULL;
    functionClassDeviceData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, predictedLength);
    if (functionClassDeviceData) {
        functionClassDeviceData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        ZeroMemory(functionClassDeviceData->DevicePath, sizeof(functionClassDeviceData->DevicePath));
    } else {
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: GetHandleByIndex(index=%d) - Failed to allocate functionClassDeviceData"), ProcessId, index);
        LocalFree(functionClassDeviceData);
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
        return INVALID_HANDLE_VALUE;
    }

    //  Now, get the data into 'functionClassDeviceData'
    if (!SetupDiGetDeviceInterfaceDetail(
        hardwareDeviceInfo,
        &deviceInfoData,
        functionClassDeviceData,
        predictedLength,
        &requiredLength,
        NULL)) {
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: GetHandleByIndex(index=%d) - Failed SetupDiGetDeviceInterfaceDetail() [2]"), ProcessId, index);
        LocalFree(functionClassDeviceData);
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
        return INVALID_HANDLE_VALUE;
    }

    // Get a handle to the device
    HANDLE HidDevice = CreateFile(functionClassDeviceData->DevicePath,
        NULL,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,        // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
        0,   // Open device as non-overlapped so we can get data
        NULL);       // No template file
    if (INVALID_HANDLE_VALUE == HidDevice) {
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: GetHandleByIndex(index=%d) - Failed to CreateFile(%s)"), ProcessId, index, functionClassDeviceData->DevicePath);
        LocalFree(functionClassDeviceData);
        SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
        CloseHandle(HidDevice);
        return INVALID_HANDLE_VALUE;
    }


    // Cleanup
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: GetHandleByIndex(index=%d) - Exit OK (Handle to %s)"), ProcessId, index, functionClassDeviceData->DevicePath);
    LocalFree(functionClassDeviceData);
    SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);


    return HidDevice;

}

//int		GetDeviceIndexByVendorId(USHORT VendorId, int BaseIndex)
//{
///*
//	This function searches for an HID device by Vendor ID
//	Retuns the index of the first devich whose index is equal or higher
//	than  BaseIndex.
//	If not found, returns a negative number
//*/
//	HANDLE h;
//	int i=BaseIndex;
//	HIDD_ATTRIBUTES Attributes;
//	while (h = GetHandleByIndex(i++))
//	{
//		HidD_GetAttributes(h, &Attributes);
//		CloseHandle(h);
//		if (Attributes.VendorID == VendorId)
//			return i-1;
//	};
//
//	return -1;
//}
int		GetDeviceIndexById(USHORT VendorId, USHORT ProductId, int BaseIndex)
{
    /*
        This function searches for an HID device by Vendor and Product ID
        Retuns the index of the first device whose index is equal or higher
        than  BaseIndex.
        If not found, returns a negative number
    */
    HANDLE h;
    int i = BaseIndex;
    HIDD_ATTRIBUTES Attributes;
    int iFound = -1;
    ZeroMemory(&Attributes, sizeof(HIDD_ATTRIBUTES));
    Attributes.Size = sizeof(HIDD_ATTRIBUTES);

    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceIndexById(BaseIndex=%d) - Starting"), ProcessId, BaseIndex);

    while (h = GetHandleByIndex(i++)) {
        if (h == INVALID_HANDLE_VALUE)
            continue;
        BOOL gotit = HidD_GetAttributes(h, &Attributes);
        CloseHandle(h);
        if (gotit == TRUE)
            if ((Attributes.VendorID == VendorId) && (Attributes.ProductID == ProductId) && (iFound == -1))
                iFound = i - 1;
        if (LogStream && (gotit == TRUE))
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceIndexById(BaseIndex=%d) - index=%d; VendorId=%x; ProductId=%x; VersionNumber=%x"), ProcessId, BaseIndex, i, Attributes.VendorID, Attributes.ProductID, Attributes.VersionNumber);
    }

    //CloseHandle(h);
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceIndexById(BaseIndex=%d) - Returning %d"), ProcessId, BaseIndex, iFound);
    return iFound;
}

int		GetDeviceIndexByReportId(USHORT VendorId, USHORT ProductId, BYTE ReportId)
{
    /*
        This function searches for an HID device by Vendor and Product ID then by Report ID
        Retuns the index of the first device that matches
        If not found, returns a negative number
    */
    int DevIndex;
    int i = 0;
    int id;

    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceIndexByReportId(%u) - Starting"), ProcessId, ReportId);

    // Get the index of the next vJoy device
    while ((DevIndex = GetDeviceIndexById(VendorId, ProductId, i++)) >= 0) {
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceIndexByReportId(%u) - GetDeviceIndexById(i=%d) => %d"), ProcessId, ReportId, i, DevIndex);

        // Get the id of this vJoy device
        id = GetvJoyReportId(DevIndex);
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceIndexByReportId(%u) - GetvJoyReportId(DevIndex=%d) => %d"), ProcessId, ReportId, DevIndex, id);


        // If this is the correct id then return the index
        if (id == ReportId)
            return DevIndex;
    };

    // No match
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDeviceIndexByReportId(%u) - No match"), ProcessId, ReportId);
    return -1;
}

BOOL	GetDeviceProductString(int Index, PWSTR* pProductString)
/*
    This function returns TRUE is it succeeds
    and puts Product String in pProductString
*/
{
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h==INVALID_HANDLE_VALUE)
        return FALSE;

    PVOID Buffer = calloc(130, sizeof(USHORT));
    if (!Buffer) {
        CloseHandle(h);
        return FALSE;
    }

    if (TRUE != HidD_GetProductString(h, Buffer, 130)) {
        CloseHandle(h);
        return FALSE;
    }

    *pProductString = _wcsdup((PWSTR)Buffer);
    free(Buffer);
    CloseHandle(h);

    return TRUE;
}


BOOL	GetDeviceManufacturerString(int Index, PWSTR* pManufacturerString)
/*
    This function returns TRUE is it succeeds
    and puts Manufacturer String in pManufacturerString
*/
{
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h == INVALID_HANDLE_VALUE)
        return FALSE;

    WCHAR Buffer[130] = { 0 };
    if (!Buffer) {
        CloseHandle(h);
        return FALSE;
    }

    if (TRUE != HidD_GetManufacturerString(h, Buffer, sizeof(Buffer))) {
        CloseHandle(h);
        return FALSE;
    }

    *pManufacturerString = _wcsdup((PWSTR)Buffer);
    CloseHandle(h);

    return TRUE;
}


BOOL	GetDeviceSerialNumberString(int Index, PWSTR* pSerialNumberString)
/*
    This function returns TRUE is it succeeds
    and puts Serial Number String in pManufacturerString
*/
{
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h==INVALID_HANDLE_VALUE)
        return FALSE;

    PVOID Buffer = calloc(130, sizeof(USHORT));
    if (!Buffer || (TRUE != HidD_GetSerialNumberString(h, Buffer, 130))) {
        CloseHandle(h);
        return FALSE;
    }

    *pSerialNumberString = _wcsdup((PWSTR)Buffer);
    free(Buffer);
    CloseHandle(h);

    return TRUE;
}


BOOL	GetDeviceVersionNumber(int Index, PUSHORT version)
{
    /*
        This function returns TRUE is it succeeds
        and puts Version number in VersionNumber
    */
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h==INVALID_HANDLE_VALUE)
        return FALSE;

    HIDD_ATTRIBUTES Attributes;
    ZeroMemory(&Attributes, sizeof(Attributes));

    if (TRUE != HidD_GetAttributes(h, &Attributes)) {
        CloseHandle(h);
        return FALSE;
    }

    *version = Attributes.VersionNumber;
    CloseHandle(h);

    return TRUE;
}

BOOL	GetDeviceAttributes(int Index, PUSHORT vendorID, PUSHORT ProductID, PUSHORT version)
{
    /*
        This function returns TRUE is it succeeds
        and puts Version number in VersionNumber
    */
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h==INVALID_HANDLE_VALUE)
        return FALSE;

    HIDD_ATTRIBUTES Attributes;
    ZeroMemory(&Attributes, sizeof(Attributes));
    if (TRUE != HidD_GetAttributes(h, &Attributes)) {
        CloseHandle(h);
        return FALSE;
    }

    *version = Attributes.VersionNumber;
    *vendorID = Attributes.VendorID;
    *ProductID = Attributes.ProductID;

    CloseHandle(h);

    return TRUE;
}


int		GetvJoyReportId(int Index)
{
    /*
        This function returns Report ID if succeeds
        or negative number if fails
    */
    int rID = 0;

    HANDLE h = GetHandleByIndex(Index);
    if (!h || h==INVALID_HANDLE_VALUE)
        return NO_HANDLE_BY_INDEX;

    NTSTATUS stat = HIDP_STATUS_SUCCESS;
    PHIDP_PREPARSED_DATA PreparsedData = NULL;
    HIDP_CAPS Capabilities;
    ZeroMemory(&Capabilities, sizeof(Capabilities));
    BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
    if (TRUE != ok) {
        CloseHandle(h);
        return BAD_PREPARSED_DATA;
    }
    stat = HidP_GetCaps(PreparsedData, &Capabilities);
    if (stat != HIDP_STATUS_SUCCESS) {
        CloseHandle(h);
        return NO_CAPS;
    }

    // Get axes date
    USHORT nVal = Capabilities.NumberInputValueCaps;
    if (nVal >= 1) {
        PHIDP_VALUE_CAPS bVals = new HIDP_VALUE_CAPS[nVal*sizeof(HIDP_VALUE_CAPS)];
        stat = HidP_GetValueCaps(HidP_Input, bVals, &nVal, PreparsedData);
        if ((stat == HIDP_STATUS_SUCCESS) && nVal) {
            //HidD_FreePreparsedData(PreparsedData);
            CloseHandle(h);
            rID = bVals[0].ReportID;
            delete[](bVals);
            return rID;
        }
    }

    // Get Button data
    int ButtonBaseIndex, nButtons = 0;
    USHORT n = Capabilities.NumberInputButtonCaps;
    PHIDP_BUTTON_CAPS bCaps = new HIDP_BUTTON_CAPS[n*sizeof(HIDP_BUTTON_CAPS)];
    stat = HidP_GetButtonCaps(HidP_Input, bCaps, &n, PreparsedData);
    // Assuming one button range, get the number of buttons
    if ((stat==HIDP_STATUS_SUCCESS) && n && (bCaps[0]).IsRange) {
        nButtons += ((bCaps[0]).Range).UsageMax - ((bCaps[0]).Range).UsageMin + 1;
        ButtonBaseIndex = ((bCaps[0]).Range).DataIndexMin;
        rID = bCaps[0].ReportID;
    }

    //HidD_FreePreparsedData(PreparsedData);
    CloseHandle(h);

    delete[](bCaps);
    return rID;
}



BOOL	GetDeviceNameSpace(char** NameSpace, int* Size, BOOL Refresh, DWORD* error)
{
    /*
        This function discoves the PNP-allocated namespace for the raw-PDO
        We do it by discovering one (no matter which) interface of vJoy
        and extracting the namespace from the interface string

        Parameters:
            NameSpace[out] -		Pointer to pointer to the namespace string. The string is allocated by this function
                                and is kept as a static variable for further use.
                                If NULL then this function only check that a namespace exists
            Size[out] -			Pointer to integer holding the size (in char) of the namespace string
            Refresh[in] -		If TRUE the recalculate Namespace (Ignored when namespace was not calculated)
            error[out] -			Error value
    */

    // Init
    static char* StatNS = NULL; // Namespace
    BOOL out = FALSE;
    if (error)
        *error = 0;
    if (Size)
        *Size = 0;

    HDEVINFO                            hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 16000, bytes = 0;
    HANDLE                              file;
    ULONG                               i = 0;
    char* DevPath = NULL;

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    // Namespace already known?
    if (StatNS && !Refresh && NameSpace) {
        *NameSpace = StatNS;
        *Size = strlen(StatNS) + 1;
        return true;
    }

    //
    // Open a handle to the device interface information set of class interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs(
        (LPGUID)&GUID_DEVINTERFACE_VJOY,
        NULL, // Define no enumerator (global)
        NULL, // Define no
        (DIGCF_PRESENT | // Only Devices present
            DIGCF_DEVICEINTERFACE)); // Function class devices.
    if (INVALID_HANDLE_VALUE == hardwareDeviceInfo) {
        if (error)
            *error = GetLastError();
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDeviceNameSpace() - Failed SetupDiGetClassDevs() with %x"), ProcessId, *error);
        return FALSE; // INVALID_HANDLE_VALUE;
    }

    // Enumerate devices of HID class
    if (SetupDiEnumDeviceInterfaces(hardwareDeviceInfo,
        NULL, // No care about specific PDOs
        (LPGUID)&GUID_DEVINTERFACE_VJOY,
        i, //
        &deviceInterfaceData)) {

        if (deviceInterfaceDetailData)
            deviceInterfaceDetailData = NULL;

        //
        // Allocate a function class device data structure to
        // receive the information about this particular device.
        //
        //
#if 0
                        // First find out required length of the buffer
        //
        if (!SetupDiGetDeviceInterfaceDetail(
            hardwareDeviceInfo,
            &deviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL)) { // not interested in the specific dev-node
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
                if (error)
                    *error = GetLastError();
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDeviceNameSpace() - Failed SetupDiGetDeviceInterfaceDetail() with error %x"), ProcessId, *error);
                SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
                return FALSE; // INVALID_HANDLE_VALUE;
            }
        }  // SetupDiGetDeviceInterfaceDetail

#endif // 0

        predictedLength = requiredLength;
        deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, predictedLength);

        if (deviceInterfaceDetailData) {
            deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            ZeroMemory(deviceInterfaceDetailData->DevicePath, sizeof(deviceInterfaceDetailData->DevicePath));
        } else {
            if (error)
                *error = GetLastError();
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDeviceNameSpace() - Failed to allocate memory for PSP_DEVICE_INTERFACE_DETAIL_DATA"), ProcessId);
            LocalFree(deviceInterfaceDetailData);
            SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
            return FALSE; // INVALID_HANDLE_VALUE;
        }

        if (!SetupDiGetDeviceInterfaceDetail(
            hardwareDeviceInfo,
            &deviceInterfaceData,
            deviceInterfaceDetailData,
            predictedLength,
            &requiredLength,
            NULL)) {
            if (error) {
                *error = GetLastError();
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDeviceNameSpace() - Failed SetupDiGetDeviceInterfaceDetail() with error %x"), ProcessId, *error);
            }

            SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
            LocalFree(deviceInterfaceDetailData);
            return FALSE; // INVALID_HANDLE_VALUE;
        }

        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceNameSpace() - DevicePath=%s"), ProcessId, deviceInterfaceDetailData->DevicePath);

        // Now we have the device path of one interface

        // Get required size for namespace
        int DestSize = 0;
        bool ok = ExtractNamespace(deviceInterfaceDetailData->DevicePath, NULL, &DestSize);
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceNameSpace() - Required size for namespace=%d"), ProcessId, DestSize);

        // Allocate string
        StatNS = new char[DestSize+1];
        ok = ExtractNamespace(deviceInterfaceDetailData->DevicePath, StatNS, &DestSize);
        LocalFree(deviceInterfaceDetailData);
        deviceInterfaceDetailData = NULL;
        if (ok) {
            if (NameSpace)
                *NameSpace = StatNS;
            if (Size)
                *Size = DestSize;
            return TRUE;
        } else
            return FALSE;

    } //  SetupDiEnumDeviceInterfaces <<
    else {
        if (error)
            *error = GetLastError();
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDeviceNameSpace() - SetupDiEnumDeviceInterfaces() interface is not a vJoy interface"), ProcessId);
        LocalFree(deviceInterfaceDetailData);
        deviceInterfaceDetailData = NULL;
        return FALSE; //
    }

    return FALSE;

}


HANDLE	OpenDeviceInterface(UINT iInterFace, DWORD* error)
{
    /*
        Open a vJoy device interface by Report ID (1-16)
        If vJoy installed AND the specific report ID is configured AND is not already open
        - this function will return a handle to the corresponding device

        If not able to open a device the function will return INVALID_HANDLE_VALUE
        and place error code in parameter error

        If iInterface==0 then just extract the namespace and use it as a basis to the pathname

    */

    int Size;
    char* NameSpace = NULL;

    BOOL ok = GetDeviceNameSpace(&NameSpace, &Size, FALSE, error);
    if (!ok)
        return 	INVALID_HANDLE_VALUE;
    Size += wcslen(VJOY_INTERFACE);
    Size += 4;
    char* DevPath = new char[Size];
    CreateDevicePath(NameSpace, iInterFace, DevPath, Size);


    ///// Open vJoy Raw device
    HANDLE file = CreateFile(DevPath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
        FILE_FLAG_OVERLAPPED, // Overlapped read/write
        NULL);

    if (INVALID_HANDLE_VALUE == file) {
        CloseHandle(file);
        if (error)
            *error = GetLastError();
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: OpenDeviceInterface(%u) - Failed CreateFile() with error %x"), ProcessId, iInterFace, *error);
        return INVALID_HANDLE_VALUE;
    }

    // Special Case - device is #0
    if (iInterFace == 0)
        return file;

    // Test that the interface is functional and represents the right device
    DEV_INFO info = { 0 };
    bool info_ok = GetDevInfo(file, (PVOID)&info);
    if (!info_ok || info.DeviceID != iInterFace) {
        CloseHandle(file);
        if (error)
            *error = GetLastError();
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Error: OpenDeviceInterface(%u) - Failed GetDevInfo()"), ProcessId, iInterFace);
        return INVALID_HANDLE_VALUE;
    }
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: OpenDeviceInterface(%u) - GetDevInfo() returned successfully for device %u"), ProcessId, iInterFace, info.DeviceID);


    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: OpenDeviceInterface(%u) - Exit OK"), ProcessId, iInterFace);
    return file;
}


// Get the handle to the General Control (Device #0)
// If already exists the return it.
// If does not exist the create and return it
// If cannot be obtained then return Invalid Handle	(INVALID_HANDLE_VALUE)
HANDLE	GetGenControlHadle(void)
{
    // Already exists and valid
    if (h0 && h0 != INVALID_HANDLE_VALUE)
        return h0;

    InitDll();

    h0 = OpenDeviceInterface(0, NULL);
    if (h0 == INVALID_HANDLE_VALUE)
        hDeviceNotifyHandle0 = NULL;
    else
        hDeviceNotifyHandle0 = RegisterHandleNotification(hWnd, h0);

    return h0;
}

// 	Get information of a vJoy device from the raw PDO interface
//	The information includes:
//		Device ID
//		Number of Implemented devices(1 - 16)
//		Device Implemented(FALSE / TRUE)
//		Max possible number of devices that the driver supports(16)
//		Is device FFB Capable(FALSE)
//		Is driver FFB Capable(FALSE)
//	Return Value:
//		True if values are valid
//		False if values are invalid (older drivers)
bool	GetDevInfo(HANDLE h, PVOID data)
{
    ULONG		bytes = 0;
    DEV_INFO* info;
    int			BufferSize = sizeof(DEV_INFO);
    HANDLE	hIoctlEvent;
    OVERLAPPED OverLapped = { 0 };

    if (!data)
        return false;
    SecureZeroMemory(data, sizeof(DEV_INFO));
    info = (DEV_INFO*)data;

    // Preparing
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;
    OverLapped.Internal = STATUS_PENDING;

    // Test  FFB queues in the device
    BOOL ok = DeviceIoControl(h, GET_DEV_INFO, NULL, NULL, data, BufferSize, &bytes, &OverLapped);

    // Imedeate Return
    if (ok) {
        CloseHandle(OverLapped.hEvent);
        if (bytes) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevInfo() - Returns (Immediatly) TRUE: ID=%u Implemented=%u isImplemented=%x %x %x %x"), ProcessId, info->DeviceID, info->nImplemented, info->isImplemented, info->MaxDevices, info->DriverFFB, info->DeviceFFB);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDevInfo() - Returns (Immediatly) FALSE"), ProcessId);
            return false;
        }
    }

    // Delayed/Error
    else {
        // Error getting the data
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDevInfo() - error (0x%X) Returns FALSE"), ProcessId, err);
            CloseHandle(OverLapped.hEvent);
            return false;
        }

        // Wait until data ready
        DWORD nBytesTranss = 0;
        BOOL gotdata = GetOverlappedResult(h, &OverLapped, &nBytesTranss, TRUE);
        CloseHandle(OverLapped.hEvent);
        if (gotdata && nBytesTranss) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevInfo() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevInfo() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
            return false;
        }
    }
}

// Get device status for a given device
// If id is valid and driver responds the this function returns true with valid data and data size
// Parameters:
//	id[in]:			ID of target device (0<id<max device)
//	nbytes[in/out]:	[in] Size in bytes of output data buffer / [out] Size of data in output buffer
//	buffer[out]:	Output data buffer
bool	GetDevStat(BYTE id, int* nbytes, BYTE* buffer)
{
    ULONG		bytes = 0;
    HANDLE		hIoctlEvent;
    OVERLAPPED	OverLapped = { 0 };
    PVOID		data = NULL;
    // Define required output buffer size
    int ReqBuffSize = 5;

    // Sanity checks
    if (id<1 || id> VJOY_MAX_N_DEVICES || !buffer || !nbytes || (*nbytes)<ReqBuffSize)
        return false;


    // Preparing
    data = (PVOID)buffer;
    SecureZeroMemory(data, *nbytes);
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;
    OverLapped.Internal = STATUS_PENDING;

    // Test  FFB queues in the device
    BOOL ok = DeviceIoControl(GetGenControlHadle(), GET_DEV_STAT, (LPVOID)(&id), sizeof(BYTE), data, *nbytes, &bytes, &OverLapped);

    // Imedeate Return
    if (ok) {
        CloseHandle(OverLapped.hEvent);
        if (bytes) {
            *nbytes = bytes;
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevStat() - Returns (Immediatly) TRUE: ID=%u ; output bytes=%u ; data = %x"), ProcessId, id, bytes, *(UINT*)buffer);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDevStat() - Returns (Immediatly) FALSE"), ProcessId);
            return false;
        }
    }

    // Delayed/Error
    else {
        // Error getting the data
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDevStat() - error (0x%X) Returns FALSE"), ProcessId, err);
            CloseHandle(OverLapped.hEvent);
            return false;
        }

        // Wait until data ready
        DWORD nBytesTranss = 0;
        BOOL gotdata = GetOverlappedResult(GetGenControlHadle(), &OverLapped, &nBytesTranss, TRUE);
        CloseHandle(OverLapped.hEvent);
        if (gotdata && nBytesTranss) {
            *nbytes = bytes;
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevStat() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevStat() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
            return false;
        }
    }

}

/*
    Get Driver status - NOT device specific
    Returns general information regarding driver capabilities
    nbytes[in/out]:	[in] Size in bytes of output data buffer / [out] Size of data in output buffer
    buffer[out]:	Output data buffer
*/
bool	GetDrvStat(int* nbytes, BYTE* buffer)
{
    ULONG		bytes = 0;
    HANDLE		hIoctlEvent;
    OVERLAPPED	OverLapped = { 0 };
    PVOID		data = NULL;

    // Define required output buffer size
    int ReqBuffSize = 4;

    // Preparing
    data = (PVOID)buffer;
    SecureZeroMemory(data, *nbytes);
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;
    OverLapped.Internal = STATUS_PENDING;

    // Test  FFB queues in the device
    BOOL ok = DeviceIoControl(GetGenControlHadle(), GET_DRV_INFO, NULL, 0, data, *nbytes, &bytes, &OverLapped);

    // Imedeate Return
    if (ok) {
        CloseHandle(OverLapped.hEvent);
        if (bytes) {
            *nbytes = bytes;
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDrvStat() - Returns (Immediatly) TRUE; output bytes=%u ; data = %x"), ProcessId, bytes, *(UINT*)buffer);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDrvStat() - Returns (Immediatly) FALSE"), ProcessId);
            return false;
        }
    }

    // Delayed/Error
    else {
        // Error getting the data
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDrvStat() - error (0x%X) Returns FALSE"), ProcessId, err);
            CloseHandle(OverLapped.hEvent);
            return false;
        }

        // Wait until data ready
        DWORD nBytesTranss = 0;
        BOOL gotdata = GetOverlappedResult(GetGenControlHadle(), &OverLapped, &nBytesTranss, TRUE);
        CloseHandle(OverLapped.hEvent);
        if (gotdata && nBytesTranss) {
            *nbytes = bytes;
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDrvStat() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDrvStat() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
            return false;
        }
    }

}
/*
    ExtractNamespace extracts the namespace of the requested device from a given Device Path (SrcDevicePath)
    If DestDevicePath points to NULL the size to on namespace string is set in DestSize
    If DestDevicePath points to to a valid buffer then DestSize points to the size of the buffer.
    If the buffer is not large enough to hold the namespace string the function returns false
    Else, the namespace is put into the buffer (DestDevicePath)	and the function returns true
*/

// TODO: Remove this pragma and solve the worning
#pragma warning( push )
#pragma warning( disable : 4996 )

bool ExtractNamespace(const char* SrcDevicePath, char* DestDevicePath, int* DestSize)
{
    // Sanity checks
    if (!SrcDevicePath || !DestSize)
        return false;

    // Convert "Device_" from UNICODE to array of lower-case char
    char cVjoy_Interface[100];
    wcstombs(cVjoy_Interface, VJOY_INTERFACE, 50);	// Convert wide-character string to multibyte string
    _strlwr_s(cVjoy_Interface, strlen(cVjoy_Interface) + 1);  // Converts a string to lowercase

    // Getting the size of the 	base (namespace) from DevicePath
    int size = strrchr(SrcDevicePath, '\\') - SrcDevicePath + 1;
    if (!size)
        return false;

    // If output buffer is NULL just report size and exit
    if (!DestDevicePath) {
        *DestSize = size;
        return true;
    }

    // If output buffer is too small return false
    if (size<*DestSize)
        return false;

    // Extract the base (namespace) from temporary string
    if (*DestSize < size)
        return false;
    strncpy(DestDevicePath, SrcDevicePath, size);
    DestDevicePath[size] = '\0';
    return 		true;
}

/*
     CreateDevicePath create a path to the requested device from a given Device Path (SrcDevicePath)
     It extracts the namespace from the input DevicePath then appends a string based on Index
     (Example: Creates "Device_000" from Index==0)
*/
bool CreateDevicePath(const char* SrcDevicePath, int Index, char* DestDevicePath, int DestSize)
{
    // Sanity checks
    if (Index < 0 || !SrcDevicePath)
        return false;

    // Convert "Device_" from UNICODE to array of lower-case char
    char cVjoy_Interface[100];
    wcstombs(cVjoy_Interface, VJOY_INTERFACE, 50);	// Convert wide-character string to multibyte string
    _strlwr_s(cVjoy_Interface, strlen(cVjoy_Interface) + 1);  // Converts a string to lowercase

    // Getting the size of the 	base (namespace) from DevicePath
    int size = strrchr(SrcDevicePath, '\\') - SrcDevicePath + 1;
    if (!size)
        return false;

    // Extract the base (namespace) from temporary string
    if (DestSize< (size + strlen(cVjoy_Interface) /*+ 1*/))
        return false;
    char* cTmp = new char[DestSize];
    strncpy(cTmp, SrcDevicePath, size);
    cTmp[size] = '\0';

    // Now create the output string
    sprintf_s(DestDevicePath, DestSize, "%s%s%03d", cTmp, cVjoy_Interface, Index);
    delete[](cTmp);
    return 		true;
}

bool isRawDevice(const char* DevicePath, int Index)
{
    // Convert "Device_" from UNICODE to char
    //size_t   i;
    char cVjoy_Interface[100];
    //size_t returned;

    wcstombs(cVjoy_Interface, VJOY_INTERFACE, 50);
    _strlwr_s(cVjoy_Interface, strlen(cVjoy_Interface)+1);

    // Looking for string that ends with "\Device_nnn" where nnn is the index
    const char* backslash = strrchr(DevicePath, '\\');
    if (!backslash)
        return false;

    char* backslash_lower = _strdup(backslash);
    _strlwr_s(backslash_lower, strlen(backslash)+1);
    char* interfacename = strstr(backslash_lower, cVjoy_Interface);
    if (!interfacename)
        return false;
    interfacename += strlen(cVjoy_Interface);

    int d = 0;
    sscanf_s(interfacename, "%03d", &d);
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: isRawDevice(%d) - Compare %s with %s(d=%d)"), ProcessId, Index, DevicePath, interfacename, d);
    free(backslash_lower);

    if (d == Index)
        return true;
    else
        return false;

}
#pragma warning( pop )

int WINAPI CreateDummyWindow(void)
{
    hInst = GetModuleHandle(0);

    {
        WNDCLASSEX wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInst;
        wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPLICATION));
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = szWindowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

        if (!RegisterClassEx(&wcex)) {
            MessageBox(NULL, _T("Call to RegisterClassEx failed!"), _T("vJoyInterface DLL"), NULL);
            return 1;
        }

        // The parameters to CreateWindow explained:
        // szWindowClass: the name of the application
        // szTitle: the text that appears in the title bar
        // WS_OVERLAPPEDWINDOW: the type of window to create
        // CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
        // 500, 100: initial size (width, length)
        // NULL: the parent of this window
        // NULL: this application does not have a menu bar
        // hInstance: the first parameter from WinMain
        // NULL: not used in this application
        hWnd = CreateWindow(
            szWindowClass,
            szTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            500, 100,
            NULL,
            NULL,
            hInst,
            NULL
        );

        if (!hWnd) {
            // MessageBox(NULL,_T("Call to CreateWindow failed!"),_T("Win32 Guided Tour"),NULL);
            return 1;
        }

        // The parameters to ShowWindow explained:
        // hWnd: the value returned from CreateWindow
        ShowWindow(hWnd, SW_HIDE);
        UpdateWindow(hWnd);

        // Release the parent loop
        SetEvent(hEvent);

        // Main message loop:
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return (int)msg.wParam;
    }
}


LRESULT CALLBACK WndProc(HWND hWind, UINT message, WPARAM wParam, LPARAM lParam)
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message) {
        case WM_PAINT:
            hdc = BeginPaint(hWind, &ps);
            EndPaint(hWind, &ps);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_DEVICECHANGE:
            DeviceChange(wParam, lParam);
            return TRUE;
        case FFB_DATA_READY:
            FfbProcessData(wParam, lParam);
            break;
        default:
            return DefWindowProc(hWind, message, wParam, lParam);
            break;
    }

    return 0;
}


void StartLogging(void)
{
    // Get environment variables - if logging NOT requested then return from function
    DWORD dwRet = 0;
    TCHAR LogLevel[5];
    dwRet = GetEnvironmentVariable(INTERFACE_LOG_LEVEL, LogLevel, 4*sizeof(TCHAR));
    if (TRUE != dwRet)
        return;
    Logging = _ttoi(LogLevel);

    // Logging requested - Get the of output file name from the environment
    // If no output file name then use default file name (and path)
    TCHAR PathName[MAX_PATH];
    TCHAR TempName[MAX_PATH];
    dwRet = GetEnvironmentVariable(INTERFACE_LOG_FILE, PathName, MAX_PATH);
    if (dwRet && dwRet < MAX_PATH)
        LogFileName = _tcsdup(PathName);

    // Default file name
    if (!LogFileName) {
        dwRet = GetEnvironmentVariable(TEXT("TEMP"), TempName, MAX_PATH);
        if (dwRet && dwRet < MAX_PATH)
            _stprintf_s(PathName, MAX_PATH, TEXT("%s\\%s"), TempName, INTERFACE_DEF_LOG_FILE);
        else
            return;
        LogFileName = _tcsdup(PathName);
    }

    // Get process id - use it to mark lines as beginning to a process
    // Get name of calling app - use it to mark lines as beginning to a process
    ProcessId = GetCurrentProcessId();
    LPTSTR ProcessCmd = GetCommandLine();
    if (!ProcessCmd)
        return;

    // Open file for writing.
    errno_t err;
    err = _tfopen_s(&LogStream, LogFileName, "a+");
    if (err || (LogStream == NULL))
        return;

    // Write First line.
    _ftprintf_s(LogStream, _T("\n\n\n[%05u]Process:%s\n"), ProcessId, ProcessCmd);
}

void	InitDll(void)
{
    // Cannot initialize more than once
    if (Init)
        return;

    // Start Logging if logging requested
    StartLogging();

    // Create window process on another thread
    DWORD dwThreadId;
    hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!hEvent) {
        MessageBox(NULL, _T("Create window process on another thread failed!"), _T("vJoyInterface DLL"), NULL);
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CreateDummyWindow, 0, 0, &dwThreadId);

    // Wait for the window on the other thread to be created
    DWORD waitRes = WaitForSingleObject(hEvent, 10000);
    if (waitRes != WAIT_OBJECT_0)
        MessageBox(NULL, _T("Creation of dummy window failed!"), _T("vJoyInterface DLL"), NULL);

    /////////// FFB /////////////////////////////
    FfbEffectState = FALSE; // Effect is off
#ifdef PRINT
    fOut = fopen("C:\\ProgramData\\vJoyDemoLog.txt", "w+");
    fprintf(fOut, "-------------------------------------------------------------\n");
#endif

    if (hThread && hWnd) {
        Init = TRUE;
        hDeviceNotifyInterFace = RegistervJoyNotification(hWnd);
    }
}
HDEVNOTIFY RegistervJoyNotification(HWND Win)
// Register event to catch changes in vJoy device interfaces
// Every time an interface of type GUID_DEVINTERFACE_VJOY is enables/disabled
// the window procedure(WndProc) will receive a window message WM_DEVICECHANGE
// Since vJoy device have 16 such interfaces - this message will be sen 16 times,
// every time the raw-PDO device is added or removed.
//	Notes:
//	1. Remember to un-register handle by calling UnregisterDeviceNotification()
//	2. (???) You can register interface even when vJoy not installed
//	3. Need some modifications if to be used with a Service
{
    if (!Win)
        return NULL;

    HDEVNOTIFY h = NULL;
    GUID InterfaceClassGuid = GUID_DEVINTERFACE_VJOY;
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = InterfaceClassGuid;


    h = RegisterDeviceNotification(
        Win,                       // events recipient
        &NotificationFilter,        // type of device
        DEVICE_NOTIFY_WINDOW_HANDLE  /* type of recipient handle*/
    );

    return h;
}

HDEVNOTIFY RegisterHandleNotification(HWND Win, HANDLE hDev)
{
    if (!Win || hDev == INVALID_HANDLE_VALUE)
        return NULL;

    HDEVNOTIFY hDeviceNotify = NULL;
    DEV_BROADCAST_HANDLE  NotificationFilterHandle;
    ZeroMemory(&NotificationFilterHandle, sizeof(NotificationFilterHandle));
    NotificationFilterHandle.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
    NotificationFilterHandle.dbch_devicetype = DBT_DEVTYP_HANDLE;
    //HANDLE hJoystickHandle = hDev;
    NotificationFilterHandle.dbch_handle = hDev;


    hDeviceNotify = RegisterDeviceNotification(
        Win,                       // events recipient
        &NotificationFilterHandle,        // type of device
        DEVICE_NOTIFY_WINDOW_HANDLE  /* type of recipient handle*/
    );

    return hDeviceNotify;
}

BOOL	InitPosition(int Index)
{
    /*
        Initialize the global structure that holds the current position for a given virtual device
        Index may range 1-16
        Initialization is chosen to be:
        Axes X,Y,Z - Centred
        Other Axes - 0
        Buttons: Not Pressed (0)
        POV Switched: Neutral (-1)
    */
    if (Index<1 || Index>16)
        return FALSE;

    // Initialize
    DEVICE_INIT_VALS data_buf;
    size_t s = sizeof(DEVICE_INIT_VALS);
    data_buf.cb = s;
    data_buf.id = Index;
    return TRUE;


    // Calculate default position
    CalcInitValue(Index, &data_buf);

    BOOL GoodPos = GetDevPosition(Index, &vJoyDevices[Index].position);


    //  Copy default position to position structure
    vJoyDevices[Index].position.wAxisX = data_buf.InitValAxis[0] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAxisY = data_buf.InitValAxis[1] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAxisZ = data_buf.InitValAxis[2] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAxisXRot = data_buf.InitValAxis[3] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAxisYRot = data_buf.InitValAxis[4] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAxisZRot = data_buf.InitValAxis[5] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wSlider = data_buf.InitValAxis[6] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wDial = data_buf.InitValAxis[7] * VJOY_AXIS_MAX_VALUE / 100 + 1;

    vJoyDevices[Index].position.wWheel = data_buf.InitValAxis[8] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAccelerator = data_buf.InitValAxis[9] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wBrake = data_buf.InitValAxis[10] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wClutch = data_buf.InitValAxis[11] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wSteering = data_buf.InitValAxis[12] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wThrottle = data_buf.InitValAxis[13] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wRudder = data_buf.InitValAxis[14] * VJOY_AXIS_MAX_VALUE / 100 + 1;
    vJoyDevices[Index].position.wAileron = data_buf.InitValAxis[15] * VJOY_AXIS_MAX_VALUE / 100 + 1;

    vJoyDevices[Index].position.wAxisVX = vJoyDevices[Index].position.wAxisVY = vJoyDevices[Index].position.wAxisVZ = 0;
    vJoyDevices[Index].position.wAxisVBRX = vJoyDevices[Index].position.wAxisVBRY = vJoyDevices[Index].position.wAxisVBRZ = 0;

    if (data_buf.InitValPov[0] == 0xFF)
        vJoyDevices[Index].position.bHats = (DWORD)-1;
    else
        vJoyDevices[Index].position.bHats = data_buf.InitValPov[0] * 0x8C9F / 100 + 1;

    if (data_buf.InitValPov[1] == 0xFF)
        vJoyDevices[Index].position.bHatsEx1 = (DWORD)-1;
    else
        vJoyDevices[Index].position.bHatsEx1 = data_buf.InitValPov[1] * 0x8C9F / 100 + 1;

    if (data_buf.InitValPov[2] == 0xFF)
        vJoyDevices[Index].position.bHatsEx2 = (DWORD)-1;
    else
        vJoyDevices[Index].position.bHatsEx2 = data_buf.InitValPov[2] * 0x8C9F / 100 + 1;

    if (data_buf.InitValPov[3] == 0xFF)
        vJoyDevices[Index].position.bHatsEx3 = (DWORD)-1;
    else
        vJoyDevices[Index].position.bHatsEx3 = data_buf.InitValPov[3] * 0x8C9F / 100 + 1;

    vJoyDevices[Index].position.lButtons = ((DWORD*)(data_buf.ButtonMask))[0];
    vJoyDevices[Index].position.lButtonsEx1 = ((DWORD*)(data_buf.ButtonMask))[1];
    vJoyDevices[Index].position.lButtonsEx2 = ((DWORD*)(data_buf.ButtonMask))[2];
    vJoyDevices[Index].position.lButtonsEx3 = ((DWORD*)(data_buf.ButtonMask))[3];

    return TRUE;
}

void	CalcInitValue(USHORT id, struct DEVICE_INIT_VALS* data_buf)
{
    UINT mask_device = 0, mask_master = 0;
    DEVICE_INIT_VALS  init_master;
    UCHAR InitValAxis[VJOY_NUMBER_OF_AXES] = { 50, 50, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    UCHAR InitValPov[VJOY_NUMBER_OF_HAT] = { (UCHAR)-1, (UCHAR)-1, (UCHAR)-1, (UCHAR)-1 };
    UCHAR ButtonMask[VJOY_NUMBER_OF_BUTTONS/8] = { 0 };
    int i, j;

    // If ID is NOT 0 then call GetInitValueFromRegistry() and save output buffer in data_buf
    if (id != 0) {
        // Get the data from the registry - if it covers all controls the return
        mask_device = GetInitValueFromRegistry(id, data_buf);
        if (mask_device == 0x1FFF) // all data taken from registry?
            return;
    }

    // Getting the missing data from the master device
    init_master.cb = sizeof(DEVICE_INIT_VALS);
    init_master.id = id;
    mask_master = GetInitValueFromRegistry(0, &init_master);
    int nAxes, nPovs, offset;

    // Merge Axes
    nAxes = (sizeof(data_buf->InitValAxis) / sizeof(data_buf->InitValAxis[0]));
    nPovs = (sizeof(data_buf->InitValPov) / sizeof(data_buf->InitValPov[0]));
    for (i = 0; i <nAxes; i++) {
        offset = nAxes + nPovs - i;
        if (!(mask_device & (1 << offset))) {
            if (mask_master & (1 << offset))
                data_buf->InitValAxis[i] = init_master.InitValAxis[i];
            else
                data_buf->InitValAxis[i] = InitValAxis[i];
        };
    };

    // Merge POVs
    for (j = 0; j < nPovs; i++, j++) {
        offset = nPovs - i;
        if (!(mask_device & (1 << offset))) {
            if (mask_master & (1 << offset))
                data_buf->InitValPov[j] = init_master.InitValPov[j];
            else
                data_buf->InitValPov[j] = InitValPov[j];
        };
    };

    // Buttons
    if (!(mask_device & 1)) {
        if (mask_master & 1)
            for (int k = 0; k < (sizeof(data_buf->ButtonMask) / sizeof(data_buf->ButtonMask[0])); k++)
                data_buf->ButtonMask[k] = init_master.ButtonMask[k];
        else
            for (int k = 0; k < (sizeof(data_buf->ButtonMask) / sizeof(data_buf->ButtonMask[0])); k++)
                data_buf->ButtonMask[k] = ButtonMask[k];
    };


}

UINT	GetInitValueFromRegistry(USHORT id, struct DEVICE_INIT_VALS* data_buf)
{
    PCWSTR	Axes[VJOY_NUMBER_OF_AXES] = {
        L"X", L"Y", L"Z", L"RX", L"RY", L"RZ", L"SL1", L"SL2",
        L"WHL", L"ACC", L"BRK", L"CLU", L"STE", L"AIL", L"RUD", L"THR" };
    UCHAR	nAxes = 0;
    PCWSTR	Povs[VJOY_NUMBER_OF_HAT] = { L"POV1", L"POV2", L"POV3", L"POV4" };
    UCHAR	nPovs = 0;
    INT		nButtons = VJOY_NUMBER_OF_BUTTONS;
    UINT	Mask = 0;


    /* Check that buffer size is sufficient */
    nAxes = sizeof(Axes) / sizeof(PCWSTR);
    nPovs = sizeof(Povs) / sizeof(PCWSTR);
    // BM was sizeof(nButtons) / 8
    // Size in bytes, axes include pov (16+4)
    if (data_buf->cb < (2 + nAxes + nPovs + (sizeof(nButtons)/8)))
        return 0;


    /* Calculate the registry name: "SYSTEM\\CurrentControlSet\\services\\vjoy\\Parameters\\Device**\\Init" */
    size_t size_dev = wcslen(REG_PARAM_DEV); // Size of "SYSTEM\\CurrentControlSet\\services\\vjoy\\Parameters\\Device"
    size_t size_ini = wcslen(REG_INIT); // Size of "Init"
    size_t s = size_ini + size_dev + 10; // Size of entire buffer including slashes, ID etc
    WCHAR* strInit = new WCHAR[s];
    int actual_size = swprintf_s(strInit, s, L"%s%02u\\%s\0", REG_PARAM_DEV, id, REG_INIT);
    if (actual_size < 0 || actual_size >= s)
        return 0; // Error: Creation of registry string failed

    /* Open registry - Most of the path should exist */
    HKEY hParams, hDevDef;
    LONG RegRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, strInit, 0, KEY_READ, &hParams);
    if (RegRes != ERROR_SUCCESS)
        return 0;	// Error: Could not open 

    /* Analyze axes */
    DWORD OutSize = sizeof(UCHAR);
    for (int iAxis = 0; iAxis < nAxes; iAxis++) {
        RegRes = RegGetValueW(hParams, NULL, Axes[iAxis], RRF_RT_REG_BINARY, NULL, &(data_buf->InitValAxis[iAxis]), &OutSize);
        if (RegRes == ERROR_SUCCESS)
            Mask |= 0x01;
        Mask = Mask << 1;
    };

    /* Analyze POVs */
    for (int iPov = 0; iPov < nPovs; iPov++) {
        RegRes = RegGetValueW(hParams, NULL, Povs[iPov], RRF_RT_REG_BINARY, NULL, &(data_buf->InitValPov[iPov]), &OutSize);
        if (RegRes == ERROR_SUCCESS)
            Mask |= 0x01;
        Mask = Mask << 1;
    };

    /* Analyze buttons */
    OutSize = 16*sizeof(UCHAR);
    RegRes = RegGetValueW(hParams, NULL, BTN_INIT, RRF_RT_REG_BINARY, NULL, &(data_buf->ButtonMask), &OutSize);
    if (RegRes == ERROR_SUCCESS)
        Mask |= 0x01;

    RegCloseKey(hParams);

    return Mask;
}

/// <summary>
/// Save current position to the global saved position member of array position
/// </summary>
/// <param name="rID"></param>
/// <param name="pData"></param>
void SavePosition(UINT rID, PVOID pData)
{
    if (!pData || (vJoyDevices.find(rID) == vJoyDevices.end()))
        return;

    memcpy(&(vJoyDevices[rID].position), pData, sizeof(JOYSTICK_POSITION));
}

/// <summary>
/// This function gets the joystick position of a given device by device ID.
/// Function does not change values in structure vJoyDevices[id].position
/// </summary>
/// <param name="id"></param>
/// <param name="pPosition"></param>
/// <returns>Returns TRUE if pPosition points to a valid position data.
/// Otherwise returns FALSE</returns>
BOOL GetDevPosition(BYTE id, PJOYSTICK_POSITION pPosition)
{
    UINT	IoCode = GET_POSITIONS;
    UINT	IoSize = sizeof(JOYSTICK_POSITION);
    ULONG	bytes;
    HANDLE	hIoctlEvent;
    OVERLAPPED OverLapped = { 0 };

    // Preparing
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;

    // Get joystick position structure from vJoy device
    BOOL res = DeviceIoControl(Get_h(id), IoCode, NULL, 0, (PVOID)pPosition, IoSize, &bytes, &OverLapped);
    // immediate Return
    if (res) {
        CloseHandle(OverLapped.hEvent);
        if (bytes) {
            // In case driver did not fill id
            if (pPosition->bDevice == 0)
                pPosition->bDevice = id;
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevPosition() - Returns (Immediatly) TRUE"), ProcessId);
            return TRUE;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDevPosition() - Returns (Immediatly) FALSE"), ProcessId);
            return FALSE;
        }
    }

    // Delayed/Error
    else {
        // Error getting the data
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: GetDevPosition() - error (0x%X) Returns FALSE"), ProcessId, err);
            CloseHandle(OverLapped.hEvent);
            return FALSE;
        }

        // Wait until data ready
        DWORD nBytesTranss = 0;
        BOOL gotdata = GetOverlappedResult(Get_h(id), &OverLapped, &nBytesTranss, TRUE);
        CloseHandle(OverLapped.hEvent);

        // Data received and it is not empty
        if (gotdata && nBytesTranss) {
            // In case driver did not fill id
            if (pPosition->bDevice == 0)
                pPosition->bDevice = id;
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevPosition() - gotdata=%d nBytesTranss=%u  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
            return TRUE;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: GetDevPosition() - gotdata=%d nBytesTranss=%u  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
            return FALSE;
        }
    }

    return FALSE;
}


/// <summary>
/// This function writes the position data to the specified VJD
/// The VJD should be open for writing.If not the function returns FALSE
/// If the data is NULL or if the Report ID(rID) is out of range then the function returns FALSE.
/// The function will return TRUE only if the writing was successful
/// </summary>
/// <param name="rID"></param>
/// <returns></returns>
BOOL Update(UINT rID)
{
    PVOID pData = &(vJoyDevices[rID].position);
    if (!pData || (vJoyDevices.find(rID) == vJoyDevices.end()) || Get_h(rID) == INVALID_HANDLE_VALUE || Get_stat(rID) != VJD_STAT_OWN)
        return FALSE;

    UINT	IoCode = LOAD_POSITIONS;
    UINT	IoSize = sizeof(JOYSTICK_POSITION);
    ULONG	bytes;
    HANDLE	hIoctlEvent;
    OVERLAPPED OverLapped = { 0 };

    // Preparing
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;

    // Send joystick position structure to vJoy device
    BOOL res = DeviceIoControl(Get_h(rID), IoCode, pData, IoSize, NULL, 0, &bytes, &OverLapped);

    // Test Results
    if (!res) {
        // The transaction failed.
        // If it is just because it is pending then wait otherwise it is an error
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING)
            res = FALSE;
        else {	// Wait for write to complete
            DWORD WaitRet = WaitForSingleObject(OverLapped.hEvent, 500);
            if (WAIT_OBJECT_0 == WaitRet)
                res = TRUE;
            else
                res = FALSE;
        }
    }
    CloseHandle(OverLapped.hEvent);
    return res;
}


/*
    Test is a given device supports FFB
    Indicate device by Device ID
    If Device supports FFB then return TRUE
    Else return FALSE
*/

int	DbgGetCaps(void)
{
    UINT rID = 1;
    PUSAGE UsageList;
    NTSTATUS stat = HIDP_STATUS_SUCCESS;
    PHIDP_PREPARSED_DATA PreparsedData = NULL;
    HIDP_CAPS Capabilities;

    // Get the Value Capabilities of a given axis in a given device
#if OLD_PREPARSED
    int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h == INVALID_HANDLE_VALUE)
        return NO_HANDLE_BY_INDEX;
    // Get Preparsed data
    BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
    HANDLE h = INVALID_HANDLE_VALUE;
    BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

    if (!ok) {
        CloseHandle(h);
        return BAD_PREPARSED_DATA;
    }

    // returns a top-level collection's HIDP_CAPS structure.
    stat = HidP_GetCaps(PreparsedData, &Capabilities);
    if (stat != HIDP_STATUS_SUCCESS) {
        CloseHandle(h);
        return NO_CAPS;
    }

    // Get data related to output values
    UINT Usage;
    ULONG n = Capabilities.NumberLinkCollectionNodes;
    if (n<1) {
        CloseHandle(h);
        return BAD_N_VAL_CAPS;
    }

    // Get array of of link collections
    PHIDP_LINK_COLLECTION_NODE vLinks = new   HIDP_LINK_COLLECTION_NODE[1 + n];
    stat = HidP_GetLinkCollectionNodes(vLinks, &n, PreparsedData);

    ULONG   UsageLength = Capabilities.NumberOutputValueCaps;
    UsageList = new USAGE[Capabilities.NumberOutputValueCaps + 1];
    //CHAR Report[10000];
    PHIDP_VALUE_CAPS val_caps = new HIDP_VALUE_CAPS[100];
    USHORT n_val_caps = 100;
    bool Effect_Report = false;


    // Check every Link Collection node
    for (UINT cnt = 0; cnt < n; cnt++) {
        // Link is the current Link Collection
        HIDP_LINK_COLLECTION_NODE Link = vLinks[cnt];

        ULONG   UsageLengthLocal = Capabilities.NumberOutputValueCaps;
        UsageList = new USAGE[Capabilities.NumberOutputValueCaps + 1];
        CHAR Report[10000];
        PHIDP_VALUE_CAPS val_caps_local = new HIDP_VALUE_CAPS[100];
        USHORT n_val_caps_local = 100;

        stat = HidP_GetUsageValue(
            HidP_Output,
            Link.LinkUsagePage,
            0/*cnt*/,
            0x25/*Link.LinkUsage*/,
            &UsageLengthLocal,
            PreparsedData,
            Report,
            Capabilities.OutputReportByteLength);
        if (HIDP_STATUS_USAGE_NOT_FOUND != stat)
            return 2;

        stat = HidP_GetSpecificValueCaps(
            HidP_Output,
            Link.LinkUsagePage,
            0,
            0x25,
            val_caps_local,
            &n_val_caps_local,
            PreparsedData
        );
        if (HIDP_STATUS_USAGE_NOT_FOUND != stat)
            return 2;

        // This collection is:
        //  Usage Set Effect Report(0x21),
        //  Usage Page Physical Interface (0x0F),
        //	Type= Output (2)

        if (Link.LinkUsage == 0x21 && Link.LinkUsagePage == 0xf && Link.CollectionType == 2)
            Effect_Report = true;
    }	 // For loop


    // Get data related to values
    USHORT ns = Capabilities.NumberOutputValueCaps;
    if (ns<1) {
        CloseHandle(h);
        return BAD_N_VAL_CAPS;
    }

    bool  Effect_Block = false;
    PHIDP_VALUE_CAPS vCaps = new HIDP_VALUE_CAPS[1 + ns]; // Added 1 just to make the Analyzer happy
    stat = HidP_GetValueCaps(HidP_Output, vCaps, &ns, PreparsedData);
    if (stat == HIDP_STATUS_SUCCESS) {
        for (int i = 0; i < ns; i++)			  //    Loop on all values
            if (vCaps[i].ReportID == 0x11     //    HID_ID_EFFREP + 0x10 * TLID
                && vCaps[i].UsagePage == 0x0F //    Usage Page Physical Interface
                && vCaps[i].LinkUsage == 0x21 //    Usage Set Effect Report
                )
                Effect_Block = true;
    }

    // Get output buttons
    USHORT nb_bu;
    USHORT nb = Capabilities.NumberOutputButtonCaps;
    HIDP_BUTTON_CAPS* bCaps = new HIDP_BUTTON_CAPS[nb];
    SecureZeroMemory(bCaps, sizeof(HIDP_BUTTON_CAPS)*nb);
    nb_bu = nb;
    stat = HidP_GetButtonCaps(HidP_Output, bCaps, &nb, PreparsedData);
    if (FAILED(stat))
        return NO_CAPS;
    if (nb>nb_bu)
        return NO_CAPS;
    bool Custom_Force = false;

    if (stat == HIDP_STATUS_SUCCESS) {
        for (int i = 0; i<nb; i++) // Loop on all values
            if (bCaps[i].ReportID == 0x11     //    HID_ID_EFFREP + 0x10 * TLID	(This is for Device #1)
                && bCaps[i].UsagePage == 0x0F //    Usage Page Physical Interface
                && bCaps[i].LinkUsage == 0x25 //    Usage Effect Type
                && bCaps[i].NotRange.Usage == 0x28 	//    Usage ET Custom Force Data (Do it for all 12 effects)
                )
                Custom_Force = true;
    }


    //HidD_FreePreparsedData(PreparsedData);
    delete[](vCaps);
    CloseHandle(h);

    return 0;
}

// Update control structure of the given vJoy Device
// Use this function to detect which axes exist, the number of POVs and the number of buttons
// It does NOT deal with their values
INT		GetControls(UINT rID)
{
    NTSTATUS stat = HIDP_STATUS_SUCCESS;
    PHIDP_PREPARSED_DATA PreparsedData = NULL;
    HIDP_CAPS Capabilities;

    // Get the Value Capabilities of a given axis in a given device
#if OLD_PREPARSED
    int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h == INVALID_HANDLE_VALUE)
        return NO_HANDLE_BY_INDEX;
    // Get the device's preparsed data
    BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
    HANDLE h = INVALID_HANDLE_VALUE;
    BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

    if (!ok) {
        CloseHandle(h);
        return BAD_PREPARSED_DATA;
    }

    // Get device's capabilities
    stat = HidP_GetCaps(PreparsedData, &Capabilities);
    if (stat != HIDP_STATUS_SUCCESS) {
        CloseHandle(h);
        return NO_CAPS;
    }

    // Create structure for this device
    vJoyDeviceEntry(rID);

    // Clean  DeviceControls structure
    vJoyDevices[rID].DeviceControls = { 
        FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
        0, 0, 0 };


    // Get Button data
    int ButtonBaseIndex, nButtons = 0;
    USHORT n = Capabilities.NumberInputButtonCaps;
    if (n<1) {
        CloseHandle(h);
        return BAD_N_BTN_CAPS;
    }

    HIDP_BUTTON_CAPS* bCaps = new HIDP_BUTTON_CAPS[n];
    SecureZeroMemory(bCaps, sizeof(HIDP_BUTTON_CAPS)*n);
    stat = HidP_GetButtonCaps(HidP_Input, bCaps, &n, PreparsedData);
    if (stat != HIDP_STATUS_SUCCESS) {
        CloseHandle(h);
        delete[] 	bCaps;
        return BAD_BTN_CAPS;
    }

    if (bCaps[0].IsRange)
        vJoyDevices[rID].DeviceControls.nButtons = (bCaps[0].Range).UsageMax - (bCaps[0].Range).UsageMin + 1;


    // Get data related to values (axes/POVs)
    UINT Usage;
    n = Capabilities.NumberInputValueCaps;
    if (n<1) {
        CloseHandle(h);
        return BAD_N_VAL_CAPS;
    }


    PHIDP_VALUE_CAPS vCaps = new HIDP_VALUE_CAPS[1 + n]; // Added 1 just to make the Analyzer happy
    stat = HidP_GetValueCaps(HidP_Input, vCaps, &n, PreparsedData);
    if (stat == HIDP_STATUS_SUCCESS) {
        for (int i = 0; i<n; i++) // Loop on all values
        {
            Usage = ((vCaps[i]).NotRange).Usage; // Usage is the code of the axis (0x30="X", 0x39="POV1 etc.)
            switch (Usage) {
                case HID_USAGE_X:
                    vJoyDevices[rID].DeviceControls.AxisX = TRUE;
                    break;
                case HID_USAGE_Y:
                    vJoyDevices[rID].DeviceControls.AxisY = TRUE;
                    break;
                case HID_USAGE_Z:
                    vJoyDevices[rID].DeviceControls.AxisZ = TRUE;
                    break;
                case HID_USAGE_RX:
                    vJoyDevices[rID].DeviceControls.AxisXRot = TRUE;
                    break;
                case HID_USAGE_RY:
                    vJoyDevices[rID].DeviceControls.AxisYRot = TRUE;
                    break;
                case HID_USAGE_RZ:
                    vJoyDevices[rID].DeviceControls.AxisZRot = TRUE;
                    break;
                case HID_USAGE_SL0:
                    vJoyDevices[rID].DeviceControls.Slider = TRUE;
                    break;
                case HID_USAGE_SL1:
                    vJoyDevices[rID].DeviceControls.Dial = TRUE;
                    break;

                case HID_USAGE_WHL:
                    vJoyDevices[rID].DeviceControls.Wheel = TRUE;
                    break;
                case HID_USAGE_ACCELERATOR:
                    vJoyDevices[rID].DeviceControls.Accelerator = TRUE;
                    break;
                case HID_USAGE_BRAKE:
                    vJoyDevices[rID].DeviceControls.Brake = TRUE;
                    break;
                case HID_USAGE_CLUTCH:
                    vJoyDevices[rID].DeviceControls.Clutch = TRUE;
                    break;
                case HID_USAGE_STEERING:
                    vJoyDevices[rID].DeviceControls.Steering = TRUE;
                    break;
                case HID_USAGE_AILERON:
                    vJoyDevices[rID].DeviceControls.Aileron = TRUE;
                    break;
                case HID_USAGE_RUDDER:
                    vJoyDevices[rID].DeviceControls.Rudder = TRUE;
                    break;
                case HID_USAGE_THROTTLE:
                    vJoyDevices[rID].DeviceControls.Throttle = TRUE;
                    break;
                case HID_USAGE_POV:
                    if (vCaps[i].LogicalMax == 3)
                        vJoyDevices[rID].DeviceControls.nDescHats++;
                    else  if (vCaps[i].LogicalMax > 3)
                        vJoyDevices[rID].DeviceControls.nContHats++;
                    break;
                default:
                    break;
            }
        }
    }

    vJoyDevices[rID].DeviceControls.Init = TRUE;
    //HidD_FreePreparsedData(PreparsedData);
    delete[](vCaps);
    delete[] 	bCaps;
    CloseHandle(h);

    return HIDP_STATUS_SUCCESS;
}

BOOL	AreControlsInit(UINT rID)
{
    // Element exists?
    if (!vJoyDevices.size() || (vJoyDevices.find(rID) == vJoyDevices.end()))
        return FALSE;

    return vJoyDevices[rID].DeviceControls.Init;
}

// Parse HID descriptor to deduce whether an axis is present
BOOL	GetAxisCaps(UINT rID, UINT Axis, HIDP_VALUE_CAPS* ValCaps)
{
    NTSTATUS stat = HIDP_STATUS_SUCCESS;
    PHIDP_PREPARSED_DATA PreparsedData = NULL;
    HIDP_CAPS Capabilities;

    if (rID<1 || rID>16)
        return BAD_ID_RANGE;

    // Get the Value Capabilities of a given axis in a given device
#if OLD_PREPARSED
    int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h == INVALID_HANDLE_VALUE)
        return NO_HANDLE_BY_INDEX;
    BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
#else
    HANDLE h = INVALID_HANDLE_VALUE;
    BOOL ok = Get_PreparsedData(rID, &PreparsedData);
#endif // OLD_PREPARSED

    if (!ok) {
        CloseHandle(h);
        return BAD_PREPARSED_DATA;
    }
    stat = HidP_GetCaps(PreparsedData, &Capabilities);
    if (stat != HIDP_STATUS_SUCCESS) {
        CloseHandle(h);
        return NO_CAPS;
    }

    // Get data related to values (axes/POVs)
    UINT Usage;
    USHORT n = Capabilities.NumberInputValueCaps;
    if (n<1) {
        CloseHandle(h);
        return BAD_N_VAL_CAPS;
    }
    PHIDP_VALUE_CAPS vCaps = new HIDP_VALUE_CAPS[1 + n]; // Added 1 just to make the Analyzer happy
    stat = HidP_GetValueCaps(HidP_Input, vCaps, &n, PreparsedData);
    if (stat==HIDP_STATUS_SUCCESS) {
        for (int i = 0; i<n; i++) // Loop on all values
        {
            Usage = ((vCaps[i]).NotRange).Usage; // Usage is the code of the axis (0x30="X", 0x39="POV1 etc.)
            if (Axis == Usage) {
                CloseHandle(h);
                memcpy(ValCaps, (void*)(&(vCaps[i])), sizeof(HIDP_VALUE_CAPS));
                delete[](vCaps);
                return TRUE;
            }
        }
    }

    //HidD_FreePreparsedData(PreparsedData);
    delete[](vCaps);
    CloseHandle(h);

    return NO_SUCH_AXIS;
}
LONG	GetAxisLogMin(UINT rID, UINT Axis)
{
    // Get the logical Minimum of a given axis in a given device
    // Default value is 0

    if (rID<1 || rID>16)
        return 0;

    HIDP_VALUE_CAPS pValCaps;
    if (!GetAxisCaps(rID, Axis, &pValCaps))
        return 0;

    return pValCaps.LogicalMin;
}

LONG	GetAxisLogMax(UINT rID, UINT Axis)
{
    // Get the logical Maximum of a given axis in a given device
    // Default value is 0

    if (rID<1 || rID>16)
        return 0;

    HIDP_VALUE_CAPS pValCaps;
    if (!GetAxisCaps(rID, Axis, &pValCaps))
        return 0;

    return pValCaps.LogicalMax;
}

LONG	GetAxisLogMid(UINT rID, UINT Axis)
{
    // Get the logical mid-point of a given axis in a given device
    // Default value is 0
    LONG Max, Min;
    Max = GetAxisLogMax(rID, Axis);
    Min = GetAxisLogMin(rID, Axis);

    return (Max+Min+1)/2;
}


#pragma region vJoy Device Map manipulation functions

// Create a new map entry with default values
// Then insert it to the container
// Fails is rID out of range
// Fails is entry already exists
BOOL vJoyDeviceEntry(int rID)
{

    // If already exists - fail
    if (vJoyDevices.find(rID) != vJoyDevices.end())
        return FALSE;

    auto out = vJoyDevices.emplace(rID, DeviceStat{ INVALID_HANDLE_VALUE, VJD_STAT_UNKN, { 0 }, NULL, { FALSE }, NULL });
    if (!out.second)
        return FALSE;

    return TRUE;
}

// Remove an existing map entry
// Returns TRUE if entry existed
// Else FALSE
BOOL vJoyDeviceRemove(int rID)
{
    // If no entries - fail
    if (vJoyDevices.empty())
        return FALSE;

    // If doesn't exist - fail
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        return FALSE;

    // Free Preparsed data
    if (vJoyDevices[rID].pPreParsedData) {
        HidD_FreePreparsedData((PHIDP_PREPARSED_DATA)vJoyDevices[rID].pPreParsedData);
        vJoyDevices[rID].pPreParsedData = NULL;
    }

    // Close handle to device
    CloseHandle(vJoyDevices[rID].h);

    // Unregister Device notification
    UnregisterDeviceNotification(vJoyDevices[rID].hDeviceNotifyHandle);


    // Erase - return number of erased
    auto erased = vJoyDevices.erase(rID);
    if (!erased)
        return FALSE;
    else
        return TRUE;
}

// Remove all map entries
void vJoyDeviceClear(void)
{
    vJoyDevices.clear();
}

/// Preparsed data manipulation

// Set_PreparsedData:
// Calculates and saves the preparsed data for a given device
// Does NOT create a device if does not exist
// Returns TRUE only if success
BOOL  Set_PreparsedData(int rID)
{
    // If doesn't exist - Fail
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        vJoyDeviceEntry(rID);
    //return FALSE;

// If preparsed data already exists - free it.
    if (vJoyDevices[rID].pPreParsedData) {
        HidD_FreePreparsedData((PHIDP_PREPARSED_DATA)vJoyDevices[rID].pPreParsedData);
        vJoyDevices[rID].pPreParsedData = NULL;
    }

    // Get handle to top-level collection
    PHIDP_PREPARSED_DATA PreparsedData = NULL;
    int Index = GetDeviceIndexByReportId(VENDOR_N_ID, PRODUCT_N_ID, (BYTE)rID);
    HANDLE h = GetHandleByIndex(Index);
    if (!h || h == INVALID_HANDLE_VALUE)
        return FALSE;// NO_HANDLE_BY_INDEX;
    // Get the preparsed data
    BOOL ok = HidD_GetPreparsedData(h, &PreparsedData);
    CloseHandle(h);

    if (TRUE != ok)
        vJoyDevices[rID].pPreParsedData = NULL;
    else {
        vJoyDevices[rID].pPreParsedData = PreparsedData;
    }
    return ok;
}

// Get_PreparsedData:
// Gets the preparsed data for a given device
// If the device exists but the preparsed data not calculated - it calculates it
// Does NOT create a device if does not exist
// Returns TRUE only if preparsed data is valid
BOOL Get_PreparsedData(int rID, PHIDP_PREPARSED_DATA* pPPData)
{
    // If doesn't exist - Fail
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        vJoyDeviceEntry(rID);



    // Get the existing preparsed data (if exists)
    if (vJoyDevices[rID].pPreParsedData != NULL) {
        *pPPData = (PHIDP_PREPARSED_DATA)(vJoyDevices[rID].pPreParsedData);
        // make sure preparsed data is valid
        HIDP_CAPS           Capabilities;
        NTSTATUS stat = HidP_GetCaps(*pPPData, &Capabilities);
        if (stat == HIDP_STATUS_SUCCESS)
            return TRUE;
    }

    if (Set_PreparsedData(rID)) {
        *pPPData = (PHIDP_PREPARSED_DATA)(vJoyDevices[rID].pPreParsedData);
        return TRUE;
    } else
        return FALSE;
}


/// Get/Set handle to vJoy Device

//Set_h() :
//Replaces the h for an existing entry in the container
//If handle is not INVALID_HANDLE_VALUE  - it Closes handle before replacing its value
//Creates a new entry if entry does not exist
void Set_h(int rID, HANDLE h)
{
    // If doesn't exist - Create
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        vJoyDeviceEntry(rID);

    // If the current device handle is valid - first close the handle
    if (vJoyDevices[rID].h != INVALID_HANDLE_VALUE)
        CloseHandle(vJoyDevices[rID].h);

    // Now set the value of the handle
    vJoyDevices[rID].h = h;
}

// Sync_Position(int rID)
// Syncs the existing position entry in the container
// Requires the h member to be valid -
// Call only after Set_h
void Sync_Position(int rID)
{
    // If doesn't exist - return
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        return;

    // if handle is invalid - return
    if (vJoyDevices[rID].h == INVALID_HANDLE_VALUE)
        return;

    // Get the current device position from the device
    JOYSTICK_POSITION Position;
    BOOL bRes = GetDevPosition(rID, &Position);
    if (!bRes)
        return;

    // Update the container
    SavePosition(rID, PVOID(&Position));
}
//Get_h() :
//If entry exists : Returns the  handle to the device
//Else : Return INVALID_HANDLE_VALUE
HANDLE 	Get_h(int rID)
{
    // If doesn't exist - Return INVALID_HANDLE_VALUE
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        return INVALID_HANDLE_VALUE;

    // If the current device handle is valid - first close the handle
    return vJoyDevices[rID].h;
}

//// Get/Set value of device status

//Set_stat() :
//If entry exists : Replaces value
//Else : Creates a new entry with this value and inserts it into the container
void Set_stat(int rID, VjdStat status)
{
    // If doesn't exist - Create
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        vJoyDeviceEntry(rID);

    // Set status
    vJoyDevices[rID].stat = status;
}

//Get_stat() :
//If entry exists : Returns the  status
//Else : Return VJD_STAT_UNKN
VjdStat  Get_stat(int rID)
{
    VjdStat stat = VJD_STAT_UNKN;

    // If doesn't exist - Return VJD_STAT_UNKN
    if (vJoyDevices.find(rID) != vJoyDevices.end())
        stat = vJoyDevices[rID].stat;

    return stat;
}

//Set_hNotify() :
//If entry exists : Replaces value
// If doesn't exist - Create
void Set_hNotify(int rID, HDEVNOTIFY h)
{
    if (vJoyDevices.find(rID) == vJoyDevices.end())
        vJoyDeviceEntry(rID);

    // Remove previous handle
    UnregisterDeviceNotification(vJoyDevices[rID].hDeviceNotifyHandle);

    // Set handle
    vJoyDevices[rID].hDeviceNotifyHandle = h;
}

//Get_hNotify() :
//If entry exists : Returns the  notification handle
//Else : Return NULL
HDEVNOTIFY 	Get_hNotify(int rID)
{
    HDEVNOTIFY h = NULL;
    // If doesn't exist - Return NULL
    if (vJoyDevices.find(rID) != vJoyDevices.end())
        h = vJoyDevices[rID].hDeviceNotifyHandle;

    return h;
}

#pragma endregion  vJoy Device Map manipulation functions

#pragma region FFB Functions

///// Force Feedback related functions //////////////////////////////////////////

BOOL FfbGetEffectState(void)
{
    if ((FfbDataPacket.cmd == IOCTL_HID_WRITE_REPORT)&& 		// Write data
        (FfbDataPacket.data[0] == 0xA)&&		// Report ID is 0xA
        (FfbDataPacket.data[1] == 1)&& 		// Block index is 1
        (
        (FfbDataPacket.data[2] == 1)|| 	// Start
            (FfbDataPacket.data[2] == 2) 		// or Start Solo
            )&&
            (FfbDataPacket.data[3] > 0)			// Loop at least once
        )
        FfbEffectState = TRUE;

    // Condition to stop effect
    if (
        (FfbDataPacket.cmd == IOCTL_HID_WRITE_REPORT)	&&		// Write data
        (FfbDataPacket.data[0] == 0xA)	&&		// Report ID is 0xA
        (FfbDataPacket.data[1] == 1)	&&		// Block index is 1
        (FfbDataPacket.data[2] == 3)	  		// Stop
        )
        FfbEffectState = FALSE;

    return FfbEffectState;
}

FFBEType FfbGetEffectType(void)
{
    if (FfbDataPacket.cmd == IOCTL_HID_SET_FEATURE) { // This is Set Feature
        if (FfbDataPacket.data[0] == 0x01) { // This is "Effect Type"
            FfbEffectType = (FFBEType)FfbDataPacket.data[1];
        }
    }
    return FfbEffectType;
}

// FFB Helper functions


#pragma endregion

#if 0
bool _FfbStart(HANDLE h) // Obsolete
{
    // Notify the driver that this feeder supports FFB
    UINT	IoCode = SET_FFB_STAT;
    UINT	IoSize = sizeof(DWORD);
    ULONG	bytes;
    DWORD	pData = 1;
    BYTE	Stat;
    HANDLE	hIoctlEvent;
    OVERLAPPED OverLapped = { 0 };

    // Preparing
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;

    // Activate FFB queues in the device
    BOOL	res = DeviceIoControl(h, IoCode, &pData, IoSize, NULL, 0, &bytes, &OverLapped);
    // Test Results
    if (!res) {
        // The transaction was not completed.
        // If it is just because it is pending then wait otherwise it is an error
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbStart() - DeviceIoControl was not completed"), ProcessId);
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            CloseHandle(OverLapped.hEvent);
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbStart() - DeviceIoControl failed with error 0x%X"), ProcessId, err);
            return FALSE;
        } else {	// Wait for write to complete
            DWORD WaitRet = WaitForSingleObject(OverLapped.hEvent, 500);
            if (WAIT_OBJECT_0 != WaitRet) {
                CloseHandle(OverLapped.hEvent);
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbStart() - WaitForSingleObject returned 0x%X"), ProcessId, WaitRet);
                return FALSE;
            }
        }
    }
    CloseHandle(OverLapped.hEvent);
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbStart() - DeviceIoControl successful"), ProcessId);


    // Start a thread that waits for FFB data
    DWORD dwFfbThreadId;
    hFfbEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE hFfbThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&FfbWaitForData, h, 0, &dwFfbThreadId);

    // Wait for the FFB thread to be created
    DWORD waitRes = WaitForSingleObject(hFfbEvent, 10000);
    if (waitRes != WAIT_OBJECT_0) {
        MessageBox(NULL, _T("Creation of Ffb data-loop failed!"), _T("vJoyInterface DLL"), NULL);
        return false;
    }


    if (dwFfbThreadId)
        return true;
    else
        return false;
}

bool _FfbStop(HANDLE h)	// Obsolete
{
    UINT	IoCode = SET_FFB_STAT;
    UINT	IoSize = sizeof(DWORD);
    ULONG	bytes;
    DWORD	pData = 0;
    BYTE	Stat;
    HANDLE	hIoctlEvent;
    OVERLAPPED OverLapped = { 0 };

    // Preparing
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;

    // Activate FFB queues in the device
    BOOL	res = DeviceIoControl(h, IoCode, &pData, IoSize, NULL, 0, &bytes, &OverLapped);
    if (!res) {
        // The transaction was not completed.
        // If it is just because it is pending then wait otherwise it is an error
        if (LogStream)
            _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbStop() - DeviceIoControl was not completed"), ProcessId);
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            CloseHandle(OverLapped.hEvent);
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbStop() - DeviceIoControl failed with error 0x%X"), ProcessId, err);
            return FALSE;
        } else {	// Wait for write to complete
            DWORD WaitRet = WaitForSingleObject(OverLapped.hEvent, 500);
            if (WAIT_OBJECT_0 != WaitRet) {
                CloseHandle(OverLapped.hEvent);
                if (LogStream)
                    _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbStop() - WaitForSingleObject returned 0x%X"), ProcessId, WaitRet);
                return FALSE;
            }
        }
    }
    CloseHandle(OverLapped.hEvent);
    if (LogStream)
        _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbStop() - DeviceIoControl successful"), ProcessId);
    return TRUE;
}

#endif

#if 0

bool	FfbIsStarted(HANDLE h)
{
    ULONG	bytes = 0;
    BYTE	Stat = 0;
    HANDLE	hIoctlEvent;
    OVERLAPPED OverLapped = { 0 };

    // Preparing
    hIoctlEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    memset(&OverLapped, 0, sizeof(OVERLAPPED));
    OverLapped.hEvent = hIoctlEvent;
    OverLapped.Internal = STATUS_PENDING;

    // Test  FFB queues in the device
    BOOL ok = DeviceIoControl(h, GET_FFB_STAT, NULL, NULL, &Stat, 1, &bytes, &OverLapped);

    // Imedeate Return
    if (ok) {
        CloseHandle(OverLapped.hEvent);
        if (bytes && Stat) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbIsStarted() - Returns (Immediatly) TRUE"), ProcessId);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbIsStarted() - Returns (Immediatly) FALSE (bytes=%d stat=%d)"), ProcessId, bytes, Stat);
            return false;
        }
    }

    // Pending or Error
    else {
        // Error
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Error: FfbIsStarted() - error (0x%X) Returns FALSE"), ProcessId, err);
            CloseHandle(OverLapped.hEvent);
            return false;
        }

        // Wait until data ready
        DWORD nBytesTranss = 0;
        BOOL gotdata = GetOverlappedResult(h, &OverLapped, &nBytesTranss, TRUE);
        CloseHandle(OverLapped.hEvent);
        if (gotdata && nBytesTranss) {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbIsStarted() - gotdata=%d nBytesTranss=%d  Returns TRUE"), ProcessId, gotdata, nBytesTranss);
            return true;
        } else {
            if (LogStream)
                _ftprintf_s(LogStream, _T("\n[%05u]Info: FfbIsStarted() - gotdata=%d nBytesTranss=%d  Returns FALSE"), ProcessId, gotdata, nBytesTranss);
            return false;
        }
    }
}

BOOL FfbGetEffectState(void)
{
    if ((FfbDataPacket.cmd == IOCTL_HID_WRITE_REPORT) && 		// Write data
        (FfbDataPacket.data[0] == 0xA) &&		// Report ID is 0xA
        (FfbDataPacket.data[1] == 1) && 		// Block index is 1
        (
        (FfbDataPacket.data[2] == 1) || 	// Start
            (FfbDataPacket.data[2] == 2) 		// or Start Solo
            ) &&
            (FfbDataPacket.data[3] > 0)			// Loop at least once
        )
        FfbEffectState = TRUE;

    // Condition to stop effect
    if (
        (FfbDataPacket.cmd == IOCTL_HID_WRITE_REPORT) &&		// Write data
        (FfbDataPacket.data[0] == 0xA) &&		// Report ID is 0xA
        (FfbDataPacket.data[1] == 1) &&		// Block index is 1
        (FfbDataPacket.data[2] == 3)	  		// Stop
        )
        FfbEffectState = FALSE;

    return FfbEffectState;
}

FFBEType FfbGetEffectType(void)
{
    if (FfbDataPacket.cmd == IOCTL_HID_SET_FEATURE) { // This is Set Feature
        if (FfbDataPacket.data[0] == 0x01) { // This is "Effect Type"
            FfbEffectType = (FFBEType)FfbDataPacket.data[1];
        }
    }
    return FfbEffectType;
}


#endif