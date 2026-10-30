#pragma once
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace bgm {
struct HostVersion {std::string version,source;};
inline std::string metadataText(const std::wstring& path){
    std::ifstream input(path,std::ios::binary);
    if(!input)return {};
    input.seekg(0,std::ios::end);auto size=input.tellg();
    if(size<0||size>2*1024*1024)throw std::runtime_error("host metadata is too large");
    input.seekg(0);return std::string(std::istreambuf_iterator<char>(input),{});
}
inline std::optional<std::string> metadataTag(std::string xml,const std::string& tag){
    size_t start=0;
    while((start=xml.find("<!--",start))!=std::string::npos){
        auto end=xml.find("-->",start+4);
        if(end==std::string::npos)throw std::runtime_error("invalid XML comment in host metadata");
        xml.erase(start,end+3-start);
    }
    const std::string opening="<"+tag,closing="</"+tag+">";
    std::optional<std::string> result;size_t pos=0;
    while((pos=xml.find(opening,pos))!=std::string::npos){
        size_t after=pos+opening.size();
        if(after==xml.size())throw std::runtime_error("invalid host metadata tag");
        if(xml[after]=='/')throw std::runtime_error("empty host metadata tag");
        if(xml[after]!='>'&&xml[after]!=' '&&xml[after]!='\t'&&xml[after]!='\r'&&xml[after]!='\n'){pos=after;continue;}
        auto begin=xml.find('>',after),end=xml.find(closing,after);
        if(begin==std::string::npos||end==std::string::npos||begin>=end||result)throw std::runtime_error("invalid or duplicate host version tag");
        auto value=xml.substr(begin+1,end-begin-1);
        auto first=value.find_first_not_of(" \r\n\t"),last=value.find_last_not_of(" \r\n\t");
        result=first==std::string::npos?"":value.substr(first,last-first+1);pos=end+closing.size();
    }
    return result;
}
inline bool validHostVersion(const std::string& version){
    return version.size()==10&&version.rfind("20",0)==0&&version.find_first_not_of("0123456789")==std::string::npos;
}
inline HostVersion resolveHostVersion(const std::string& bootstrap,const std::string& ea3,
                                     const std::string& runtimeModel={},const std::string& runtimeVersion={}){
    auto model=metadataTag(ea3,"model");
    if((model&&*model!="LDJ")||(!runtimeModel.empty()&&runtimeModel!="LDJ"))throw std::runtime_error("host model is not LDJ");
    HostVersion result;
    auto release=metadataTag(bootstrap,"release_code");
    if(release){result={*release,"prop/bootstrap.xml release_code"};}
    else if(!runtimeVersion.empty()){result={runtimeVersion,"Spice SDK AVS ext"};}
    else {auto ext=metadataTag(ea3,"ext");if(ext)result={*ext,"prop/ea3-config.xml ext"};}
    if(!validHostVersion(result.version))throw std::runtime_error("missing or invalid host version");
    return result;
}
} // namespace bgm
