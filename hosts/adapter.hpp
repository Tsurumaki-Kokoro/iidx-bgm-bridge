#pragma once
#include <windows.h>
#include <memory>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../include/playback.hpp"
#include "../vendor/minhook/include/MinHook.h"

#include "builds.hpp"

namespace bgmhost {
// All executable-specific knowledge is confined to this adapter.
inline Spec supported=iidx34;
inline bool select(const std::string& version){
    for(const auto& candidate:builds)if(version==candidate.version||(!version.empty()&&version==candidate.versionAlias)){supported=candidate;return true;}
    return false;
}
inline bool validLayout(HMODULE game){
    if(!game)return false;
    auto base=reinterpret_cast<const unsigned char*>(game);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0x40||dos->e_lfanew>0x1000)return false;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
       nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||!nt->FileHeader.NumberOfSections||
       nt->FileHeader.NumberOfSections>96)return false;
    auto executable=[&](uintptr_t rva,size_t length){
        if(rva>=nt->OptionalHeader.SizeOfImage||length>nt->OptionalHeader.SizeOfImage-rva)return false;
        auto section=IMAGE_FIRST_SECTION(nt);
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
            uint64_t end=static_cast<uint64_t>(section[i].VirtualAddress)+std::max(section[i].Misc.VirtualSize,section[i].SizeOfRawData);
            if((section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)&&rva>=section[i].VirtualAddress&&rva+length<=end)return true;
        }
        return false;
    };
    auto matches=[&](uintptr_t rva,std::initializer_list<unsigned char> bytes){
        return executable(rva,bytes.size())&&memcmp(base+rva,bytes.begin(),bytes.size())==0;
    };
    auto call=[&](uintptr_t site,uintptr_t target){
        if(!executable(site,5)||base[site]!=0xe8)return false;
        int32_t displacement=0;memcpy(&displacement,base+site+1,4);
        return static_cast<int64_t>(site)+5+displacement==static_cast<int64_t>(target);
    };
    return matches(supported.advance,{0x40,0x53,0x48,0x83,0xec,0x20})&&
        matches(supported.target,{0x48,0x89,0x5c,0x24,0x08,0x57})&&
        matches(supported.lookup,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x63})&&
        matches(supported.playing,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74})&&
        matches(supported.volume,{0x48,0x89,0x5c,0x24,0x08,0x57})&&
        matches(supported.custom,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74})&&
        matches(supported.customSecondary,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74})&&
        executable(supported.play,6)&&base[supported.play]==0xba&&base[supported.play+5]==0xe9&&
        call(supported.advance+0x23,supported.playing)&&call(supported.advance+0x4c,supported.play)&&
        call(supported.advance+0x73,supported.playing);
}
using Logger=void(*)(const char*,bool);
using Advance=void(__cdecl*)(void*);
using Target=float(__cdecl*)(void*,int);
using Lookup=uintptr_t*(__cdecl*)(int,int);
inline Advance originalAdvance=nullptr;
inline Target originalTarget=nullptr;
inline Lookup lookup=nullptr;
inline bool (__cdecl* playing)(int,int*)=nullptr;
inline void (__cdecl* play)(int)=nullptr;
inline bool (__cdecl* custom)(void*,int*)=nullptr;
inline bool (__cdecl* customSecondary)(void*,int*)=nullptr;
inline void (__cdecl* volume)(int,int)=nullptr;
inline void* advanceAddress=nullptr;
inline void* targetAddress=nullptr;
inline bool owned=false;
inline Logger logger=nullptr;
inline int lastState=-1;
inline int& field(void* owner,size_t offset){return *reinterpret_cast<int*>(static_cast<unsigned char*>(owner)+offset);}
inline uintptr_t voice(unsigned slot){
    int group=(supported.soundIds.at(slot)-0x2000)/80;
    auto shared=lookup(group>=13?1:0,group>=13?group-13:group);
    return shared?shared[0]:0;
}
class WinEngine final:public bgm::Engine {
public:
    bool playing(unsigned slot)override{return bgmhost::playing(supported.soundIds.at(slot),nullptr);}
    double position(unsigned slot)override{
        auto object=voice(slot);if(!object)return -1;
        auto table=*reinterpret_cast<uintptr_t**>(object);
        return reinterpret_cast<double(__cdecl*)(void*)>(table[0x68/8])(reinterpret_cast<void*>(object));
    }
    void start(unsigned slot)override{play(supported.soundIds.at(slot));}
    void seek(unsigned slot,double ms)override{
        auto object=voice(slot);if(!object)return;
        auto table=*reinterpret_cast<uintptr_t**>(object);
        reinterpret_cast<void(__cdecl*)(void*,double)>(table[0x70/8])(reinterpret_cast<void*>(object),ms);
    }
    void volume(unsigned slot,float value)override{bgmhost::volume(supported.soundIds.at(slot),static_cast<int>(std::clamp(value,0.f,127.f)));}
    uint64_t now()override{return GetTickCount64();}
};
inline WinEngine audio;
inline std::unique_ptr<bgm::Playback> playback;
inline bgm::Context context(void* owner){
    bgm::Context c;c.owner=owner;c.state=field(owner,0);c.mask=field(owner,0x10);
    c.forced=*(static_cast<unsigned char*>(owner)+0x49)!=0;c.custom=custom(owner,nullptr)||(*(static_cast<unsigned char*>(owner)+0x48)&&customSecondary(owner,nullptr));
    for(unsigned i=0;i<4;++i)memcpy(&c.applied[i],static_cast<unsigned char*>(owner)+0x14+i*4,4);
    return c;
}
inline void __cdecl advanceHook(void* owner){
    auto c=context(owner);auto result=playback->advance(c);
    if(result.mask)field(owner,0x10)=*result.mask;
    if(result.synchronized&&logger)logger("phase synchronized on default song-select entry",false);
    if(!result.handled)originalAdvance(owner);
}
inline float __cdecl targetHook(void* owner,int layer){
    float original=originalTarget(owner,layer);auto c=context(owner);
    if(!c.custom&&!c.forced&&c.state!=lastState){
        lastState=c.state;
        if(logger)logger(("playback state="+std::to_string(c.state)+" mask="+std::to_string(c.mask)).c_str(),false);
    }
    return playback->target(c,static_cast<unsigned>(layer),original);
}
inline double readLoop(const std::wstring& file){
    FILE* f=nullptr;if(_wfopen_s(&f,file.c_str(),L"rb")||!f)return 0;
    unsigned char h[122]{};size_t length=fread(h,1,sizeof(h),f);fclose(f);
    if(length!=sizeof(h)||memcmp(h,"SD9\0",4)||memcmp(h+102,"fact",4))return 0;
    uint32_t frames=0,rate=0,start=0,end=0;
    memcpy(&frames,h+110,4);memcpy(&rate,h+56,4);memcpy(&start,h+20,4);memcpy(&end,h+24,4);
    if(rate!=44100||!frames||start||(end&&end!=frames*4))return 0;
    return frames*1000./rate;
}
inline void stop(){
    if(advanceAddress){MH_DisableHook(advanceAddress);MH_RemoveHook(advanceAddress);}
    if(targetAddress){MH_DisableHook(targetAddress);MH_RemoveHook(targetAddress);}
    if(owned)MH_Uninitialize();
    advanceAddress=targetAddress=nullptr;owned=false;playback.reset();lastState=-1;
}
inline bool install(HMODULE game,const bgm::Profile& profile,const std::vector<std::wstring>& loops,Logger log){
    logger=log;
    if(!bgm::supportsHost(profile,supported.family,supported.id))return false;
    if(!validLayout(game))return false;
    if(profile.strategy==bgm::Strategy::Original)return true;
    if(loops.size()!=3)return false;
    double duration=0;
    if(profile.strategy!=bgm::Strategy::Staged)
        for(auto& file:loops){double d=readLoop(file);if(d<=0||(duration&&std::abs(duration-d)>.001))return false;duration=d;}
    auto base=reinterpret_cast<unsigned char*>(game);
    const unsigned char a[]={0x40,0x53,0x48,0x83,0xEC,0x20},t[]={0x48,0x89,0x5C,0x24,0x08,0x57};
    if(memcmp(base+supported.advance,a,sizeof(a))||memcmp(base+supported.target,t,sizeof(t)))return false;
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    owned=status==MH_OK;
    lookup=reinterpret_cast<Lookup>(base+supported.lookup);
    playing=reinterpret_cast<decltype(playing)>(base+supported.playing);
    play=reinterpret_cast<decltype(play)>(base+supported.play);
    custom=reinterpret_cast<decltype(custom)>(base+supported.custom);
    customSecondary=reinterpret_cast<decltype(customSecondary)>(base+supported.customSecondary);
    volume=reinterpret_cast<decltype(volume)>(base+supported.volume);
    playback=std::make_unique<bgm::Playback>(profile,audio,duration);
    advanceAddress=base+supported.advance;targetAddress=base+supported.target;
    if(MH_CreateHook(advanceAddress,reinterpret_cast<void*>(advanceHook),reinterpret_cast<void**>(&originalAdvance))!=MH_OK||
       MH_CreateHook(targetAddress,reinterpret_cast<void*>(targetHook),reinterpret_cast<void**>(&originalTarget))!=MH_OK||
       MH_EnableHook(advanceAddress)!=MH_OK||MH_EnableHook(targetAddress)!=MH_OK){stop();return false;}
    logger(("controller ready: profile="+profile.id+" transition="+std::to_string(profile.fadeMs)+"ms").c_str(),false);
    return true;
}
} // namespace bgmhost
