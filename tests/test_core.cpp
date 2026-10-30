#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include "profile.hpp"
#include "playback.hpp"
#include "../vendor/minhook/include/MinHook.h"
#include "../hosts/adapter.hpp"
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Audio final:bgm::Engine {
    uint64_t time=1000;std::array<bool,4> running{true,true,false,false};
    std::array<double,4> positions{16250.75,16250.75,0,0};int starts=0,seeks=0,mutes=0;
    bool playing(unsigned i)override{return running[i];}
    double position(unsigned i)override{return positions[i];}
    void start(unsigned i)override{running[i]=true;++starts;}
    void seek(unsigned i,double p)override{positions[i]=p;++seeks;}
    void volume(unsigned,float v)override{if(v==0)++mutes;}
    uint64_t now()override{return time;}
};
static int __cdecl detour(){return 2;}
static void hostAdapterTest(const bgm::Profile& profile){
    unsigned char header[122]{};memcpy(header,"SD9\0",4);memcpy(header+102,"fact",4);
    uint32_t rate=44100,frames=44100;memcpy(header+56,&rate,4);memcpy(header+110,&frames,4);
    {std::ofstream file("build/test-loop.sd9",std::ios::binary);file.write(reinterpret_cast<char*>(header),sizeof(header));}
    const unsigned char advance[]={0x40,0x53,0x48,0x83,0xec,0x20};
    const unsigned char target[]={0x48,0x89,0x5c,0x24,0x08,0x57};
    auto image=static_cast<unsigned char*>(VirtualAlloc(nullptr,0xB00000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    require(image!=nullptr,"host fixture allocation");
    for(const auto& spec:bgmhost::builds){
        require(bgmhost::select(spec.sha256),"known host hash selected");
        require(!bgmhost::select(std::string(64,'0')),"unknown host hash rejected");
        memset(image+spec.advance,0x90,32);memset(image+spec.target,0x90,32);
        memcpy(image+spec.advance,advance,sizeof(advance));memcpy(image+spec.target,target,sizeof(target));
        auto logger=+[](const char*,bool){};
        require(bgmhost::install(reinterpret_cast<HMODULE>(image),profile,{L"build/test-loop.sd9",L"build/test-loop.sd9",L"build/test-loop.sd9"},logger),"host adapter accepts real compiler prologues");
        bgmhost::stop();
        require(memcmp(image+spec.advance,advance,sizeof(advance))==0&&memcmp(image+spec.target,target,sizeof(target))==0,"adapter restores both hooks");
        image[spec.advance]=0x90;
        require(!bgmhost::install(reinterpret_cast<HMODULE>(image),profile,{L"build/test-loop.sd9",L"build/test-loop.sd9",L"build/test-loop.sd9"},logger),"changed host prologue rejected");
        bgmhost::stop();
    }
    VirtualFree(image,0,MEM_RELEASE);
}
static void detourTest(){
    auto page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    require(page!=nullptr,"allocation");const unsigned char original[]={0xB8,1,0,0,0,0xC3};memcpy(page,original,sizeof(original));
    using Function=int(__cdecl*)();Function trampoline=nullptr;
    require(MH_Initialize()==MH_OK,"MinHook init");
    require(MH_CreateHook(page,reinterpret_cast<void*>(detour),reinterpret_cast<void**>(&trampoline))==MH_OK,"MinHook create");
    require(MH_EnableHook(page)==MH_OK,"MinHook enable");
    require(reinterpret_cast<Function>(page)()==2&&trampoline()==1,"detour and trampoline");
    MH_DisableHook(page);MH_RemoveHook(page);MH_Uninitialize();
    require(reinterpret_cast<Function>(page)()==1&&!memcmp(page,original,sizeof(original)),"hook restore");
    VirtualFree(page,0,MEM_RELEASE);
}
int main(){try{
    auto native=bgm::loadProfile(L"profiles\\sinobuz24.ini");
    auto compat=bgm::loadProfile(L"profiles\\sinobuz24_compat.ini");
    require(native.strategy==bgm::Strategy::Layered&&compat.strategy==bgm::Strategy::Crossfade,"strategies");
    require(native.resources.size()==31&&compat.resources.size()==31,"profile routes");
    require(bgm::expand(native.root,"wed").find("{day}")==std::string::npos,"variant expansion");
    Audio audio;bgm::Playback player(native,audio,12000);
    bgm::Context c;c.owner=&audio;c.state=0;
    require(player.target(c,0,10)==0,"initial fade");audio.time+=native.fadeMs;
    require(player.target(c,0,10)==127&&player.target(c,1,10)==0,"entry stage");
    c.state=1;c.applied={127,0,0,0};player.target(c,0,10);audio.time+=native.fadeMs;
    require(player.target(c,0,10)==110&&player.target(c,1,10)==110,"mode stage");
    c.state=2;c.mask=0;c.applied={110,110,0,0};auto result=player.advance(c);
    require(result.handled&&result.mask==1&&audio.starts==1&&audio.seeks==1&&std::abs(audio.positions[2]-4250.75)<1e-8,"incoming phase sync");
    c.mask=1;player.target(c,0,10);audio.time+=native.fadeMs;
    require(player.target(c,0,10)==96&&player.target(c,1,10)==96&&player.target(c,2,10)==96&&player.target(c,3,10)==0,"layered selection no full-mix duplication");
    player.advance(c);require(audio.starts==1&&audio.seeks==1,"muted stems not restarted each tick");
    c.state=3;c.applied={96,96,96,0};player.target(c,0,10);audio.time+=native.fadeMs;
    require(player.target(c,2,10)==96&&player.target(c,3,10)==0,"host solo state mapped to three stems");
    c.custom=true;require(player.target(c,0,33)==33&&!player.advance(c).handled,"custom BGM untouched");
    c.custom=false;c.forced=true;require(player.target(c,0,22)==22,"forced host mode untouched");
    Audio crossAudio;bgm::Playback cross(compat,crossAudio,12000);c={};c.owner=&crossAudio;c.state=2;
    result=cross.advance(c);require(result.mask==1&&crossAudio.positions[2]==4250.75,"fullmix phase sync");
    c.mask=1;crossAudio.time+=compat.fadeMs/2;
    require(cross.target(c,0,54)==27&&cross.target(c,2,62)==31,"fullmix midpoint");
    crossAudio.time+=compat.fadeMs/2;result=cross.advance(c);require(result.mask==2,"fullmix steady only");
    auto fixed=std::string("[profile]\nschema=1\nid=other_version\nhost=iidx34-20260916\nresource_root=/data/sound/system/other/{day}\nschedule=fixed\nfixed_variant=default\n[playback]\nstrategy=original\n[map]\n0/test.sd9=0/test.sd9\n");
    std::istringstream stream(fixed);auto alternate=bgm::parseProfile(stream);
    require(alternate.schedule=="fixed"&&alternate.fixedVariant=="default","non-daily profile");
    for(const auto& invalid:std::vector<std::pair<std::string,std::string>>{{"schema=1","schema=9"},{"id=other_version","id=../bad"},{"strategy=original","strategy=unknown"},{"0/test.sd9=0/test.sd9","0/test.sd9=../../escape.sd9"},{"schedule=fixed","schedule=unknown"}}){
        auto bad=fixed;auto pos=bad.find(invalid.first);bad.replace(pos,invalid.first.size(),invalid.second);bool rejected=false;
        try{std::istringstream s(bad);bgm::parseProfile(s);}catch(const std::exception&){rejected=true;}
        require(rejected,"invalid profile must fail closed");
    }
    require(!bgm::safeId("../x")&&!bgm::safePath("C:/outside")&&!bgm::safePath("../x"),"path validation");
    require(bgm::supportsHost(native,"iidx33","iidx33-20260819")&&bgm::supportsHost(native,"iidx34","iidx34-20260916"),"shared profile host families");
    require(!bgm::supportsHost(native,"iidx32","iidx32-unknown"),"unlisted hosts rejected");
    auto sparkle=bgm::loadProfile(L"profiles/sparkle33.ini");
    require(sparkle.strategy==bgm::Strategy::Original&&sparkle.schedule=="fixed","Sparkle keeps native intro-loop scheduling");
    auto resort=bgm::loadProfile(L"profiles/resort18.ini");
    Audio stageAudio;bgm::Playback staged(resort,stageAudio,0);c={};c.owner=&stageAudio;
    staged.target(c,0,0);stageAudio.time+=resort.fadeMs;
    require(staged.target(c,0,0)==62,"independent entry track");
    c.state=1;c.applied={62,0,0,0};staged.target(c,0,0);stageAudio.time+=resort.fadeMs;
    require(staged.target(c,0,0)==0&&staged.target(c,1,0)==62,"mode track replaces entry track");
    c.state=2;c.applied={0,62,0,0};result=staged.advance(c);
    require(result.handled&&result.mask==2&&stageAudio.starts==1&&stageAudio.seeks==0,"legacy selection starts independently without phase seek");
    c.mask=2;staged.target(c,0,0);stageAudio.time+=resort.fadeMs;staged.advance(c);
    require(staged.target(c,1,0)==0&&staged.target(c,2,0)==62&&stageAudio.starts==1,"selection steady without restart or menu overlap");
    c.custom=true;require(staged.target(c,2,27)==27&&!staged.advance(c).handled,"legacy custom BGM bypass");
    detourTest();
    hostAdapterTest(native);
    std::cout<<"PASS: profiles/schema/path validation, fixed and weekday layouts, layered stages, muted-stem sync/no restart, host-state adaptation, fullmix fallback, custom/forced bypass and real detours\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
