#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <stdexcept>
struct Filter {
    double b0,b1,b2,a1,a2,z1=0,z2=0;
    double step(double x) { double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y; }
};
static Filter shelf(double fs) {
    double k=tan(3.141592653589793*1681.974450955533/fs),q=.7071752369554196;
    double vh=pow(10.,3.999843853973347/20),vb=pow(vh,.4996667741545416),a=1+k/q+k*k;
    return {(vh+vb*k/q+k*k)/a,2*(k*k-vh)/a,(vh-vb*k/q+k*k)/a,2*(k*k-1)/a,(1-k/q+k*k)/a};
}
static Filter highpass(double fs) {
    double k=tan(3.141592653589793*38.13547087602444/fs),q=.5003270373238773,a=1+k/q+k*k;
    return {1,-2,1,2*(k*k-1)/a,(1-k/q+k*k)/a};
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc<2)throw std::runtime_error("meter PCM16-stereo-file [linear-gain]");
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
        if(!f)throw std::runtime_error("input missing");
        auto size=static_cast<size_t>(f.tellg());f.seekg(0);
        std::vector<int16_t> data(size/2);f.read(reinterpret_cast<char*>(data.data()),size);
        double gain=argc>2?std::stod(argv[2]):1.;size_t frames=data.size()/2;
        if(frames<17640)throw std::runtime_error("need 400 ms");
        Filter pre[2]={shelf(44100),shelf(44100)},post[2]={highpass(44100),highpass(44100)};
        std::vector<double> prefix(frames+1);double sum=0,peak=0;
        for(size_t i=0;i<frames;++i) {
            double energy=0;
            for(int c=0;c<2;++c){double x=data[2*i+c]/32768.*gain;sum+=x*x;peak=std::max(peak,abs(x));
                double y=post[c].step(pre[c].step(x));energy+=y*y;}
            prefix[i+1]=prefix[i]+energy;
        }
        std::vector<double> blocks;double absoluteSum=0;size_t absoluteCount=0;
        for(size_t p=0;p+17640<=frames;p+=4410){double energy=(prefix[p+17640]-prefix[p])/17640;
            double db=-.691+10*log10(std::max(energy,1e-30));blocks.push_back(energy);
            if(db>-70){absoluteSum+=energy;++absoluteCount;}}
        if(!absoluteCount)throw std::runtime_error("signal below absolute gate");
        double threshold=std::max(-70.,-.691+10*log10(absoluteSum/absoluteCount)-10),gated=0;size_t count=0;
        for(double energy:blocks)if(-.691+10*log10(std::max(energy,1e-30))>threshold){gated+=energy;++count;}
        std::cout<<std::setprecision(12)<<"{\"integrated_lufs\":"<<(-.691+10*log10(gated/count))
                 <<",\"rms_dbfs\":"<<10*log10(sum/(frames*2))
                 <<",\"sample_peak_dbfs\":"<<20*log10(peak)<<",\"frames\":"<<frames
                 <<",\"gated_blocks\":"<<count<<",\"total_blocks\":"<<blocks.size()<<"}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
