// SPDX-License-Identifier: AGPL-3.0-only
static void testSelectorAutomation(const File& root,Assets& assets){
    auto metadata=automationParametersToJson();
    for(int id=101;id<118;++id){
        const auto& d=automationParameters()[(size_t)id];auto labels=automationParameterChoices(id);
        expect(d.discrete&&!d.logarithmic&&labels.size()==(int)d.high+1,"every appended selector discovers its complete ordered labels");
        expect(metadata[id]["choices"].size()==labels.size()&&metadata[id]["instruments"].isArray()==isSynthAutomation(id),"selector metadata distinguishes effect and synth destinations");
        for(int value=0;value<labels.size();++value){
            float normal=automationParameterNormal(id,value);Track t;
            if(isSynthAutomation(id))applySynthAutomation(t.synth,t.attack,t.release,id,normal);else applyMixerAutomation(t.mixer,id,normal);
            expect(automationParameterValue(id,normal)==value&&automationParameterText(id,normal)==labels[value]&&std::abs(trackAutomationNormal(t,id)-normal)<1e-6,"all selector choices round-trip through real renderer fields and labels");
            expect(validateSynthPatch(t.synth).wasOk()&&validateMasterEffects(t.mixer).wasOk(),"every named selector leaves valid synth and mixer settings");
        }
    }
    expect(automationParameterValue(101,.499)==0&&automationParameterValue(101,.5)==1&&automationParameterValue(114,.49)==1&&automationParameterValue(114,.5)==2,"continuous selector curves round at deterministic half-value thresholds");
    auto p=emptyProject();p.bpm=120;p.songMode=true;p.loop=false;p.master=1;p.tracks[7].volume=.7f;p.tracks[7].cutoff=20000;
    auto file=root.getChildFile("selector-automation-source.wav");AudioBuffer<float> tone(2,96000);
    for(int i=0;i<96000;++i){tone.setSample(0,i,.22f*(float)std::sin(i*.043));tone.setSample(1,i,.13f*(float)std::cos(i*.067));}
    WavAudioFormat format;auto stream=file.createOutputStream();std::unique_ptr<AudioFormatWriter> writer(stream?format.createWriterFor(stream.get(),48000,2,24,{},0):nullptr);bool written=false;if(writer){stream.release();written=writer->writeFromAudioSampleBuffer(tone,0,96000);writer.reset();}expect(written,"create asymmetric stereo fixture for audible selector checks");
    if(!written)return;
    Clip source;source.track=7;source.length=4;source.audioPath=file.getFullPathName();p.clips={source};
    for(int id=101;id<118;++id){
        auto base=p;auto& m=base.tracks[7].mixer;m.low=9;m.high=-8;m.threshold=-30;m.ratio=8;m.reverbMix=.7f;m.chorus.mix=.8f;m.flanger.mix=.8f;m.phaser.mix=.8f;m.delay.interval=20;m.delay.mix=.7f;m.distortion.mix=1;
        if(id==109){m.stereo.width=.2f;m.stereo.trim=-4;}if(id>=110&&id<=113)m.stereo.enabled=true;
        if(id==114)m.compressor=true;if(id==115)m.distortion.enabled=true;
        if(isSynthAutomation(id)){base.clips[0].audioPath.clear();base.patterns[0].lanes[7]={{0,4,48,.8f}};base.tracks[7].instrument=Instrument::analog;base.tracks[7].synth.mix=id==116?0:1;base.tracks[7].synth.wave1=base.tracks[7].synth.wave2=0;}
        auto reference=base;float value=1;
        if(isSynthAutomation(id))applySynthAutomation(reference.tracks[7].synth,reference.tracks[7].attack,reference.tracks[7].release,id,value);else applyMixerAutomation(reference.tracks[7].mixer,id,value);
        auto automated=base;auto curve=parameterCurve(id,7,value,value);curve.length=4;curve.points.back().beat=4;automated.clips.push_back(curve);
        auto a=render(automated,assets,root,48000,127),b=render(reference,assets,root,48000,511),plain=render(base,assets,root,48000,257);
        float error=0;double changed=0;bool finite=true;
        for(int ch=0;ch<2;++ch)for(int n=24000;n<48000;++n){error=jmax(error,std::abs(a.getSample(ch,n)-b.getSample(ch,n)));changed+=std::abs(a.getSample(ch,n)-plain.getSample(ch,n));finite&=std::isfinite(a.getSample(ch,n));}
        if(error>=3e-6||changed<=.01)std::cerr<<"selector "<<id<<" error="<<error<<" difference="<<changed<<"\n";
        expect(finite&&error<3e-6,"constant selector automation converges to the corresponding static sound");
        expect(changed>.01,"each new selector changes actual stereo audio rather than metadata alone");
    }
    auto switched=p;auto curve=parameterCurve(108,7,0,0);curve.length=4;curve.points={{0,0,true},{.5,1,true},{1,0,true},{4,0,true}};switched.clips.push_back(curve);switched.tracks[7].mixer.distortion.drive=32;
    auto a=render(switched,assets,root,96000,127),b=render(switched,assets,root,96000,511);bool exact=true;for(int ch=0;ch<2;++ch)for(int n=0;n<96000;++n)exact&=a.getSample(ch,n)==b.getSample(ch,n)&&std::isfinite(a.getSample(ch,n));expect(exact,"selector changes and 5ms effect-bypass fades are callback-size independent");
    auto dry=render(p,assets,root,96000,257);bool dryAgain=true;for(int ch=0;ch<2;++ch)for(int n=30000;n<48000;++n)dryAgain&=a.getSample(ch,n)==dry.getSample(ch,n);expect(dryAgain,"completed bypass fade returns bit-exact dry audio after the effect was active");
    RenderSettings settings;settings.bits=32;settings.endBeat=4;settings.tailSeconds=0;auto out=root.getChildFile("selector-automation.wav");expect(renderAudio(switched,assets,root,out,settings).wasOk(),"selector automation exports using the common renderer");String warning;auto rendered=assets.load(out,warning);bool matches=rendered&&rendered->frameCount()==96000;if(matches)for(int ch=0;ch<2;++ch)for(int n=0;n<96000;++n)matches&=std::abs(rendered->audio.getSample(ch,n)-a.getSample(ch,n))<1e-6;expect(matches,"float WAV export matches callback selector changes and bypass fades");
    auto engine=makeEngine(switched,assets,root);AudioBuffer<float> block(2,256);engine->seekRequested.store(.75);engine->render(block.getWritePointer(0),block.getWritePointer(1),256);expect(engine->automationReadout[7][108].load()==1,"seek chases a held effect selector inside its active interval");engine->seekRequested.store(5);engine->render(block.getWritePointer(0),block.getWritePointer(1),256);expect(engine->automationReadout[7][108].load()==-1,"seeking beyond a selector clip clears its live readout");
    switched.loop=true;switched.markers={{"phrase","Phrase",.5,.75,0}};switched.loopSection="phrase";auto loop=makeEngine(switched,assets,root);loop->seekRequested.store(.745);loop->render(block.getWritePointer(0),block.getWritePointer(1),256);expect(loop->completedLoops.load()>0&&loop->automationReadout[7][108].load()==1,"section-loop wrap restores the selector at the loop start");
    auto state=projectToJson(switched);Project read;expect(projectFromJson(state,read).wasOk()&&read.clips[1].automationTarget==108,"schema 30 saves and loads selector automation");auto before=JSON::toString(projectToJson(read));state.getDynamicObject()->setProperty("schema",29);expect(projectFromJson(state,read).failed()&&JSON::toString(projectToJson(read))==before,"selector IDs mislabeled as schema 29 reject atomically");state=projectToJson(p);state.getDynamicObject()->setProperty("schema",29);expect(projectFromJson(state,read).wasOk(),"actual schema-29 structure remains readable with neutral selector defaults");
    auto operations=JSON::parse(R"([{"op":"add_automation","track":7,"target":"distortion.enabled","targetTrack":-1,"beat":0,"length":4,"points":[{"beat":0,"value":0,"hold":true},{"beat":4,"value":1}]},{"op":"add_automation","track":7,"target":"synth.wave1","targetTrack":7,"beat":0,"length":4,"points":[{"beat":0,"value":0},{"beat":4,"value":1}]}])");Project edited;expect(applyControlEdits(p,operations,edited).wasOk()&&edited.clips[1].automationTrack==-1&&edited.clips[1].automationTarget==108&&edited.clips[2].automationTarget==116,"named MCP selectors create independent Master and synth destinations");
    auto bus=p;bus.tracks[6].isBus=true;auto raw=projectToJson(bus);raw["clips"].getArray()->add(projectToJson(edited)["clips"][1]);raw["clips"][1].getDynamicObject()->setProperty("automationTrack",6);expect(projectFromJson(raw,read).wasOk(),"appended effect selectors remain valid on bus destinations");raw["clips"][1].getDynamicObject()->setProperty("automationTarget",116);expect(projectFromJson(raw,read).failed(),"new waveform selectors reject bus destinations");
}
