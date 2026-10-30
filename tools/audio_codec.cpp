#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdint>
using Bytes = std::vector<unsigned char>;
static Bytes read(const wchar_t* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("open input");
    return Bytes(std::istreambuf_iterator<char>(f), {});
}
static void write(const wchar_t* path, const Bytes& b) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), b.size());
    if (!f) throw std::runtime_error("write output");
}
static uint32_t u32(const Bytes& b, size_t p) { uint32_t v; memcpy(&v,b.data()+p,4);return v; }
static void put(Bytes& b,size_t p,uint32_t v) { memcpy(b.data()+p,&v,4); }
static size_t chunk(const Bytes& b,const char* name) {
    for(size_t p=44;p+8<=b.size();) {
        auto len=u32(b,p+4);
        if(p+8+len>b.size()) throw std::runtime_error("bad chunk");
        if(!memcmp(b.data()+p,name,4))return p;
        p+=8+len+(len&1);
    }
    throw std::runtime_error("missing chunk");
}
static Bytes convert(WAVEFORMATEX* src,WAVEFORMATEX* dst,const Bytes& input) {
    HACMSTREAM stream=nullptr;
    auto result=acmStreamOpen(&stream,nullptr,src,dst,nullptr,0,0,ACM_STREAMOPENF_NONREALTIME);
    if(result)throw std::runtime_error("acmStreamOpen: "+std::to_string(result));
    DWORD size=0;
    result=acmStreamSize(stream,static_cast<DWORD>(input.size()),&size,ACM_STREAMSIZEF_SOURCE);
    if(result){acmStreamClose(stream,0);throw std::runtime_error("acmStreamSize");}
    Bytes out(size+dst->nBlockAlign*4);
    ACMSTREAMHEADER h{};h.cbStruct=sizeof(h);
    h.pbSrc=const_cast<BYTE*>(input.data());h.cbSrcLength=static_cast<DWORD>(input.size());
    h.pbDst=out.data();h.cbDstLength=static_cast<DWORD>(out.size());
    result=acmStreamPrepareHeader(stream,&h,0);
    if(result){acmStreamClose(stream,0);throw std::runtime_error("prepare");}
    result=acmStreamConvert(stream,&h,ACM_STREAMCONVERTF_START|ACM_STREAMCONVERTF_END|ACM_STREAMCONVERTF_BLOCKALIGN);
    DWORD used=h.cbDstLengthUsed,consumed=h.cbSrcLengthUsed;
    acmStreamUnprepareHeader(stream,&h,0);acmStreamClose(stream,0);
    if(result||consumed!=input.size())throw std::runtime_error("convert: "+std::to_string(result)+" consumed "+std::to_string(consumed)+" of "+std::to_string(input.size()));
    out.resize(used);return out;
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc<4)throw std::runtime_error("decode input.sd9 out.pcm | encode template.sd9 in.pcm out.sd9");
        auto b=read(argv[2]);auto fmt=chunk(b,"fmt "),dat=chunk(b,"data");
        auto compressed=reinterpret_cast<WAVEFORMATEX*>(b.data()+fmt+8);
        WAVEFORMATEX pcm{};pcm.wFormatTag=WAVE_FORMAT_PCM;pcm.nChannels=compressed->nChannels;
        pcm.nSamplesPerSec=compressed->nSamplesPerSec;pcm.wBitsPerSample=16;
        pcm.nBlockAlign=pcm.nChannels*2;pcm.nAvgBytesPerSec=pcm.nBlockAlign*pcm.nSamplesPerSec;
        if(!wcscmp(argv[1],L"decode")) {
            Bytes input(b.begin()+dat+8,b.begin()+dat+8+u32(b,dat+4));
            auto out=convert(compressed,&pcm,input);write(argv[3],out);
            std::cout<<out.size()/pcm.nBlockAlign<<" frames\n";
        } else if(!wcscmp(argv[1],L"encode")&&argc==5) {
            auto input=read(argv[3]);
            if(compressed->wFormatTag!=WAVE_FORMAT_ADPCM||compressed->cbSize<2)
                throw std::runtime_error("MS ADPCM template required");
            auto samples=reinterpret_cast<ADPCMWAVEFORMAT*>(compressed)->wSamplesPerBlock;
            size_t block=static_cast<size_t>(samples)*pcm.nBlockAlign;
            if(!block||input.size()%pcm.nBlockAlign)throw std::runtime_error("bad PCM frame alignment");
            // ACM consumes whole ADPCM blocks. Loop/fact metadata keeps padding inaudible.
            if(input.size()%block){
                size_t previous=input.size();input.resize(previous+block-previous%block);
                for(size_t i=previous;i<input.size();++i)input[i]=input[(i-previous)%previous];
            }
            auto out=convert(&pcm,compressed,input);
            Bytes result(b.begin(),b.begin()+dat+8);
            put(result,dat+4,static_cast<uint32_t>(out.size()));
            result.insert(result.end(),out.begin(),out.end());
            if(out.size()&1)result.push_back(0);
            result.insert(result.end(),b.begin()+dat+8+u32(b,dat+4)+(u32(b,dat+4)&1),b.end());
            put(result,36,static_cast<uint32_t>(result.size()-40));
            put(result,8,static_cast<uint32_t>(result.size()-32));
            write(argv[4],result);std::cout<<out.size()<<" ADPCM bytes\n";
        } else throw std::runtime_error("bad operation");
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
