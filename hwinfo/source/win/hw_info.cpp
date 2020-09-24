#include "hw_info.h"
#include <iostream>

#pragma comment(lib, "wbemuuid.lib")
using namespace std;

HRESULT        hres;
IWbemLocator*  pLoc = NULL;
IWbemServices* pSvc = NULL;

static std::wstring CharToWchar(const char* c, size_t m_encode = CP_ACP) {
    std::wstring str;
    int          len     = MultiByteToWideChar(m_encode, 0, c, strlen(c), NULL, 0);
    wchar_t*     m_wchar = new wchar_t[len + 1];
    MultiByteToWideChar(m_encode, 0, c, strlen(c), m_wchar, len);
    m_wchar[len] = '\0';
    str          = m_wchar;
    delete m_wchar;
    return str;
}

static std::string WcharToChar(const wchar_t* wp, size_t m_encode = CP_ACP) {
    std::string str;
    int         len    = WideCharToMultiByte(m_encode, 0, wp, wcslen(wp), NULL, 0, NULL, NULL);
    char*       m_char = new char[len + 1];
    WideCharToMultiByte(m_encode, 0, wp, wcslen(wp), m_char, len, NULL, NULL);
    m_char[len] = '\0';
    str         = m_char;
    delete m_char;
    return str;
}

bool hw_init() {
    bool ret = false;
    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        cout << "Failed to initialize COM library. Error code = 0x" << hex << hres << endl;
        return ret;  // Program has failed.
    }

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------

    hres = CoInitializeSecurity(NULL,
                                -1,                           // COM authentication
                                NULL,                         // Authentication services
                                NULL,                         // Reserved
                                RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
                                RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
                                NULL,                         // Authentication info
                                EOAC_NONE,                    // Additional capabilities
                                NULL                          // Reserved
    );

    if (FAILED(hres)) {
        cout << "Failed to initialize security. Error code = 0x" << hex << hres << endl;
        CoUninitialize();
        return ret;  // Program has failed.
    }

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI ------------------------

    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres)) {
        cout << "Failed to create IWbemLocator object."
             << " Err code = 0x" << hex << hres << endl;
        CoUninitialize();
        return ret;  // Program has failed.
    }

    // Step 4: -----------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method

    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
                               NULL,                     // User name. NULL = current user
                               NULL,                     // User password. NULL = current
                               0,                        // Locale. NULL indicates current
                               NULL,                     // Security flags.
                               0,                        // Authority (for example, Kerberos)
                               0,                        // Context object
                               &pSvc                     // pointer to IWbemServices proxy
    );

    if (FAILED(hres)) {
        cout << "Could not connect. Error code = 0x" << hex << hres << endl;
        pLoc->Release();
        CoUninitialize();
        return ret;  // Program has failed.
    }

    cout << "Connected to ROOT\\CIMV2 WMI namespace" << endl;

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------

    hres = CoSetProxyBlanket(pSvc,                         // Indicates the proxy to set
                             RPC_C_AUTHN_WINNT,            // RPC_C_AUTHN_xxx
                             RPC_C_AUTHZ_NONE,             // RPC_C_AUTHZ_xxx
                             NULL,                         // Server principal name
                             RPC_C_AUTHN_LEVEL_CALL,       // RPC_C_AUTHN_LEVEL_xxx
                             RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
                             NULL,                         // client identity
                             EOAC_NONE                     // proxy capabilities
    );

    if (FAILED(hres)) {
        cout << "Could not set proxy blanket. Error code = 0x" << hex << hres << endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return ret;  // Program has failed.
    }
    return true;
}

bool hw_cleanup() {
    if (pSvc != NULL) {
        pSvc->Release();
    }
    if (pLoc != NULL) {
        pLoc->Release();
    }
    CoUninitialize();
    return true;
}

bool get_os_disk(int& num) {
    
    bool ret = false;
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT Name FROM Win32_OperatingSystem"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
                           &pEnumerator);

    if (FAILED(hres)) {
        cout << "Query for board serialnum failed."
             << " Error code = 0x" << hex << hres << endl;
        return false;  // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG             uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        // Get the value of the Name property
        hr     = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        std::string name = WcharToChar(vtProp.bstrVal);
        VariantClear(&vtProp);

        pclsObj->Release();

        int pos = name.find("Harddisk") + strlen("Harddisk");
        name = name.substr(pos );
        num = atoi(name.c_str());
        ret = true;
    }

    pEnumerator->Release();
    return ret;

}
bool get_disk_id(std::string& serial) {
   
    int num = 0;
    bool ret = false;
      if (!get_os_disk(num)) {
          return false;
      }
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT SerialNumber,DeviceID FROM Win32_DiskDrive"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
                           &pEnumerator);

    if (FAILED(hres)) {
        cout << "Query for board serialnum failed."
             << " Error code = 0x" << hex << hres << endl;
        return false;  // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG             uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;
        // Get the value of the DeviceId property
        hr     = pclsObj->Get(L"DeviceId", 0, &vtProp, 0, 0);
        std::string deviceId = WcharToChar(vtProp.bstrVal);
        VariantClear(&vtProp);

        int pos  = deviceId.find("PHYSICALDRIVE") + strlen("PHYSICALDRIVE");
        deviceId = deviceId.substr(pos);
        if (num == atoi(deviceId.c_str())) {
            // Get the value of the Name property
            hr     = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
            serial = WcharToChar(vtProp.bstrVal);
            VariantClear(&vtProp);
            ret = true;
            break;
        }
        pclsObj->Release();
    }

    pEnumerator->Release();
    return ret;

}

bool get_board_serial_number(std::string& board_serial) {
    bool                  ret         = false;
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_BaseBoard"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
                           &pEnumerator);

    if (FAILED(hres)) {
        cout << "Query for board serialnum failed."
             << " Error code = 0x" << hex << hres << endl;
        return false;  // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG             uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        // Get the value of the Name property
        hr           = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
        board_serial = WcharToChar(vtProp.bstrVal);
        VariantClear(&vtProp);

        pclsObj->Release();
        ret = true;
    }

    pEnumerator->Release();
    return ret;
}

bool get_cpu_id(std::string& cpu_id) {
    bool ret = false;

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Processor"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
                           &pEnumerator);

    if (FAILED(hres)) {
        cout << "Query for board serialnum failed."
             << " Error code = 0x" << hex << hres << endl;
        return false;  // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG             uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        // Get the value of the Name property
        hr     = pclsObj->Get(L"ProcessorId", 0, &vtProp, 0, 0);
        cpu_id = WcharToChar(vtProp.bstrVal);
        VariantClear(&vtProp);

        pclsObj->Release();
        ret = true;
    }

    pEnumerator->Release();
    return ret;
}

bool get_mac_address(std::string& mac_address) {
    return true;
}
