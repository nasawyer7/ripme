#include "general.h"
#include "irpHandlers.h"
#include <libwsk.h>
#include <ntddk.h>
#include <wsk.h>


extern "C" void AssemFunc(UINT64 InputValue);


BOOLEAN StopServer = FALSE;
PVOID ThreadObject = NULL;
PVOID WskDataBuffer = NULL;
SOCKET GlobalListenSocket = NULL;


int HexCharToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}


UINT64 ParseHexString(const char* str) {
    UINT64 result = 0;
    while (*str) {
        char c = *str;
        // Basic hex validation
        if (!((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f'))) {
            break;
        }
        result = (result << 4) | HexCharToInt(c);
        str++;
    }
    return result;
}


char* FindString(char* buffer, SIZE_T bufferLen, const char* target) {
    SIZE_T targetLen = strlen(target);
    if (targetLen > bufferLen) return NULL;
    for (SIZE_T i = 0; i <= bufferLen - targetLen; i++) {
        BOOLEAN match = TRUE;
        for (SIZE_T j = 0; j < targetLen; j++) {
            if (buffer[i + j] != target[j]) {
                match = FALSE;
                break;
            }
        }
        if (match) return &buffer[i];
    }
    return NULL;
}


void RunWebServer(PVOID Context) {
    UNREFERENCED_PARAMETER(Context);

    NTSTATUS status;

   
    const char* SinglePageHtml =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
        "<html><body style='font-family:sans-serif; text-align:center; padding-top:50px;'>"
        "<h1>Control the RIP register</h1>"
        "<form method='POST' action=''>"
        "<input type='text' name='kinput' value='hex no 0x' style='font-size:20px; padding:5px;'>"
        "<br><br>"
        "<input type='submit' value='Jump to input value' style='font-size:20px; padding:10px; font-weight:bold;'>"
        "</form>"
        "<p style='color:gray'>Clicking above immediantly jumps to RIP. Will likely crash the webserver</p>"
        "</body></html>";

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, " [WSK] (+) Web Server Thread Started.\n");

    // 1. Create Socket
    status = WSKSocket(&GlobalListenSocket, AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL);
    if (!NT_SUCCESS(status)) {
        PsTerminateSystemThread(status);
        return;
    }

    // 2. Bind
    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = RtlUshortByteSwap(80);
    serverAddr.sin_addr.s_addr = 0;

    status = WSKBind(GlobalListenSocket, (PSOCKADDR)&serverAddr, sizeof(serverAddr));
    if (!NT_SUCCESS(status)) {
        WSKCloseSocket(GlobalListenSocket);
        PsTerminateSystemThread(status);
        return;
    }

    // 3. Listen
    WSKListen(GlobalListenSocket, 10);

    // 4. Accept Loop
    while (!StopServer) {
        SOCKET clientSocket = NULL;
        SOCKADDR_IN localInfo = { 0 };
        SOCKADDR_IN remoteInfo = { 0 };

        status = WSKAccept(
            GlobalListenSocket, &clientSocket,
            (PSOCKADDR)&localInfo, sizeof(localInfo),
            (PSOCKADDR)&remoteInfo, sizeof(remoteInfo)
        );

        if (NT_SUCCESS(status) && clientSocket != NULL) {
            char RecvBuf[4096];
            RtlZeroMemory(RecvBuf, sizeof(RecvBuf));
            SIZE_T bytesReceived = 0;

            status = WSKReceive(clientSocket, RecvBuf, sizeof(RecvBuf) - 1, &bytesReceived, 0, NULL, NULL);

            if (NT_SUCCESS(status) && bytesReceived > 0) {
                RecvBuf[bytesReceived] = '\0';

                
                // If it is a POST, we assume user clicked the button
                if (RecvBuf[0] == 'P' && RecvBuf[1] == 'O') {
                    char* bodyStart = FindString(RecvBuf, bytesReceived, "\r\n\r\n");
                    if (bodyStart) {
                        bodyStart += 4;
                        char* varStart = FindString(bodyStart, strlen(bodyStart), "kinput=");
                        if (varStart) {
                            varStart += 7;
                            char hexBuffer[64] = { 0 };
                            int i = 0;

                            // Extract value
                            while (varStart[i] != '\0' && varStart[i] != '&' && i < 63) {
                                if (varStart[i] != '+') {
                                    hexBuffer[i] = varStart[i];
                                }
                                i++;
                            }
                            hexBuffer[i] = '\0';

                            // 1. Parse
                            UINT64 parsedValue = ParseHexString(hexBuffer);

                            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                                " [WSK] EXECUTE: 0x%llX\n", parsedValue);

                            // 2. Execute Assembly Immediately
                            AssemFunc(parsedValue);
                        }
                    }
                }

                // 3. Always send the same page back (No redirect, no "Thank You" screen)
                SIZE_T bytesSent = 0;
                WSKSend(clientSocket, (PVOID)SinglePageHtml, (SIZE_T)strlen(SinglePageHtml), &bytesSent, 0, NULL, NULL);
            }

            WSKDisconnect(clientSocket, NULL);
            WSKCloseSocket(clientSocket);
        }
        else {
            break;
        }
    }

    if (GlobalListenSocket) {
        WSKCloseSocket(GlobalListenSocket);
        GlobalListenSocket = NULL;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, " [WSK] Server Thread Exiting.\n");
    PsTerminateSystemThread(STATUS_SUCCESS);
}












extern "C" {
    VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {
        UNREFERENCED_PARAMETER(DriverObject);
        StopServer = TRUE;
        if (GlobalListenSocket) {
            WSKCloseSocket(GlobalListenSocket);
            GlobalListenSocket = NULL;
        }
        if (ThreadObject) {
            KeWaitForSingleObject(ThreadObject, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(ThreadObject);
        }
        WSKCleanup();
        if (WskDataBuffer) ExFreePool(WskDataBuffer);
    }

    NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
        UNREFERENCED_PARAMETER(RegistryPath);
        NTSTATUS status;
        HANDLE threadHandle;

        WskDataBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, 2048, 'KSW');
        if (!WskDataBuffer) return STATUS_INSUFFICIENT_RESOURCES;

        status = WSKStartup(MAKE_WSK_VERSION(1, 0), (WSKDATA*)WskDataBuffer);
        if (!NT_SUCCESS(status)) { ExFreePool(WskDataBuffer); return status; }

        status = PsCreateSystemThread(&threadHandle, (ACCESS_MASK)0, NULL, (HANDLE)0, NULL, (PKSTART_ROUTINE)RunWebServer, NULL);
        if (!NT_SUCCESS(status)) { WSKCleanup(); ExFreePool(WskDataBuffer); return status; }

        status = ObReferenceObjectByHandle(threadHandle, THREAD_ALL_ACCESS, NULL, KernelMode, &ThreadObject, NULL);
        ZwClose(threadHandle);

        DriverObject->DriverUnload = UnloadDriver;
        return STATUS_SUCCESS;
    }
}