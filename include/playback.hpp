#pragma once
#include "profile.hpp"
#include <cstdint>
#include <optional>

namespace bgm {
struct Context {
    void* owner=nullptr;
    int state=0, mask=0;
    bool custom=false, forced=false;
    std::array<float,4> applied{};
};
struct Engine {
    virtual ~Engine()=default;
    virtual bool playing(unsigned slot)=0;
    virtual double position(unsigned slot)=0;
    virtual void start(unsigned slot)=0;
    virtual void seek(unsigned slot,double milliseconds)=0;
    virtual void volume(unsigned slot,float value)=0;
    virtual uint64_t now()=0;
};
struct AdvanceResult { bool handled=false, synchronized=false;std::optional<int> mask; };
class Playback {
    Profile config;
    Engine& engine;
    double duration;
    void* fadingOwner=nullptr;
    bool crossfading=false;
    uint64_t crossfadeStart=0,stageStart=0;
    void* stageOwner=nullptr;
    int stage=-1;
    std::array<float,4> from{};
    bool valid(const Context& c)const{return !c.custom&&!c.forced&&c.state>=0&&c.state<4;}
    double fraction(uint64_t start)const{return std::min(1.,static_cast<double>(engine.now()-start)/config.fadeMs);}
public:
    Playback(const Profile& profile,Engine& audio,double loop):config(profile),engine(audio),duration(loop){}
    void reset(){crossfading=false;fadingOwner=stageOwner=nullptr;stage=-1;}
    AdvanceResult advance(const Context& c) {
        if(!valid(c)||config.strategy==Strategy::Original){reset();return {};}
        if(c.state!=2)return {};
        if(config.strategy==Strategy::Staged){
            if(c.mask==0){engine.volume(2,0);engine.start(2);engine.volume(2,0);}
            return {true,false,2};
        }
        if(c.mask==0) {
            int outgoing=engine.playing(0)?0:(engine.playing(1)?1:-1);
            if(outgoing<0||duration<=0)return {};
            double pos=engine.position(static_cast<unsigned>(outgoing));
            if(!std::isfinite(pos)||pos<0)return {};
            pos=std::fmod(pos,duration);
            if(config.strategy==Strategy::Layered) {
                // Muted stems normally already run. Only repair the incoming
                // stem when it stopped or drifted; leave audible layers alone.
                if(config.layerCount>2){
                    bool running=engine.playing(2);
                    double current=running?engine.position(2):-1;
                    double delta=duration;
                    if(std::isfinite(current)&&current>=0){
                        delta=std::abs(std::fmod(current,duration)-pos);
                        delta=std::min(delta,duration-delta);
                    }
                    if(!running){engine.volume(2,0);engine.start(2);engine.volume(2,0);}
                    if(!running||delta>2.)engine.seek(2,pos);
                }
                return {true,true,1};
            }
            engine.volume(2,0);engine.start(2);engine.volume(2,0);engine.seek(2,pos);
            fadingOwner=c.owner;crossfadeStart=engine.now();crossfading=true;
            return {true,true,1};
        }
        if(config.strategy==Strategy::Layered)return {true,false,1};
        if(crossfading&&fadingOwner==c.owner){
            if(fraction(crossfadeStart)>=1){crossfading=false;return {true,false,2};}
            return {true,false,{}};
        }
        return {c.mask==2,false,{}};
    }
    float target(const Context& c,unsigned layer,float original) {
        if(!valid(c)){reset();return original;}
        if(layer>=4||config.strategy==Strategy::Original)return original;
        if(config.strategy==Strategy::Crossfade) {
            if(c.state!=2){crossfading=false;return original;}
            if(!crossfading||fadingOwner!=c.owner)return original;
            float t=static_cast<float>(fraction(crossfadeStart));
            return layer<2?original*(1-t):(layer==2?original*t:original);
        }
        if(stageOwner!=c.owner||stage!=c.state){
            stageOwner=c.owner;stage=c.state;from=c.applied;stageStart=engine.now();
        }
        float t=static_cast<float>(fraction(stageStart));
        return from[layer]+(config.levels[c.state][layer]-from[layer])*t;
    }
};
} // namespace bgm
