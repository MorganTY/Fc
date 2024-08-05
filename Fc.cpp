// Fc.cpp : This file contains the 'main' function. Program execution begins and ends there.
//#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <sddl.h>
#include <string>
#include <Lmcons.h>
#include <strsafe.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>


using namespace std;

// Function for HKEY_USERS
std::wstring EnumerateHKeyUsersWithPrefix(const wchar_t* prefix, const wchar_t* ignoreSubstring) {
    std::wstring resultString;
    HKEY hKey;
    LONG result;

    // Open the HKEY_USERS key
    result = RegOpenKeyEx(HKEY_USERS, L"", 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    if (result != ERROR_SUCCESS) {
        resultString = L"Failed to open HKEY_USERS.";
        return resultString;
    }

    // Enumerate subkeys under HKEY_USERS
    WCHAR subKeyName[256];
    DWORD subKeyNameSize = ARRAYSIZE(subKeyName);
    DWORD index = 0;

    while (true) {
        result = RegEnumKeyEx(hKey, index++, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);
        if (result == ERROR_SUCCESS) {
            // Check if the subkey name starts with the specified prefix and does not contain the ignore substring
            if (wcsncmp(subKeyName, prefix, wcslen(prefix)) == 0 && !wcsstr(subKeyName, ignoreSubstring)) {
                resultString += subKeyName;
                resultString += L"";
            }
            subKeyNameSize = ARRAYSIZE(subKeyName); // Reset size for next enumeration
        }
        else if (result == ERROR_NO_MORE_ITEMS) {
            break; // No more subkeys
        }
        else {
            resultString = L"Failed to enumerate subkeys under HKEY_USERS.";
            break;
        }
    }

    // Close HKEY_USERS
    RegCloseKey(hKey);

    return resultString;
}
// Function to get the current username
std::wstring getUsername() {
    TCHAR username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserName(username, &size)) {
        return std::wstring(username);
    }
    else {
        std::wcerr << L"Failed to get username. Error: " << GetLastError() << std::endl;
        return L"";
    }
}

// Utility function to convert wide string to narrow string
std::string WideStringToString(const std::wstring& wstr) {
    int bufferSize = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(bufferSize, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], bufferSize, NULL, NULL);
    return str;
}

// Function to enumerate and display registry values
void DisplayRegistryValues(HKEY hKeyRoot, LPCTSTR subKey) {
    HKEY hKey;
    if (RegOpenKeyEx(hKeyRoot, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        std::cerr << "Failed to open registry key." << std::endl;
        return;
    }

    TCHAR valueName[256];
    BYTE valueData[1024];
    DWORD valueNameSize;
    DWORD valueDataSize;
    DWORD index = 0;
    DWORD type;

    while (true) {
        valueNameSize = sizeof(valueName) / sizeof(valueName[0]);
        valueDataSize = sizeof(valueData);

        LONG result = RegEnumValue(hKey, index, valueName, &valueNameSize, NULL, &type, valueData, &valueDataSize);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        else if (result != ERROR_SUCCESS) {
            std::cerr << "Failed to enumerate registry values." << std::endl;
            break;
        }

        std::wstring wValueData(reinterpret_cast<wchar_t*>(valueData), valueDataSize / sizeof(wchar_t));
        std::string valueDataStr = WideStringToString(wValueData);

        std::wcout << L"Value Name: " << valueName << L", Value Data: " << valueDataStr.c_str() << std::endl;

        index++;
    }

    RegCloseKey(hKey);
}

// Function to process HKEY_USERS and check specific subkeys
void ProcessHKeyUsers() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_USERS, NULL, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        std::cerr << "Failed to open HKEY_USERS." << std::endl;
        return;
    }

    TCHAR subKeyName[256];
    DWORD subKeyNameSize;
    DWORD index = 0;

    while (true) {
        subKeyNameSize = sizeof(subKeyName) / sizeof(subKeyName[0]);

        LONG result = RegEnumKeyEx(hKey, index, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        else if (result != ERROR_SUCCESS) {
            std::cerr << "Failed to enumerate subkeys in HKEY_USERS." << std::endl;
            break;
        }

        std::wstring wSubKeyName(subKeyName);

        // Check if the subkey name starts with S-1-5-21- and does not contain "Classes"
        if (wSubKeyName.find(L"S-1-5-21-") == 0 && wSubKeyName.find(L"Classes") == std::wstring::npos) {
            std::wcout << L"Processing key: " << wSubKeyName << std::endl;

            // Construct the full paths for the subkeys
            std::wstring appCompatPath = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store";
            std::wstring featureUsagePath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FeatureUsage\\AppSwitched";

            std::wstring appCompatFullPath = L"HKEY_USERS\\" + wSubKeyName + L"\\" + appCompatPath;
            std::wstring featureUsageFullPath = L"HKEY_USERS\\" + wSubKeyName + L"\\" + featureUsagePath;
            std::wcout << L"\n\n";
            std::wcout << L"\n\nValues under path: " << appCompatFullPath << std::endl;
            std::wcout << L"\n\n";
            DisplayRegistryValues(HKEY_USERS, (wSubKeyName + L"\\" + appCompatPath).c_str());
            std::wcout << L"\n\n";
            std::wcout << L"Values under path: " << featureUsageFullPath << std::endl;
            std::wcout << L"\n\n";
            DisplayRegistryValues(HKEY_USERS, (wSubKeyName + L"\\" + featureUsagePath).c_str());
            std::wcout << L"\n\n";

        }

        index++;
    }

    RegCloseKey(hKey);

}
namespace fs = std::filesystem;

// Function to display contents of a directory
void displayDirectoryContents(const std::wstring& dirPath, bool showFiles = true, bool showDirectories = true) {
    try {
        // Check if the given path is a directory
        if (!fs::is_directory(dirPath)) {
            std::wcerr << L"Error: " << dirPath << L" is not a directory or cannot be accessed." << std::endl;
            return;
        }

        // Iterate through the directory contents
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (showFiles && fs::is_regular_file(entry.status())) {
                std::wcout << L"File: " << entry.path().filename().wstring() << std::endl;
            }
            if (showDirectories && fs::is_directory(entry.status())) {
                std::wcout << L"Directory: " << entry.path().filename().wstring() << std::endl;
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::wcerr << L"Filesystem error: " << e.what() << std::endl;
    }
}

// Main function to demonstrate file and registry key deletion
int main() {
    //Main Var
    bool showFiles = true;      
    bool showDirectories = true;
    std::string input;
    std::wstring username = getUsername();
    std::wcout << L"Your Current Account Name: " << username << L"\n" << endl;
    //INI Del
    //Unleashed Var
    std::wstring roamingDirectoryUnleashed = L"C:\\Users\\" + username + L"\\AppData\\Roaming\\Unleashed";
    std::wstring acFileCrash = L"C:\\Users\\" + username + L"\\AppData\\Local\\Packages\\StudioWildcard.4558480580BB9_1w2mm55455e38\\Ac";
    std::wstring acFolderUnleashed = L"C:\\Users\\" + username + L"\\AppData\\Local\\Packages\\StudioWildcard.4558480580BB9_1w2mm55455e38\\Ac\\UNLEASHED";
    //Headshot Var
    std::wstring hsLogFolder = L"C:\\Users\\" + username + L"\\AppData\\Local\\Packages\\StudioWildcard.4558480580BB9_1w2mm55455e38\\AC\\Temp\\HeadshotLogs";
    std::wstring hsConfigFolder = L"C:\\Users\\" + username + L"\\AppData\\Local\\Packages\\StudioWildcard.4558480580BB9_1w2mm55455e38\\AC\\Temp\\HeadshotConfigs";
    std::wstring winTemp = L"C:\\Users\\" + username + L"\\AppData\\Local\\Temp";
    std::wstring registryAppSwitched = L"";
    //Shared Val
    std::wstring winFileShell = L"C:\\Users\\" + username + L"\\AppData\\Local\\Microsoft\\Windows\\Shell"; // Find??
    std::wstring winFileRecent = L"C:\\Users\\" + username + L"\\AppData\\Roaming\\Microsoft\\Windows\\Recent"; // 
    std::wstring hkeyMui = L"HKEY_CURRENT_USER\\Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache"; //Further Work

    int choice;
    std::cout << " Welcome to Fems File Checker V1\n ";
    std::cout << "Please Pick an Option to get started\n ";
    std::cout << "Remeber you are looking for Primal, Unleashed(Client_3, Headshot (Hs Hsloader \n";
    while (true) {
    // Display menu options to the user

    std::cout << "\n[1] Check MUI Cache \n";
    std::cout << "[2] Check HKEY Users\n";
    std::cout << "[3] Check AC folder\n";
    std::cout << "[4] Check Recent folder\n";
    std::cout << "[5] Exit\n";
    std::cin >> choice;

    // Use switch statement to handle different cases
    switch (choice) {
    case 1:
        std::cout << "\n\nPlease READ through the Options\n";
        DisplayRegistryValues(HKEY_CURRENT_USER, _T("Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache"));
        break;
    case 2:
        std::cout << "\n\nPlease READ through the Options\n";
        ProcessHKeyUsers();
        break;
    case 3:
        std::cout << "\n\nFolders in AC Folder\n";
        displayDirectoryContents(acFileCrash);
        break;
    case 4:
        std::cout << "\n\nFolders in Recent\n";
       
            displayDirectoryContents(winFileRecent);
        break;
    case 5:
        return 0;
    
    
    
    default:
        std::cout << "Invalid choice. Please Enter an Option" << std::endl;
        break;




    }
    
    }
    return 0;
}