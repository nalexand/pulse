// SPDX-License-Identifier: AGPL-3.0-only
#include "StudioViewport.h"

class PulseApplication final : public juce::JUCEApplication {
public:
    // Share single-instance and recovery protocols with schema-13 releases.
    const juce::String getApplicationName() override { return "Pulse Studio 0.29"; }
    const juce::String getApplicationVersion() override { return "0.37.0"; }
    bool moreThanOneInstanceAllowed() override {
        auto args=getCommandLineParameters();
        return args.contains("--diagnose")||args.contains("--transport-checks")||args.contains("--request-update")||args.contains("--preview")||args.contains("--editor-checks")||args.contains("--audio-smoke")||args.contains("--window-checks")||args.contains("--control-session");
    }
    void initialise(const juce::String& commandLine) override {
        auto args=juce::StringArray::fromTokens(commandLine,true); args.removeEmptyStrings();
        juce::File root=juce::File::getCurrentWorkingDirectory();
        auto folder=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        for(int i=0;i<5;++i){if(folder.getChildFile("IMPLEMENTATION_PLAN.md").existsAsFile()){root=folder;break;}folder=folder.getParentDirectory();}
        for (int i=0;i<args.size()-1;++i) if (args[i]=="--workspace") root=juce::File(args[i+1].unquoted());
        if(args.contains("--control-session")&&root.getParentDirectory().getFileName()!="test-output"){setApplicationReturnValue(1);quit();return;}
        if(args.contains("--diagnose")){
            auto folder=root.getChildFile("data");auto result=folder.createDirectory();if(result.failed()||!folder.getChildFile("diagnose-v0.29.request").replaceWithText("snapshot"))setApplicationReturnValue(1);quit();return;
        }
        if(args.contains("--request-update")){
            auto folder=root.getChildFile("data");auto result=folder.createDirectory();
            if(result.failed()||!folder.getChildFile("close-for-update-v0.29.request").replaceWithText(juce::String(juce::Time::currentTimeMillis())))setApplicationReturnValue(1);
            quit();return;
        }
        for (int i=0;i<args.size()-1;++i) if (args[i]=="--preview") {
            pulse::Studio preview(root,false); preview.setSize(1440,940);
            for(int j=0;j<args.size()-1;++j)if(args[j]=="--open")preview.openProject(juce::File(args[j+1].unquoted()));
            if (!preview.snapshot(juce::File(args[i+1].unquoted()))) setApplicationReturnValue(1);
            quit(); return;
        }
        for (int i=0;i<args.size()-1;++i) if (args[i]=="--editor-checks") {
            pulse::Studio tester(root,false);tester.setSize(1440,940);
            if(!tester.runEditorChecks(juce::File(args[i+1].unquoted()),args.contains("--typing-only"),args.contains("--arrangement-only"),args.contains("--appearance-only"),args.contains("--selection-only"),args.contains("--sections-only"),args.contains("--resampling-only")))setApplicationReturnValue(1);
            quit();return;
        }
        for(int i=0;i<args.size()-1;++i)if(args[i]=="--window-checks"){
            window=std::make_unique<Window>(root,false,false);if(!window->runWindowChecks(juce::File(args[i+1].unquoted())))setApplicationReturnValue(1);window.reset();quit();return;
        }
        window=std::make_unique<Window>(root,true);
        if(args.contains("--enable-control"))window->studio()->setLocalControlEnabled(true);
        for (int i=0;i<args.size()-1;++i) if (args[i]=="--open") window->studio()->openProject(juce::File(args[i+1].unquoted()));
        if(args.contains("--audio-smoke"))window->studio()->startAudioSmoke();
        if(args.contains("--play"))window->studio()->startPlayback();
        for(int i=0;i<args.size()-1;++i)if(args[i]=="--transport-checks")window->studio()->startTransportChecks(juce::File(args[i+1].unquoted()));
    }
    void shutdown() override { window.reset(); }
    void anotherInstanceStarted(const juce::String& commandLine) override {
        if(window){if(commandLine.contains("--enable-control"))window->studio()->setLocalControlEnabled(true);window->setVisible(true);window->toFront(true);if(!juce::Component::getCurrentlyModalComponent())window->studio()->grabKeyboardFocus();if(commandLine.contains("--play"))window->studio()->startPlayback();}
    }
    void systemRequestedQuit() override {
        if (!window || !window->studio()->isDirty()) { quit(); return; }
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Close Pulse Studio?").withMessage("Your latest work has a local recovery copy. Save your project before closing to choose a permanent file.")
            .withButton("Keep working").withButton("Close with recovery"),[](int result) { if (result==0) juce::JUCEApplication::getInstance()->quit(); });
    }
private:
    class Window final : public juce::DocumentWindow,private juce::Timer {
    public:
        explicit Window(const juce::File& root,bool resume=false,bool audio=true):DocumentWindow("Pulse Studio 0.37.0",juce::Colour(0xff343b3f),allButtons) {
            setUsingNativeTitleBar(true);host=new pulse::StudioViewport(root,audio,resume);setContentOwned(host,false);setResizable(true,false);
            studio()->fitMainWindow=[this]{if(isFullScreen())setFullScreen(false);fitToDesktop();};
            fitToDesktop(true);setVisible(true);fitToDesktop();studio()->grabKeyboardFocus();startTimer(1000);
        }
        ~Window()override{stopTimer();studio()->fitMainWindow={};}
        pulse::Studio* studio(){return host?&host->editor():nullptr;}
        void activeWindowStatusChanged()override{if(isActiveWindow()&&!juce::Component::getCurrentlyModalComponent())if(auto* s=studio())s->grabKeyboardFocus();}
        void closeButtonPressed()override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
        bool runWindowChecks(const juce::File& folder){
            using namespace juce;folder.createDirectory();Array<var> checks;int failures=0;auto check=[&](bool passed,const String& name){auto* item=new DynamicObject();item->setProperty("check",name);item->setProperty("passed",passed);checks.add(var(item));if(!passed)++failures;};
            BorderSize<int> testFrame{31,8,8,8};for(auto area:{Rectangle<int>{0,0,1920,1040},Rectangle<int>{0,0,1366,728},Rectangle<int>{0,0,1280,680},Rectangle<int>{0,0,1024,560},Rectangle<int>{0,0,800,560},Rectangle<int>{0,0,640,400},Rectangle<int>{-1600,80,1600,860}}){for(auto desired:{Rectangle<int>{-10000,-10000,1440,940},Rectangle<int>{9000,9000,1180,800},Rectangle<int>{area.getX()+20,area.getY()+50,700,430}}){auto fit=pulse::fitClientToWorkArea(desired,area,testFrame);check(area.contains(testFrame.addedTo(fit)),"native title bar and all borders fit work area "+area.toString());auto next=pulse::fitClientToWorkArea(fit,area,testFrame);check(next==fit,"screen fit is stable after repeated checks");}}
            auto* display=Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());auto realArea=display?display->userArea:Rectangle<int>{0,0,1920,1080};
            for(auto size:{Point<int>{1280,680},Point<int>{1024,560},Point<int>{800,560},Point<int>{640,400}}){auto area=Rectangle<int>(realArea.getX(),realArea.getY(),size.x,size.y).getIntersection(realArea);constrainToArea(area,1,true);auto frame=frameSize();check(area.contains(frame.addedTo(getBounds())),"actual native window fits simulated work area "+size.toString());check(studio()->getWidth()>=pulse::minimumCanvasWidth&&studio()->getHeight()>=pulse::minimumCanvasHeight,"small window retains a complete logical workstation");auto visible=host->getLocalArea(studio(),studio()->getLocalBounds()).toFloat();check(host->getLocalBounds().toFloat().expanded(2).contains(visible)&&visible.getWidth()>=host->getWidth()-1&&visible.getHeight()>=host->getHeight()-1,"scaled canvas fills native content without inaccessible edges");
                auto point=studio()->localPointToGlobal(Point<float>{411,22});auto physical=host->getLocalPoint(nullptr,point);auto restored=studio()->getLocalPoint(host,physical);check(restored.getDistanceFrom(Point<float>{411,22})<.01f&&host->getComponentAt(physical.roundToInt())==studio(),"native hit testing and input coordinates follow the scaled toolbar");auto image=host->createComponentSnapshot(host->getLocalBounds());PNGImageFormat png;FileOutputStream stream(folder.getChildFile("screen-"+String(size.x)+"x"+String(size.y)+".png"));check(stream.openedOk()&&png.writeImageToStream(image,stream),"render fitted workspace "+size.toString());
                if(size.x==800){auto time=Time::getCurrentTime();Point<float> menuPoint{14,56};MouseEvent event(Desktop::getInstance().getMainMouseSource(),menuPoint,ModifierKeys(ModifierKeys::leftButtonModifier),1,0,0,0,0,studio(),studio(),time,menuPoint,time,1,false);studio()->mouseDown(event);studio()->mouseUp(event);auto* menu=Component::getCurrentlyModalComponent();auto anchor=studio()->localPointToGlobal(menuPoint).roundToInt();check(menu&&realArea.expanded(2).contains(menu->getScreenBounds())&&menu->getScreenBounds().expanded(24).contains(anchor),"scaled File menu stays on screen and anchored to its toolbar");PopupMenu::dismissAllActiveMenus();}
            }
            for(auto screen:{Point<int>{1600,950},Point<int>{800,560}}){auto area=Rectangle<int>{realArea.getX(),realArea.getY(),screen.x,screen.y}.getIntersection(realArea);constrainToArea(area,1,true);for(int requested:pulse::interfaceScales){check(studio()->setInterfaceScale(requested),"set native interface scale "+String(requested));auto actual=host->canvasScale();auto fit=pulse::fitCanvas(host->getWidth(),host->getHeight(),requested/100.f);check(std::abs(actual-fit.scale)<.00001&&actual<=requested/100.f+.00001,"native scale respects requested zoom and full-canvas fit");auto p=studio()->localPointToGlobal(Point<float>{411,22});auto local=host->getLocalPoint(nullptr,p);check(host->getComponentAt(local.roundToInt())==studio()&&studio()->getLocalPoint(host,local).getDistanceFrom(Point<float>{411,22})<.01f,"manual zoom preserves toolbar hit testing");}}
            studio()->setInterfaceScale(100);
            setBoundsConstrained(getBounds().withSize(640,400));check(getWidth()<=640&&getHeight()<=400,"native resize constraints allow shrinking below the old minimum");
            constrainToArea(realArea,1,true);setBounds(getX()+600,getY()+500,1440,940);auto smaller=Rectangle<int>{realArea.getX(),realArea.getY(),800,560}.getIntersection(realArea);constrainToArea(smaller,1,false);check(smaller.contains(frameSize().addedTo(getBounds())),"display shrink recovers existing window bounds");
            // Simulate a disconnected monitor / unreachable older window, then use the real shortcut.
            setBounds(-10000,-10000,1440,940);studio()->keyPressed(KeyPress('H',ModifierKeys::ctrlModifier|ModifierKeys::shiftModifier,'H'));check(realArea.contains(frameSize().addedTo(getBounds())),"Ctrl+Shift+H recovers an offscreen main window");check(!studio()->isDirty(),"window fitting and recovery shortcut leave musical data unchanged");
            auto* report=new DynamicObject();report->setProperty("failures",failures);report->setProperty("checks",checks);report->setProperty("passed",failures==0);report->setProperty("actualWorkArea",realArea.toString());report->setProperty("actualFrame",String(frameSize().getTop())+","+String(frameSize().getLeft())+","+String(frameSize().getBottom())+","+String(frameSize().getRight()));folder.getChildFile("window-checks.json").replaceWithText(JSON::toString(var(report),false));return failures==0;
        }
    private:
        pulse::StudioViewport* host=nullptr;
        juce::Rectangle<int> lastArea;
        juce::BorderSize<int> lastFrame;
        double lastScale=0;
        juce::BorderSize<int> frameSize()const{return getPeer()?getPeer()->getFrameSize():juce::BorderSize<int>{};}
        void constrainToArea(juce::Rectangle<int> area,double scale,bool initial){
            auto frame=frameSize();auto available=pulse::availableClientArea(area,frame);auto desired=initial?juce::Rectangle<int>{0,0,1440,940}.withCentre(available.getCentre()):getBounds();
            setResizeLimits(juce::jmin(640,available.getWidth()),juce::jmin(400,available.getHeight()),juce::jmax(3000,area.getWidth()),juce::jmax(2000,area.getHeight()));
            if(!isFullScreen())setBounds(pulse::fitClientToWorkArea(desired,area,frame));lastArea=area;lastFrame=frame;lastScale=scale;
        }
        void fitToDesktop(bool initial=false){auto& displays=juce::Desktop::getInstance().getDisplays();auto* display=initial?displays.getDisplayForPoint(juce::Desktop::getInstance().getMainMouseSource().getScreenPosition()):displays.getDisplayForRect(getScreenBounds());if(!display)display=displays.getPrimaryDisplay();if(display)constrainToArea(display->userArea,display->scale,initial);}
        void timerCallback()override{if(isMinimised())return;auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());if(display&&(lastArea!=display->userArea||lastScale!=display->scale||lastFrame!=frameSize()))constrainToArea(display->userArea,display->scale,false);}
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(PulseApplication)
