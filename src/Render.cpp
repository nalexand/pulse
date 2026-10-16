// SPDX-License-Identifier: AGPL-3.0-only
#include "Render.h"
#include <cmath>
#include <limits>

namespace pulse {
using namespace juce;
Result planRender(const Project& p,const RenderSettings& s,RenderPlan& plan){
    if(p.tracks.empty()||p.patterns.empty()||p.activePattern<0||p.activePattern>=(int)p.patterns.size()||!std::isfinite(p.bpm)||p.bpm<30||p.bpm>300)return Result::fail("Invalid project timing for export.");
    if((s.format!=RenderFormat::wav&&s.format!=RenderFormat::flac)||(s.source!=RenderSource::current&&s.source!=RenderSource::song&&s.source!=RenderSource::pattern))return Result::fail("Unsupported export format or source.");
    if((s.sampleRate!=44100&&s.sampleRate!=48000&&s.sampleRate!=96000)||(s.bits!=16&&s.bits!=24&&s.bits!=32)||(s.channels!=1&&s.channels!=2)||(s.format==RenderFormat::flac&&s.bits==32))return Result::fail("Choose WAV 16/24/32-float or FLAC 16/24, mono/stereo, at 44.1/48/96 kHz.");
    if(!std::isfinite(s.startBeat)||!std::isfinite(s.endBeat)||!std::isfinite(s.tailSeconds)||!std::isfinite(s.targetDb)||s.tailSeconds<0||s.tailSeconds>10||s.targetDb< -24||s.targetDb>0||s.startBeat<0||(s.endBeat<0&&s.endBeat!=-1))return Result::fail("Invalid export range, tail or normalization level.");
    RenderPlan next;next.songMode=s.source==RenderSource::current?p.songMode:s.source==RenderSource::song;
    double length=next.songMode?p.songLength():p.patterns[(size_t)p.activePattern].length;next.endBeat=s.endBeat<0?length:s.endBeat;
    if(!std::isfinite(length)||length>8192||next.endBeat>length||next.endBeat<=s.startBeat)return Result::fail("Export range must be increasing and inside the selected Song or Pattern.");
    double framesPerBeat=s.sampleRate*60/p.bpm;
    next.firstFrame=(int64)std::ceil(s.startBeat*framesPerBeat-1e-7);next.endFrame=(int64)std::ceil(next.endBeat*framesPerBeat-1e-7);next.tailFrames=(int64)std::llround(s.tailSeconds*s.sampleRate);next.outputFrames=next.endFrame-next.firstFrame+next.tailFrames;
    if(next.endFrame<=next.firstFrame||next.outputFrames<=0||next.outputFrames>(int64)s.sampleRate*3600)return Result::fail("Export length must be nonempty and at most one hour including tails.");
    plan=next;return Result::ok();
}
namespace {
// Keep final encoder/stream failures visible even when an encoder finalises in
// its destructor (FLAC). The old destination is replaced only after that point.
class CheckedRenderStream final:public OutputStream {
public:
    CheckedRenderStream(const File& file,bool& error):stream(file.createOutputStream()),failed(error){if(!stream)failed=true;}
    ~CheckedRenderStream()override{flush();}
    bool valid()const{return stream&&stream->openedOk();}
    bool write(const void* bytes,size_t count)override{bool ok=stream&&stream->write(bytes,count);failed|=!ok;return ok;}
    int64 getPosition()override{return stream?stream->getPosition():0;}
    bool setPosition(int64 pos)override{bool ok=stream&&stream->setPosition(pos);failed|=!ok;return ok;}
    void flush()override{if(stream){stream->flush();failed|=stream->getStatus().failed();}}
private:
    std::unique_ptr<FileOutputStream> stream;bool& failed;
};
class Quantizer {
public:
    int encode(float sample,int bits,double gain,bool dither){
        double scale=std::ldexp(1.,bits-1),value=std::isfinite(sample)?sample*gain:0;
        value=jlimit(-1.,1.,value)*scale;if(dither)value+=random()-random();
        auto q=(int64)jlimit(-scale,scale-1,std::floor(value+.5));return(int)(q*((int64)1<<(32-bits)));
    }
private:
    uint32_t state=0x50554c53;
    double random(){state^=state<<13;state^=state>>17;state^=state<<5;return(double)(state>>8)/16777216.;}
};
}
Result renderAudio(const Project& project,Assets& assets,const File& base,const File& target,const RenderSettings& settings,int onlyTrack,std::atomic<bool>* cancel,std::atomic<float>* progress){
    if(progress)progress->store(0);auto cancelled=[&]{return cancel&&cancel->load();};if(cancelled())return Result::fail("Export cancelled.");
    RenderPlan plan;auto result=planRender(project,settings,plan);if(result.failed())return result;
    if(onlyTrack< -1||onlyTrack>=(int)project.tracks.size())return Result::fail("Invalid isolated export track.");
    auto renderer=std::make_unique<Engine>();auto p=project;p.songMode=plan.songMode;p.loop=false;p.metronome=false;
    if(onlyTrack>=0)for(int t=0;t<(int)p.tracks.size();++t){if(t==onlyTrack)p.tracks[(size_t)t].mute=false;p.tracks[(size_t)t].solo=t==onlyTrack;}
    String warnings;auto& state=renderer->exchange.beginWrite();result=compilePlayback(p,assets,base,state,warnings);if(result.failed())return result;
    if(warnings.isNotEmpty())return Result::fail("Export requires missing samples to be resolved:\n"+warnings);
    state.length=plan.endBeat;while(state.eventCount>0&&state.events[(size_t)state.eventCount-1].beat>=plan.endBeat)--state.eventCount;
    for(int i=0;i<state.eventCount;++i)state.events[(size_t)i].length=jmin(state.events[(size_t)i].length,plan.endBeat-state.events[(size_t)i].beat);
    renderer->preserveAutomationInTails=true;renderer->exchange.publish();AudioBuffer<float> buffer(2,512);double peak=0,gain=1;
    auto pass=[&](bool analyse,AudioFormatWriter* writer)->Result{
        renderer->prepare(settings.sampleRate);renderer->playing.store(true);Quantizer quantizer;std::array<std::array<int,512>,2> encoded;const int* channels[]={encoded[0].data(),encoded[1].data(),nullptr};
        int64 total=plan.endFrame+plan.tailFrames;
        for(int64 pos=0;pos<total;){
            if(cancelled())return Result::fail("Export cancelled.");int count=(int)jmin((int64)512,total-pos);
            if(pos<plan.firstFrame)count=(int)jmin((int64)count,plan.firstFrame-pos);if(pos<plan.endFrame)count=(int)jmin((int64)count,plan.endFrame-pos);else renderer->playing.store(false);
            renderer->render(buffer.getWritePointer(0),buffer.getWritePointer(1),count);
            if(pos>=plan.firstFrame){
                if(settings.channels==1)for(int i=0;i<count;++i)buffer.setSample(0,i,(buffer.getSample(0,i)+buffer.getSample(1,i))*.5f);
                for(int ch=0;ch<settings.channels;++ch)for(int i=0;i<count;++i)if(!std::isfinite(buffer.getSample(ch,i)))return Result::fail("Export stopped after non-finite audio. Check the source samples.");
                if(analyse){for(int ch=0;ch<settings.channels;++ch)peak=jmax(peak,(double)buffer.getMagnitude(ch,0,count));}
                else if(settings.bits==32){buffer.applyGain((float)gain);if(!writer->writeFromAudioSampleBuffer(buffer,0,count))return Result::fail("Export write failed. Check disk space.");}
                else{for(int ch=0;ch<settings.channels;++ch)for(int i=0;i<count;++i)encoded[(size_t)ch][(size_t)i]=quantizer.encode(buffer.getSample(ch,i),settings.bits,gain,settings.dither);if(!writer->write(channels,count))return Result::fail("Export write failed. Check disk space.");}
            }
            pos+=count;if(progress){float fraction=(float)((double)pos/total);progress->store(settings.normalize?(analyse?.45f*fraction:.45f+.54f*fraction):.99f*fraction);}
        }return Result::ok();
    };
    if(settings.normalize){result=pass(true,nullptr);if(result.failed())return result;if(peak>1e-12)gain=std::pow(10.,settings.targetDb/20)/peak;}
    if(cancelled())return Result::fail("Export cancelled.");TemporaryFile temporary(target);bool failed=false;auto checked=std::make_unique<CheckedRenderStream>(temporary.getFile(),failed);if(!checked->valid())return Result::fail("Cannot create export file.");std::unique_ptr<OutputStream> output(std::move(checked));
    auto options=AudioFormatWriterOptions{}.withSampleRate(settings.sampleRate).withNumChannels(settings.channels).withBitsPerSample(settings.bits).withSampleFormat(settings.bits==32?AudioFormatWriterOptions::SampleFormat::floatingPoint:AudioFormatWriterOptions::SampleFormat::integral);
    std::unique_ptr<AudioFormatWriter> writer;if(settings.format==RenderFormat::flac)writer=FlacAudioFormat().createWriterFor(output,options.withQualityOptionIndex(5));else writer=WavAudioFormat().createWriterFor(output,options);
    if(!writer)return Result::fail("Cannot initialize export encoder.");result=pass(false,writer.get());writer.reset();
    if(result.failed())return result;if(failed)return Result::fail("Export could not be finalized. Check disk space.");if(cancelled())return Result::fail("Export cancelled.");
    if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Could not commit exported audio.");if(progress)progress->store(1);return Result::ok();
}
Result renderWav(const Project& project,Assets& assets,const File& base,const File& target,int bits,int onlyTrack,std::atomic<bool>* cancel,std::atomic<float>* progress){RenderSettings settings;settings.bits=bits;return renderAudio(project,assets,base,target,settings,onlyTrack,cancel,progress);}
}
