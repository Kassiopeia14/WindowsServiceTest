#include <windows.h>
#include <string>
#include "TestServiceMessages.h"

#include <iostream>
#include <fstream>

std::wstring 
    serviceName(L"TestService"),
    serviceDisplayName(L"Test Service"),
    servicePath(L"D:\\Projects\\GitHub\\Stanford\\x64\\Release\\TestService.exe");

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain();

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit();
VOID SvcReportEvent(std::wstring);

int main(int argc, char* argv[])
{
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.

    if ((argc > 1) && strcmp(argv[1], "install") == 0)
    {
        SvcInstall();
        return 0;
    }

    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { (LPWSTR)serviceName.c_str(), (LPSERVICE_MAIN_FUNCTION)SvcMain},
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        SvcReportEvent(L"Test Service Started");
    }
}

VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        std::cout << "OpenSCManager failed, error code: " << GetLastError() << std::endl;
        return;
    }

    // Create the service

    schService = CreateService(
        schSCManager,              // SCM database 
        serviceName.c_str(),       // name of service 
        serviceDisplayName.c_str(),// service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        servicePath.c_str(),       // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL)
    {
        std::cout << "CreateService failed, error code: " << GetLastError() << std::endl;
        CloseServiceHandle(schSCManager);
        return;
    }
    else
    {
        std::cout << "Service installed successfully" << std::endl;
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

VOID WINAPI SvcMain()
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler(
        serviceName.c_str(),
        SvcCtrlHandler);

    if (!gSvcStatusHandle)
    {
        SvcReportEvent(L"Test Service NOT registered");
        return;
    }
    else
    {
        SvcReportEvent(L"Test Service registered successfully");
    }

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit();
}


VOID SvcInit()
{
    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // Create file

    std::ofstream MyFile("D:\\out.txt");
    for (int i = 0; i < 60; i++)
    {
        MyFile << "Step " << i << std::endl;
        Sleep(1000);
    }
    MyFile.close();

    while (1)
    {
        // Check whether to stop the service.

        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
}

VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    // Handle the requested control code. 

    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }

}

VOID SvcReportEvent(std::wstring message)
{
    HANDLE hEventSource;
    LPCWSTR strings[1] = { message.c_str() };

    hEventSource = RegisterEventSource(NULL, serviceName.c_str());

    if (NULL != hEventSource)
    {
        ReportEvent(hEventSource,// event log handle
            EVENTLOG_INFORMATION_TYPE, // event type
            0,                   // event category
            INFORMATION_EVENT,     // event identifier
            NULL,                // no security identifier
            1,                   // size of lpszStrings array
            0,                   // no binary data
            strings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}

/*
    mc -U TestServiceMessages.mc
    rc -r TestServiceMessages.rc
    link -dll -noentry -out:"..\x64\Release\TestServiceMessages.dll" TestServiceMessages.res
*/