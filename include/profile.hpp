#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace bgm {
enum class Strategy { Original, Crossfade, Layered };
struct Resource { std::string source, target; };
struct Profile {
    std::string id, host, root, fallback, schedule = "weekday", fixedVariant = "default";
    Strategy strategy = Strategy::Original;
    unsigned fadeMs = 600, layerCount = 3;
    std::array<std::array<float,4>,4> levels{};
    std::vector<Resource> resources;
};
inline std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?"":s.substr(a,b-a+1);
}
inline bool safeId(const std::string& s) {
    return !s.empty() && s.size()<=64 && s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;
}
inline bool safePath(const std::string& s) {
    return !s.empty() && s.find("..") == std::string::npos && s.find(':') == std::string::npos &&
        s.find('\\') == std::string::npos && s.find('\0') == std::string::npos;
}
inline std::string expand(std::string s,const std::string& variant) {
    size_t pos=0;
    while((pos=s.find("{day}",pos))!=std::string::npos){s.replace(pos,5,variant);pos+=variant.size();}
    if(s.find_first_of("{}")!=std::string::npos)throw std::runtime_error("unknown path placeholder");
    return s;
}
inline unsigned number(const std::string& s,unsigned minimum,unsigned maximum) {
    if(s.empty() || s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("invalid number");
    auto n=std::stoul(s);if(n<minimum || n>maximum)throw std::runtime_error("number outside supported range");
    return static_cast<unsigned>(n);
}
inline Profile parseProfile(std::istream& input) {
    std::map<std::string,std::map<std::string,std::string>> ini;
    std::string section,line;
    while(std::getline(input,line)) {
        if(line.compare(0,3,"\xEF\xBB\xBF")==0)line.erase(0,3);
        line=trim(line);if(line.empty() || line[0]==';' || line[0]=='#')continue;
        if(line.front()=='[' && line.back()==']'){section=line.substr(1,line.size()-2);continue;}
        auto equals=line.find('=');
        if(section.empty() || equals==std::string::npos)throw std::runtime_error("invalid INI entry");
        auto key=trim(line.substr(0,equals)),value=trim(line.substr(equals+1));
        if(key.empty() || !ini[section].emplace(key,value).second)throw std::runtime_error("duplicate or empty profile key");
    }
    auto required=[&](const char* sec,const char* key)->std::string {
        auto i=ini[sec].find(key);if(i==ini[sec].end() || i->second.empty())throw std::runtime_error(std::string("missing ")+sec+"."+key);
        return i->second;
    };
    if(required("profile","schema")!="1")throw std::runtime_error("unsupported profile schema");
    Profile p;p.id=required("profile","id");p.host=required("profile","host");
    if(!safeId(p.id)||!safeId(p.host))throw std::runtime_error("unsafe profile/host id");
    p.root=required("profile","resource_root");
    if(!safePath(p.root)||p.root.rfind("/data/sound/system/",0)!=0)throw std::runtime_error("unsafe resource root");
    if(ini["profile"].count("fallback")){p.fallback=ini["profile"]["fallback"];if(!safeId(p.fallback))throw std::runtime_error("invalid fallback id");}
    if(ini["profile"].count("schedule"))p.schedule=ini["profile"]["schedule"];
    if(p.schedule!="weekday"&&p.schedule!="fixed")throw std::runtime_error("unknown schedule");
    if(p.schedule=="fixed"){p.fixedVariant=required("profile","fixed_variant");if(!safeId(p.fixedVariant))throw std::runtime_error("unsafe fixed variant");}
    auto mode=required("playback","strategy");
    if(mode=="layered_sync")p.strategy=Strategy::Layered;
    else if(mode=="crossfade_fullmix")p.strategy=Strategy::Crossfade;
    else if(mode=="original")p.strategy=Strategy::Original;
    else throw std::runtime_error("unsupported playback strategy");
    if(ini["playback"].count("transition_ms"))p.fadeMs=number(ini["playback"]["transition_ms"],100,2000);
    if(ini["playback"].count("layer_count"))p.layerCount=number(ini["playback"]["layer_count"],1,3);
    if(p.strategy==Strategy::Layered) {
        for(unsigned state=0;state<4;++state){
            auto key="state"+std::to_string(state);std::istringstream values(required("levels",key.c_str()));
            std::string token;unsigned i=0;
            while(std::getline(values,token,',')){
                if(i>=4)throw std::runtime_error("too many layer levels");
                size_t used=0;token=trim(token);float v=std::stof(token,&used);
                if(used!=token.size()||!std::isfinite(v)||v<0||v>127)throw std::runtime_error("invalid layer level");
                if(i>=p.layerCount&&v!=0)throw std::runtime_error("inactive layer must have zero level");
                p.levels[state][i++]=v;
            }
            if(i!=4)throw std::runtime_error("four layer levels required");
        }
    }
    for(auto& [source,target]:ini["map"]) {
        if(!safePath(source)||source.front()=='/'||source.find("{day}")!=std::string::npos ||!safePath(target))throw std::runtime_error("unsafe mapping");
        auto full=target.front()=='/'?target:p.root+"/"+target;
        if(full.rfind("/data/sound/system/",0)!=0)throw std::runtime_error("mapping outside system sounds");
        p.resources.push_back({"/data/sound/system/"+source,full});
    }
    if(p.resources.empty()||p.resources.size()>128)throw std::runtime_error("profile must contain 1-128 resources");
    return p;
}
inline Profile loadProfile(const std::wstring& path) {
    std::ifstream input(path);if(!input)throw std::runtime_error("profile file not found");
    return parseProfile(input);
}
} // namespace bgm
