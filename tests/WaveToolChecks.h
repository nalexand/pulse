// SPDX-License-Identifier: AGPL-3.0-only
#include "WaveTools.h"
#include <limits>
static void testWaveTools(const File& root,Assets& assets){
    AudioBuffer<float> source(2,10),clipboard(2,3),out(2,1);for(int ch=0;ch<2;++ch){for(int i=0;i<10;++i)source.setSample(ch,i,(ch?-.01f:.01f)*(i+1));for(int i=0;i<3;++i)clipboard.setSample(ch,i,(ch?-.2f:.2f)*(i+1));}out.clear();int a=-1,b=-1;
    auto splice=[&](WaveOperation op,int first,int end){return spliceWave(source,8000,first,end,op,clipboard,8000,out,a,b);};
    auto equals=[&](const std::vector<int>& indexes){if(out.getNumSamples()!=(int)indexes.size())return false;for(int ch=0;ch<2;++ch)for(int i=0;i<(int)indexes.size();++i){int at=indexes[(size_t)i];float reference=at>=0?source.getSample(ch,at):clipboard.getSample(ch,-at-1);if(out.getSample(ch,i)!=reference)return false;}return true;};
    expect(splice(WaveOperation::trim,2,6).wasOk()&&equals({2,3,4,5})&&a==0&&b==4,"trim retains exact stereo frame range");
    expect(splice(WaveOperation::erase,2,6).wasOk()&&equals({0,1,6,7,8,9})&&a==2&&b==3,"delete joins exact prefix and suffix");
    expect(splice(WaveOperation::erase,0,3).wasOk()&&equals({3,4,5,6,7,8,9}),"delete at file start keeps suffix");
    expect(splice(WaveOperation::erase,7,10).wasOk()&&equals({0,1,2,3,4,5,6})&&a==6&&b==7,"delete at file end leaves bounded selection");
    expect(splice(WaveOperation::insert,2,6).wasOk()&&equals({0,1,-1,-2,-3,2,3,4,5,6,7,8,9})&&a==2&&b==5,"paste inserts without replacing selected source");
    expect(splice(WaveOperation::replace,2,6).wasOk()&&equals({0,1,-1,-2,-3,6,7,8,9}),"paste replace removes only selection");
    expect(splice(WaveOperation::replace,0,10).wasOk()&&equals({-1,-2,-3})&&a==0&&b==3,"whole-file replacement is valid");
    expect(splice(WaveOperation::duplicate,2,6).wasOk()&&equals({0,1,2,3,4,5,2,3,4,5,6,7,8,9})&&a==6&&b==10,"duplicate appends copied range and selects inserted audio");
    expect(splice(WaveOperation::duplicate,9,10).wasOk()&&equals({0,1,2,3,4,5,6,7,8,9,9}),"single-frame duplication at EOF is exact");
    expect(splice(WaveOperation::silence,2,6).wasOk()&&out.getNumSamples()==10&&out.getMagnitude(2,4)==0&&out.getSample(1,6)==source.getSample(1,6)&&a==2&&b==6,"silence preserves length and unselected stereo samples");
    expect(copyWaveRange(source,8000,1,3,out).wasOk()&&equals({1,2}),"copy is bounded and stereo");
    auto oldA=a,oldB=b;out.setSize(2,1);out.setSample(0,0,.7f);out.setSample(1,0,-.7f);
    auto atomic=[&]{return out.getNumSamples()==1&&out.getSample(0,0)==.7f&&out.getSample(1,0)==-.7f&&a==oldA&&b==oldB;};
    expect(splice(WaveOperation::erase,0,10).failed()&&atomic(),"reject removing every frame atomically");
    expect(splice(WaveOperation::trim,-1,4).failed()&&atomic(),"reject negative selection atomically");
    expect(splice(WaveOperation::erase,2,11).failed()&&atomic(),"reject selection beyond EOF atomically");
    expect(splice(WaveOperation::insert,2,2).failed()&&atomic(),"reject empty selection atomically");
    expect(spliceWave(source,8000,0,1,WaveOperation::insert,clipboard,7999,out,a,b).failed()&&atomic(),"invalid clipboard rate rejects atomically");
    AudioBuffer<float> empty;expect(spliceWave(source,8000,0,1,WaveOperation::insert,empty,8000,out,a,b).failed()&&atomic(),"empty clipboard leaves destination and selection intact");
    source.setSample(1,8,std::numeric_limits<float>::quiet_NaN());expect(splice(WaveOperation::trim,0,1).failed()&&atomic(),"non-finite source is rejected before editing");source.setSample(1,8,-.09f);
    clipboard.setSample(0,1,std::numeric_limits<float>::infinity());expect(splice(WaveOperation::replace,0,1).failed()&&atomic(),"non-finite clipboard is rejected atomically");clipboard.setSample(0,1,.4f);
    expect(validateWaveAudio(source,std::numeric_limits<double>::quiet_NaN()).failed(),"non-finite audio rate rejected");AudioBuffer<float> mono(1,10);mono.clear();expect(validateWaveAudio(mono,8000).failed(),"splice core requires stereo cache format");
    AudioBuffer<float> longAudio(2,960000);longAudio.clear();expect(spliceWave(longAudio,8000,0,1,WaveOperation::insert,clipboard,8000,out,a,b).failed()&&atomic(),"paste cannot exceed 120-second bound");
    auto song=emptyProject();song.bpm=137;auto path=root.getChildFile("grid-slice-source.wav");Project sliced=demoProject();auto before=JSON::toString(projectToJson(sliced));
    expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),44100,44101,37,43997,.25,3,sliced).wasOk(),"fractional-frame beat-grid slicing succeeds");
    bool contiguous=!sliced.clips.empty();int frame=37;double nextBeat=3;for(const auto& clip:sliced.clips){int first=(int)std::llround(clip.sourceOffset*44100),count=(int)std::llround(clip.length*60/song.bpm*44100);contiguous&=first==frame&&std::abs(clip.beat-nextBeat)<1e-10&&count>0&&clip.audioPath==path.getFullPathName();frame+=count;nextBeat=clip.beat+clip.length;}
    expect(contiguous&&frame==43997&&sliced.songMode&&sliced.clips.back().length<.25,"slices cover the entire selection once with no gaps and a short final slice");
    Project roundtrip;expect(projectFromJson(projectToJson(sliced),roundtrip).wasOk()&&JSON::toString(projectToJson(sliced))==JSON::toString(projectToJson(roundtrip)),"grid slices roundtrip as ordinary current-schema clips");
    sliced=demoProject();expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),44100,44101,37,43997,.25,4095,sliced).failed()&&JSON::toString(projectToJson(sliced))==before,"out-of-timeline slice batch is atomic");
    song.tracks[7].isBus=true;expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),44100,44101,0,44100,1,0,sliced).failed(),"bus destinations reject audio slicing");song.tracks[7].isBus=false;
    song.clips.resize(maxClips-1);expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),44100,44101,0,44100,.25,0,sliced).failed()&&JSON::toString(projectToJson(sliced))==before,"clip capacity rejection leaves entire output intact");song.clips.clear();
    expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),44100,44101,0,44100,0,0,sliced).failed(),"zero slicing grid rejected");expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),44100,44101,0,44100,std::numeric_limits<double>::quiet_NaN(),0,sliced).failed(),"non-finite slicing grid rejected");
    AudioBuffer<float> tone(2,48000);for(int i=0;i<48000;++i){tone.setSample(0,i,.3f*std::sin(i*.031));tone.setSample(1,i,.2f*std::sin(i*.019));}auto stream=path.createOutputStream();WavAudioFormat wav;std::unique_ptr<AudioFormatWriter> writer(stream?wav.createWriterFor(stream.get(),48000,2,32,{},0):nullptr);expect(writer!=nullptr,"create slice render fixture");if(writer){stream.release();writer->writeFromAudioSampleBuffer(tone,0,48000);writer.reset();}
    song.bpm=123;song.loop=false;song.songMode=true;song.tracks[7].send=0;expect(sliceWaveToPlaylist(song,7,path.getFullPathName(),48000,48000,311,47901,.25,0,sliced).wasOk(),"create renderable slice batch");auto reference=song;Clip region;region.track=7;region.audioPath=path.getFullPathName();region.sourceOffset=311/48000.;region.length=(47901-311)/48000.*123/60;reference.clips={region};auto sliceAudio=render(sliced,assets,root,52000,127),wholeAudio=render(reference,assets,root,52000,512);float difference=0;for(int ch=0;ch<2;++ch)for(int i=0;i<52000;++i)difference=jmax(difference,std::abs(sliceAudio.getSample(ch,i)-wholeAudio.getSample(ch,i)));expect(sliceAudio.getMagnitude(0,0,52000)>.001f&&difference<.00001f,"grid slices render like one uninterrupted source across callback sizes");
}
