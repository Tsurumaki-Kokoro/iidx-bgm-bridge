// Optional private integration test. Maps a user-supplied game DLL without
// resolving imports or running its entry point; never calls game functions.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include "../hosts/adapter.hpp"
static void log(const char* text,bool){std::cout<<text<<'\n';}
int wmain(int argc,wchar_t** argv){
    if(argc!=6){std::cerr<<"family game.dll l0.sd9 l1.sd9 l2.sd9\n";return 2;}
    bool found=false;
    for(const auto& spec:bgmhost::builds){
        std::wstring family(spec.family,spec.family+strlen(spec.family));
        if(family==argv[1]){bgmhost::supported=spec;found=true;break;}
    }
    if(!found)return 2;
    auto game=LoadLibraryExW(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!game){std::cerr<<"Cannot map game DLL: "<<GetLastError()<<'\n';return 1;}
    auto profile=bgm::loadProfile(L"profiles/sinobuz24.ini");
    bool installed=bgmhost::install(game,profile,{argv[3],argv[4],argv[5]},log);
    bgmhost::stop();
    auto start=reinterpret_cast<const unsigned char*>(game)+bgmhost::supported.advance;
    const unsigned char expected[]={0x40,0x53,0x48,0x83,0xec,0x20};
    bool restored=memcmp(start,expected,sizeof(expected))==0;
    FreeLibrary(game);
    if(!installed||!restored){std::cerr<<"FAIL: game-image hook install/restore\n";return 1;}
    std::cout<<"PASS: "<<bgmhost::supported.id<<" game-image hook install/restore; game entry point not executed\n";
    return 0;
}
