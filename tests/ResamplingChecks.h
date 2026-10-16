// SPDX-License-Identifier: AGPL-3.0-only
#include "WaveTools.h"
#include <limits>
static void testResampling(){
    constexpr double pi=3.14159265358979323846;
    auto signal=[&](int count,double rate,double frequency){AudioBuffer<float> result(2,count);for(int i=0;i<count;++i){float s=(float)(.4*std::sin(2*pi*frequency*i/rate));result.setSample(0,i,s);result.setSample(1,i,-.5f*s);}return result;};
    auto peakError=[&](const AudioBuffer<float>& buffer,double rate,double frequency){double error=0;for(int i=256;i<buffer.getNumSamples()-256;++i)error=jmax(error,std::abs(buffer.getSample(0,i)-.4*std::sin(2*pi*frequency*i/rate)));return error;};
    auto stereo=[](const AudioBuffer<float>& buffer){for(int i=0;i<buffer.getNumSamples();++i)if(buffer.getSample(1,i)!=-.5f*buffer.getSample(0,i))return false;return true;};
    auto source=signal(44100,44100,997);AudioBuffer<float> output;
    expect(resampleWave(source,44100,48000,output).wasOk()&&output.getNumSamples()==48000&&peakError(output,48000,997)<.0001&&stereo(output),"44.1 to 48 kHz conversion preserves duration, stereo polarity and 997 Hz pitch");
    auto once=output;expect(resampleWave(source,44100,48000,output).wasOk()&&std::equal(output.getReadPointer(0),output.getReadPointer(0)+48000,once.getReadPointer(0)),"offline resampling is deterministic");
    expect(resampleWave(once,48000,44100,output).wasOk()&&output.getNumSamples()==44100&&peakError(output,44100,997)<.00015,"48 to 44.1 kHz conversion round-trips a passband tone");
    for(double rate:{8000.,16000.,22050.,32000.,44100.,48000.,88200.,96000.,192000.}){
        auto original=signal((int)rate/10,rate,440);expect(resampleWave(original,rate,48000,output).wasOk()&&output.getNumSamples()==4800&&peakError(output,48000,440)<.0001&&stereo(output),"every editor sample-rate preset converts to 48 kHz at correct pitch");
    }
    auto high=signal(48000,48000,14000);expect(resampleWave(high,48000,16000,output).wasOk()&&output.getMagnitude(0,256,output.getNumSamples()-512)<.0001f,"downsampling suppresses a 14 kHz tone below -80 dBFS instead of aliasing to 2 kHz");
    auto low=signal(48000,48000,2000);expect(resampleWave(low,48000,8000,output).wasOk()&&peakError(output,8000,2000)<.0001,"6:1 downsampling keeps a 2 kHz passband tone");
    auto extreme=signal(19200,192000,300);expect(resampleWave(extreme,192000,8000,output).wasOk()&&output.getNumSamples()==800&&peakError(output,8000,300)<.0001,"maximum 24:1 downsampling remains band limited and frame accurate");
    AudioBuffer<float> dc(2,123);for(int i=0;i<123;++i){dc.setSample(0,i,.2f);dc.setSample(1,i,-.7f);}expect(resampleWave(dc,8000,192000,output).wasOk()&&output.getNumSamples()==2952&&std::abs(output.getSample(0,0)-.2)<1.e-7&&std::abs(output.getSample(1,2951)+.7)<1.e-7,"endpoint extension preserves independent stereo DC including first and last output frames");
    AudioBuffer<float> single(2,1);single.setSample(0,0,.6f);single.setSample(1,0,-.2f);expect(resampleWave(single,192000,8000,output).wasOk()&&output.getNumSamples()==1&&output.getSample(0,0)==.6f,"sub-frame conversions retain one bounded frame");
    auto fractional=signal(1001,44100,500);expect(resampleWave(fractional,44100,48000,output).wasOk()&&output.getNumSamples()==1090,"fractional output lengths round to the nearest frame");
    expect(resampleWave(source,44100,44100,output).wasOk()&&output.getNumSamples()==44100&&std::equal(source.getReadPointer(0),source.getReadPointer(0)+44100,output.getReadPointer(0)),"same-rate conversion is sample exact");
    auto alias=source;expect(resampleWave(alias,44100,48000,alias).wasOk()&&alias.getNumSamples()==48000&&peakError(alias,48000,997)<.0001,"in-place output aliasing does not corrupt resampling input");
    auto sentinel=single;output=sentinel;auto unchanged=[&]{return output.getNumSamples()==1&&output.getSample(0,0)==.6f&&output.getSample(1,0)==-.2f;};
    expect(resampleWave(source,44100,7999,output).failed()&&unchanged(),"invalid target rate leaves output intact");
    expect(resampleWave(source,44100,std::numeric_limits<double>::quiet_NaN(),output).failed()&&unchanged(),"nonfinite target rate is rejected atomically");
    expect(resampleWave(source,0,48000,output).failed()&&unchanged(),"invalid source rate is rejected atomically");
    expect(resampleWave(source,44100,48000,output,[]{return true;}).failed()&&unchanged(),"cancelled conversion never commits output");
    int calls=0;expect(resampleWave(source,44100,48000,output,[&]{return ++calls>520;}).failed()&&calls>520&&unchanged(),"cancellation during output generation retains original output");
    auto bad=source;bad.setSample(1,20,std::numeric_limits<float>::infinity());expect(resampleWave(bad,44100,48000,output).failed()&&unchanged(),"nonfinite source samples are rejected before conversion");
    AudioBuffer<float> mono(1,10);mono.clear();expect(resampleWave(mono,44100,48000,output).failed()&&unchanged(),"unsupported audio channel format fails atomically");
    AudioBuffer<float> base(2,4800);base.clear();int a=-1,b=-1;
    expect(spliceWave(base,48000,100,200,WaveOperation::insert,source,44100,output,a,b).wasOk()&&output.getNumSamples()==52800&&a==100&&b==48100&&peakError(once,48000,997)<.0001,"cross-rate insert selects the converted frames and retains destination length");
    bool exact=true;for(int ch=0;ch<2;++ch)exact&=std::equal(once.getReadPointer(ch),once.getReadPointer(ch)+48000,output.getReadPointer(ch,100));expect(exact&&output.getSample(0,99)==0&&output.getSample(0,48100)==0,"cross-rate insert exactly matches standalone conversion and preserves splice boundaries");
    expect(spliceWave(base,48000,100,200,WaveOperation::replace,source,44100,output,a,b).wasOk()&&output.getNumSamples()==52700&&a==100&&b==48100,"cross-rate replacement removes only the original selected frames");
    output=sentinel;a=7;b=8;expect(spliceWave(base,48000,100,200,WaveOperation::insert,source,44100,output,a,b,[]{return true;}).failed()&&unchanged()&&a==7&&b==8,"cancelled cross-rate splice retains output and selection atomically");
    AudioBuffer<float> full(2,960000);full.clear();expect(spliceWave(full,8000,0,1,WaveOperation::insert,source,44100,output,a,b).failed()&&unchanged()&&a==7&&b==8,"oversized cross-rate paste rejects before conversion");
    expect(resampleWave(full,8000,8000,output).wasOk()&&output.getNumSamples()==960000,"exactly 120 seconds remain valid at the duration boundary");
}
