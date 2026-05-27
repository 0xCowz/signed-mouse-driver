#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

#define UVHID_VID       0x9512
#define UVHID_PID       0x9512
#define UVHID_BUF_SIZE  65 

#pragma pack(push, 1)
struct MouseReport03 {
    UCHAR  ReportId;  
    UCHAR  Buttons;   
    CHAR   X;        
    CHAR   Y;         
    CHAR   Wheel;     
    CHAR   HScroll;   
};

struct UvhidOutBuf {
    UCHAR       VendorReportId; 
    UCHAR       DataLength;
    UCHAR       Command;       
    MouseReport03 Inner;
    UCHAR       Pad[64 - 2 - sizeof(MouseReport03)];
};
#pragma pack(pop)

static_assert(sizeof(UvhidOutBuf) == UVHID_BUF_SIZE, "size mismatch");

HANDLE OpenUvhidDevice() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO devInfo = SetupDiGetClassDevs(
        &hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    SP_DEVICE_INTERFACE_DATA iface = { sizeof(iface) };
    for (DWORD i = 0;
        SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, i, &iface);
        i++)
    {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo, &iface, NULL, 0, &needed, NULL);
        auto* det = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(needed);
        if (!det) continue;
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail(devInfo, &iface, det, needed, NULL, NULL)) {
            free(det); continue;
        }
        HANDLE h = CreateFile(det->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);
        free(det);
        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES a = { sizeof(a) };
        if (HidD_GetAttributes(h, &a)
            && a.VendorID == UVHID_VID
            && a.ProductID == UVHID_PID) {
            printf("Opened uvhid \n");
            SetupDiDestroyDeviceInfoList(devInfo);
            return h;
        }
        CloseHandle(h);
    }
    SetupDiDestroyDeviceInfoList(devInfo);
    return INVALID_HANDLE_VALUE;
}

BOOL MouseMove(HANDLE hDev, int dX, int dY) {
    UvhidOutBuf buf = {};
    buf.VendorReportId = 0x40;
    buf.DataLength = sizeof(MouseReport03);
    buf.Command = 0;
    buf.Inner.ReportId = 0x03;
    buf.Inner.Buttons = 0;
    buf.Inner.X = (CHAR)max(-127, min(127, dX));
    buf.Inner.Y = (CHAR)max(-127, min(127, dY));
    buf.Inner.Wheel = 0;
    buf.Inner.HScroll = 0;

    BOOL ok = HidD_SetOutputReport(hDev, &buf, UVHID_BUF_SIZE);
    if (!ok)
        printf("failed: 0x%08X\n", GetLastError());
    return ok;
}

BOOL MouseClick(HANDLE hDev, UCHAR buttonMask) {
    UvhidOutBuf buf = {};
    buf.VendorReportId = 0x40;
    buf.DataLength = sizeof(MouseReport03);
    buf.Command = 0;
    buf.Inner.ReportId = 0x03;
    buf.Inner.Buttons = buttonMask;  
    HidD_SetOutputReport(hDev, &buf, UVHID_BUF_SIZE);
    Sleep(20);
    buf.Inner.Buttons = 0;          
    return HidD_SetOutputReport(hDev, &buf, UVHID_BUF_SIZE);
}

void MouseMoveLarge(HANDLE hDev, int dX, int dY) {
    while (dX != 0 || dY != 0) {
        int stepX = max(-127, min(127, dX));
        int stepY = max(-127, min(127, dY));
        MouseMove(hDev, stepX, stepY);
        dX -= stepX;
        dY -= stepY;
        Sleep(1);
    }
}

int main() {
    HANDLE hDev = OpenUvhidDevice();
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("Device not found \n");
        return 1;
    }

    printf("Moving mouse.. \n");
    while (true) 
    {
        Sleep(1000);
        if (MouseMove(hDev, 50, 50))
            printf("Executed \n");
    }
   

    CloseHandle(hDev);
    return 0;
}
