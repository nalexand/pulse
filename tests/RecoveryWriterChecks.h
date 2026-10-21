// SPDX-License-Identifier: AGPL-3.0-only
#include "RecoveryWriter.h"
#include <stdexcept>
static void testRecoveryWriter(const File& parent){
    auto root=parent.getChildFile("writer-"+Uuid().toString());root.createDirectory();auto file=root.getChildFile("recovery.pulse");
    auto source=emptyProject();source.title="Captured revision";source.patterns[0].lanes[7]={{0,1,60,.8f}};
    WaitableEvent reached,release;std::atomic<bool> offThread{false};auto owner=std::this_thread::get_id();
    ProjectSaveControl paused;paused.checkpoint=[&](ProjectSaveStage stage,const File&){if(stage==ProjectSaveStage::beforeCommit){offThread.store(std::this_thread::get_id()!=owner);reached.signal();return release.wait(5000)?Result::ok():Result::fail("Test gate timed out");}return Result::ok();};
    RecoveryWriter writer;expect(writer.start(source,file,41,&paused),"background writer accepts an immutable project snapshot");
    expect(reached.wait(5000)&&offThread.load(),"recovery validation and commit execute on a distinct worker thread");
    source.title="Edited after capture";source.patterns[0].lanes[7][0].pitch=72;
    expect(writer.active()&&!writer.poll()&&!file.exists(),"poll remains nonblocking while the worker is paused before publication");
    expect(!writer.start(source,file,42),"a busy writer never queues or races a second recovery snapshot");
    release.signal();auto first=writer.wait();Project read;
    expect(first&&first->result.wasOk()&&first->revision==41,"completion acknowledges only the revision actually captured");
    expect(loadProject(file,read).wasOk()&&read.title=="Captured revision"&&read.patterns[0].lanes[7][0].pitch==60,"editing the source while recovery runs cannot mutate its captured musical content");
    expect(!writer.active()&&!writer.poll()&&!writer.wait(),"completion is consumed once and wait leaves no outstanding writer");
    expect(writer.start(source,file,42),"the next revision can start after a completed writer is joined");auto second=writer.wait();
    expect(second&&second->result.wasOk()&&loadProject(file,read).wasOk()&&read.title==source.title,"a followup captures the newest source and replaces recovery safely");
    expect(loadProject(projectBackupFile(file),read).wasOk()&&read.title=="Captured revision","background replacement retains the previous verified generation");
    auto blocked=root.getChildFile("blocked.pulse");blocked.createDirectory();writer.start(source,blocked,43);auto failed=writer.wait();
    expect(failed&&failed->result.failed()&&failed->revision==43&&!writer.active(),"worker write failures are delivered without losing their revision or leaving a stuck worker");
    blocked.deleteFile();writer.start(source,blocked,43);auto retried=writer.wait();expect(retried&&retried->result.wasOk()&&loadProject(blocked,read).wasOk(),"a failed recovery can be retried on the same worker");
    auto invalid=source;invalid.patterns[0].lanes[7][0].pitch=128;auto token=projectFileToken(file);writer.start(invalid,file,44);auto rejected=writer.wait();expect(rejected&&rejected->result.failed()&&projectFileToken(file)==token,"worker validation rejects silent source repair and retains committed recovery");
    ProjectSaveControl throwing;throwing.checkpoint=[](ProjectSaveStage,const File&)->Result{throw std::runtime_error("Injected worker exception");};writer.start(source,file,45,&throwing);auto exception=writer.wait();expect(exception&&exception->result.failed()&&exception->result.getErrorMessage().contains("Injected worker exception")&&projectFileToken(file)==token,"worker exceptions report failure and leave the existing recovery untouched");
    {RecoveryWriter closing;source.title="Destructor barrier";closing.start(source,file,46);}
    expect(loadProject(file,read).wasOk()&&read.title=="Destructor barrier","destruction joins and publishes an already requested snapshot before releasing its state");
    file.deleteFile();projectBackupFile(file).deleteFile();Thread::sleep(20);expect(!file.exists(),"after a barrier and retirement no old worker can republish recovery");
}
