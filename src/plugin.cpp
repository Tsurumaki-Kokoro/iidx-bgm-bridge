#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <stdexcept>
#include "spicesdk.h"
#include "../hosts/adapter.hpp"
#include "host_version.hpp"

static constexpr const char* kDays[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
static bgm::Profile activeProfile;
static std::string activeVariant;
using Open = uint32_t (__cdecl*)(const char*, uint16_t, int);
using Stat = int (__cdecl*)(const char*, void*);
static Open originalOpen = nullptr;
static Stat originalStat = nullptr;
static void** openSlot = nullptr;
static void** statSlot = nullptr;
static SPICE_SDK_V0 sdk{};
static LONG initialized = 0;
static SRWLOCK logLock = SRWLOCK_INIT;
static std::wstring root, logPath;
struct Entry {
    std::string source, target;
    std::wstring disk;
    LONG opened = 0;
};
static std::vector<Entry> entries;

static std::wstring wide(const std::string& text) {
    return std::wstring(text.begin(), text.end()); // internal paths are ASCII
}
static void logLine(const char* text, bool warning = false) {
    if (sdk.log) sdk.log(warning ? SPICE_SDK_LOG_LEVEL_WARNING : SPICE_SDK_LOG_LEVEL_INFO,
                         "bgm_bridge", text);
    OutputDebugStringA(text);
    if (logPath.empty()) return;
    AcquireSRWLockExclusive(&logLock);
    FILE* file = nullptr;
    if (_wfopen_s(&file, logPath.c_str(), L"ab") == 0 && file) {
        SYSTEMTIME t; GetLocalTime(&t);
        fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u %s\r\n", t.wYear, t.wMonth,
                t.wDay, t.wHour, t.wMinute, t.wSecond, text);
        fclose(file);
    }
    ReleaseSRWLockExclusive(&logLock);
}

static std::string normalize(const char* path) {
    if (!path) return {};
    std::string s(path);
    std::replace(s.begin(), s.end(), '\\', '/');
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (s.rfind("./", 0) == 0) s.erase(0, 2);
    if (s.rfind("data/", 0) == 0) s.insert(s.begin(), '/');
    return s;
}
static Entry* lookup(const char* path) {
    if (!path) return nullptr;
    const auto key = normalize(path);
    for (auto& entry : entries) if (entry.source == key) return &entry;
    return nullptr;
}
static uint32_t __cdecl hookedOpen(const char* path, uint16_t mode, int flags) {
    Entry* entry = nullptr;
    try { if (mode == 1) entry = lookup(path); } catch (...) { /* forward unchanged */ }
    if (entry) {
        const uint32_t handle = originalOpen(entry->target.c_str(), mode, flags);
        if (static_cast<int32_t>(handle) > 0) {
            if (InterlockedCompareExchange(&entry->opened, 1, 0) == 0) {
                try { logLine(("redirect: " + entry->source + " -> " + entry->target).c_str()); }
                catch (...) { /* logging must not break reads */ }
            }
            return handle;
        }
        try { logLine(("replacement open failed; using original: " + entry->source).c_str(), true); }
        catch (...) {}
    }
    return originalOpen(path, mode, flags);
}
static int __cdecl hookedStat(const char* path, void* stat) {
    Entry* entry = nullptr;
    try { entry = lookup(path); } catch (...) {}
    if (entry) {
        int result = originalStat(entry->target.c_str(), stat);
        if (result >= 0) return result;
    }
    return originalStat(path, stat);
}

static IMAGE_NT_HEADERS64* headers(HMODULE module) {
    auto base = reinterpret_cast<unsigned char*>(module);
    if (!base) return nullptr;
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return nullptr;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return nullptr;
    return nt;
}
static DWORD exportOrdinal(HMODULE module, const char* name) {
    auto nt = headers(module);
    if (!nt) return 0;
    auto base = reinterpret_cast<unsigned char*>(module);
    auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!rva) return 0;
    auto exp = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + rva);
    auto names = reinterpret_cast<DWORD*>(base + exp->AddressOfNames);
    auto ordinals = reinterpret_cast<WORD*>(base + exp->AddressOfNameOrdinals);
    for (DWORD i = 0; i < exp->NumberOfNames; ++i)
        if (strcmp(reinterpret_cast<char*>(base + names[i]), name) == 0)
            return exp->Base + ordinals[i];
    return 0;
}
static void** importSlot(HMODULE module, const char* library, DWORD ordinal, const char* name) {
    auto nt = headers(module);
    if (!nt || !ordinal) return nullptr;
    auto base = reinterpret_cast<unsigned char*>(module);
    auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return nullptr;
    auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
    for (; imp->Name; ++imp) {
        if (_stricmp(reinterpret_cast<char*>(base + imp->Name), library) != 0 || !imp->OriginalFirstThunk)
            continue;
        auto names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + imp->OriginalFirstThunk);
        auto pointers = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++pointers) {
            bool match = IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)
                ? IMAGE_ORDINAL64(names->u1.Ordinal) == ordinal
                : strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name,
                         name) == 0;
            if (match) return reinterpret_cast<void**>(&pointers->u1.Function);
        }
    }
    return nullptr;
}
static bool exchange(void** slot, void* expected, void* replacement) {
    DWORD oldProtect;
    if (!slot || !VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &oldProtect)) return false;
    void* previous = InterlockedCompareExchangePointer(slot, replacement, expected);
    DWORD ignored;
    VirtualProtect(slot, sizeof(void*), oldProtect, &ignored);
    return previous == expected;
}


static void __cdecl destroy() {
    bgmhost::stop();
    // Do not undo a later plugin's hook if it has replaced our slot.
    if (openSlot) exchange(openSlot, reinterpret_cast<void*>(hookedOpen), reinterpret_cast<void*>(originalOpen));
    if (statSlot) exchange(statSlot, reinterpret_cast<void*>(hookedStat), reinterpret_cast<void*>(originalStat));
}
static void makeEntries(const bgm::Profile& profile,const std::string& variant) {
    entries.clear();entries.reserve(profile.resources.size());
    for(const auto& m:profile.resources){Entry e;e.source=m.source;e.target=bgm::expand(m.target,variant);
        e.disk=root+wide(e.target.substr(1));entries.push_back(std::move(e));}
}
static std::wstring profilePath(const std::string& id){
    if(!bgm::safeId(id))throw std::runtime_error("unsafe profile id");
    return root+L"modules\\bgm_profiles\\"+wide(id)+L".ini";
}
static std::vector<std::wstring> loopFiles(){
    std::vector<std::wstring> files;
    for(unsigned layer=0;layer<3;++layer){
        auto source="/data/sound/system/0/syssd_bgm_default_l"+std::to_string(layer)+".sd9";
        for(auto& e:entries)if(e.source==source){files.push_back(e.disk);break;}}
    return files;
}
static bool selectProfile(const std::string& id,unsigned day){
    auto candidate=bgm::loadProfile(profilePath(id));
    if(!bgm::supportsHost(candidate,bgmhost::supported.family,bgmhost::supported.id))throw std::runtime_error("profile requires a different host adapter");
    std::string variant=candidate.schedule=="fixed"?candidate.fixedVariant:kDays[day];
    makeEntries(candidate,variant);
    for(auto& e:entries){DWORD attr=GetFileAttributesW(e.disk.c_str());
        if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_DIRECTORY))throw std::runtime_error("missing asset: "+e.target);}
    activeProfile=std::move(candidate);activeVariant=variant;
    return true;
}
static bool configure(HMODULE game) {
    wchar_t path[32768];
    DWORD count = GetModuleFileNameW(game, path, std::size(path));
    if (!count || count >= std::size(path)) return false;
    std::wstring gamePath(path, count);
    auto slash = gamePath.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return false;
    root = gamePath.substr(0, slash); // modules
    slash = root.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return false;
    root.resize(slash + 1);
    logPath = root + L"bgm_bridge.log";
    auto ini = root + L"modules\\bgm_bridge.ini";
    if (!GetPrivateProfileIntW(L"bgm_bridge", L"enabled", 1, ini.c_str())) {
        logLine("disabled by bgm_bridge.ini"); return false;
    }
    SPICE_SDK_AVS_INFO info{};
    std::string model,runtimeVersion;
    if(sdk.get_avs_info&&sdk.get_avs_info(&info)==SPICE_SDK_STATUS_SUCCESS){
        model.assign(info.model,strnlen_s(info.model,sizeof(info.model)));
        runtimeVersion.assign(info.ext,strnlen_s(info.ext,sizeof(info.ext)));
    }
    auto detected=bgm::resolveHostVersion(bgm::metadataText(root+L"prop\\bootstrap.xml"),
                                          bgm::metadataText(root+L"prop\\ea3-config.xml"),model,runtimeVersion);
    if (!bgmhost::select(detected.version)) {
        logLine(("unsupported host version="+detected.version).c_str(),true);return false;
    }
    logLine(("host version="+detected.version+" source="+detected.source).c_str());
    if(!bgmhost::validLayout(game)){
        logLine("host audio layout does not match this version; no hooks installed",true);return false;
    }
    wchar_t value[32];
    GetPrivateProfileStringW(L"bgm_bridge", L"weekday", L"auto", value, std::size(value), ini.c_str());
    SYSTEMTIME local; GetLocalTime(&local);
    unsigned day = local.wDayOfWeek;
    if (_wcsicmp(value, L"auto") != 0) {
        bool found = false;
        for (unsigned i = 0; i < 7; ++i) if (_wcsicmp(value, wide(kDays[i]).c_str()) == 0) {
            day = i; found = true; break;
        }
        if (!found) { logLine("invalid weekday in bgm_bridge.ini; expected auto/sun/mon/tue/wed/thu/fri/sat", true); return false; }
    }
    wchar_t selected[80];
    GetPrivateProfileStringW(L"bgm_bridge",L"profile",L"sinobuz24",selected,std::size(selected),ini.c_str());
    std::string id; for (wchar_t ch : std::wstring(selected)) { if (ch > 127) throw std::runtime_error("non-ASCII profile ID"); id.push_back(static_cast<char>(ch)); }
    wchar_t fallback[80];
    GetPrivateProfileStringW(L"bgm_bridge",L"fallback_profile",L"sinobuz24_compat",fallback,std::size(fallback),ini.c_str());
    std::string fallbackId; for (wchar_t ch : std::wstring(fallback)) { if (ch > 127) throw std::runtime_error("non-ASCII fallback ID"); fallbackId.push_back(static_cast<char>(ch)); }
    bool controllerEnabled=GetPrivateProfileIntW(L"bgm_bridge",L"smooth_transition",1,ini.c_str())!=0;
    if(!controllerEnabled){id=fallbackId;}
    try {selectProfile(id,day);}
    catch(const std::exception& e){logLine(e.what(),true);if(id==fallbackId)throw;selectProfile(fallbackId,day);}
    if(!controllerEnabled){
        if(activeProfile.strategy==bgm::Strategy::Staged)throw std::runtime_error("staged tracks require the playback controller");
        activeProfile.strategy=bgm::Strategy::Original;
    }
    if(!bgmhost::install(game,activeProfile,loopFiles(),logLine)){
        auto next=activeProfile.fallback.empty()?fallbackId:activeProfile.fallback;
        logLine("profile controller unavailable; selecting complete-mix fallback",true);
        bgmhost::stop();selectProfile(next,day);
        if(activeProfile.strategy==bgm::Strategy::Staged)throw std::runtime_error("staged fallback requires unavailable playback hooks");
        activeProfile.strategy=bgm::Strategy::Original;
    }
    logLine(("selected profile="+activeProfile.id+" variant="+activeVariant+"; fixed until restart").c_str());
    return true;
}

extern "C" __declspec(dllexport) int __cdecl spice_sdk_entry_point(spice_sdk_init_func* init) {
    if (!init || InterlockedCompareExchange(&initialized, 1, 0) != 0) return 1;
    try {
        sdk.size = sizeof(sdk);
        if (init(0, destroy, &sdk) != SPICE_SDK_STATUS_SUCCESS) return 1;
        auto game = GetModuleHandleW(L"bm2dx.dll");
        auto avs = GetModuleHandleW(L"avs2-core.dll");
        if (!game || !avs) { logLine("game or AVS module missing; no hooks installed", true); return 1; }
        if (!configure(game)) return 1;
        openSlot = importSlot(game, "avs2-core.dll", exportOrdinal(avs, bgmhost::supported.avsOpen), bgmhost::supported.avsOpen);
        statSlot = importSlot(game, "avs2-core.dll", exportOrdinal(avs, bgmhost::supported.avsStat), bgmhost::supported.avsStat);
        if (!openSlot || !statSlot) { destroy(); logLine("AVS import slots missing; no hooks installed", true); return 1; }
        originalOpen = reinterpret_cast<Open>(*openSlot);
        originalStat = reinterpret_cast<Stat>(*statSlot);
        if (!exchange(openSlot, reinterpret_cast<void*>(originalOpen), reinterpret_cast<void*>(hookedOpen))) {
            destroy(); logLine("open hook installation failed", true); return 1;
        }
        if (!exchange(statSlot, reinterpret_cast<void*>(originalStat), reinterpret_cast<void*>(hookedStat))) {
            destroy(); logLine("stat hook installation failed; open hook rolled back", true); return 1;
        }
        logLine((std::string("ready: profile-driven BGM engine; host=")+bgmhost::supported.id).c_str());
        return 0;
    } catch (const std::exception& error) {
        destroy();logLine((std::string("initialization failed; hooks rolled back: ")+error.what()).c_str(),true);return 1;
    } catch (...) { destroy(); logLine("initialization failed; hooks rolled back", true); return 1; }
}
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
