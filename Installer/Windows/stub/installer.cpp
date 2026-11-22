// Tiny self-contained Windows installer for NF Vocal Sunshine (cross-compiled on the Mac, no Inno Setup / NSIS needed).
// The plug-in DLL is embedded as resource 1 (RCDATA). Run it: asks, installs the VST3 for all users, registers an uninstall entry.
// "/uninstall" removes everything again. "/silent" skips the dialogs.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

#ifndef NFVS_VERSION
 #define NFVS_VERSION "1.0.0"
#endif
static const wchar_t* kTitle = L"NF Vocal Sunshine";
static const wchar_t* kUninstKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NF Vocal Sunshine";

static std::wstring knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR p = nullptr; std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) r = p;
    CoTaskMemFree(p);
    return r;
}
static bool removeDir(const std::wstring& dir)
{
    std::wstring from = dir + L'\0';
    SHFILEOPSTRUCTW op = {}; op.wFunc = FO_DELETE; op.pFrom = from.c_str(); op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0;
}
static bool silent = false;
static void say(const wchar_t* text, UINT icon) { if (!silent) MessageBoxW(nullptr, text, kTitle, MB_OK | icon); }

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int)
{
    const std::wstring cmd = GetCommandLineW();
    silent = cmd.find(L"/silent") != std::wstring::npos;
    const bool uninstall = cmd.find(L"/uninstall") != std::wstring::npos;
    const std::wstring vst3Dir = knownFolder(FOLDERID_ProgramFilesCommon) + L"\\VST3\\NF Vocal Sunshine.vst3";
    const std::wstring appDir = knownFolder(FOLDERID_ProgramFiles) + L"\\NF Audio Tools\\NF Vocal Sunshine";

    if (uninstall)
    {
        if (!silent && MessageBoxW(nullptr, L"Remove NF Vocal Sunshine from this computer?", kTitle, MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
        removeDir(vst3Dir);
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, kUninstKey);
        // the running uninstaller cannot delete itself; leave a note for the next reboot
        wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
        MoveFileExW(self, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(appDir.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        say(L"NF Vocal Sunshine was removed.", MB_ICONINFORMATION);
        return 0;
    }

    if (!silent && MessageBoxW(nullptr, L"Install NF Vocal Sunshine " L"" NFVS_WVERSION L" (VST3) for all users?\n\nNF Audio Tools by Nenno Fernando", kTitle, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return 0;

    HRSRC res = FindResourceW(inst, MAKEINTRESOURCEW(1), RT_RCDATA);
    HGLOBAL h = res ? LoadResource(inst, res) : nullptr;
    const void* data = h ? LockResource(h) : nullptr;
    const DWORD size = res ? SizeofResource(inst, res) : 0;
    if (!data || !size) { say(L"The installer is damaged (no plug-in inside).", MB_ICONERROR); return 1; }

    const std::wstring binDir = vst3Dir + L"\\Contents\\x86_64-win";
    if (SHCreateDirectoryExW(nullptr, binDir.c_str(), nullptr) != ERROR_SUCCESS && GetFileAttributesW(binDir.c_str()) == INVALID_FILE_ATTRIBUTES)
    { say(L"Could not create the VST3 folder. Run the installer as administrator.", MB_ICONERROR); return 1; }
    const std::wstring dll = binDir + L"\\NF Vocal Sunshine.vst3";
    HANDLE f = CreateFileW(dll.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    if (f == INVALID_HANDLE_VALUE || !WriteFile(f, data, size, &written, nullptr) || written != size)
    { if (f != INVALID_HANDLE_VALUE) CloseHandle(f); say(L"Could not write the plug-in. Close your DAW and try again.", MB_ICONERROR); return 1; }
    CloseHandle(f);

    // keep a copy of this installer as the uninstaller
    SHCreateDirectoryExW(nullptr, appDir.c_str(), nullptr);
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::wstring unPath = appDir + L"\\Uninstall NF Vocal Sunshine.exe";
    CopyFileW(self, unPath.c_str(), FALSE);
    HKEY k;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kUninstKey, 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &k, nullptr) == ERROR_SUCCESS)
    {
        auto set = [&](const wchar_t* n, const std::wstring& v) { RegSetValueExW(k, n, 0, REG_SZ, (const BYTE*) v.c_str(), (DWORD) ((v.size() + 1) * sizeof(wchar_t))); };
        set(L"DisplayName", L"NF Vocal Sunshine"); set(L"DisplayVersion", NFVS_WVERSION); set(L"Publisher", L"NF Audio Tools");
        set(L"UninstallString", L"\"" + unPath + L"\" /uninstall");
        RegCloseKey(k);
    }
    say(L"NF Vocal Sunshine was installed.\n\nRescan your plug-ins in your DAW. Thank you for choosing NF Audio Tools.", MB_ICONINFORMATION);
    return 0;
}
