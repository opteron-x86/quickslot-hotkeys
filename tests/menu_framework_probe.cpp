#include "MenuFramework.h"
#include "MenuFrameworkMock.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace quickslot;
FrameworkStatus status;
unsigned requests=0,checks=0;
bool accept=true;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
int main(int argc,char** argv)try {
    if(argc!=3)return 2;
    const FrameworkClient client{[]{return status;},[]{++requests;if(accept)status.pending=true;return accept;}};
    check(!registerMenuFramework(client)&&!frameworkCapturingInput(),"Missing framework must remain optional");
    auto incomplete=LoadLibraryA(argv[1]);check(incomplete!=nullptr,"Load incomplete SDK fixture");
    check(!registerMenuFramework(client),"Missing SDK export must reject registration without a null call");
    FreeLibrary(incomplete);
    auto full=LoadLibraryA(argv[2]);check(full!=nullptr,"Load complete SDK fixture");
    const auto getMock=reinterpret_cast<MockSMF*(*)()>(GetProcAddress(full,"GetMockState"));
    check(getMock!=nullptr,"Fixture API available");auto& mock=*getMock();
    check(registerMenuFramework(client),"Real bridge registers using SDK ABI");
    check(registerMenuFramework(client)&&mock.registrations==1,"Repeated initialization cannot duplicate page");
    check(std::strcmp(mock.path,"Quickslot/Equipment & widget")==0&&mock.render,"Registered page and render callback are valid");
    mock.window.open=true;check(frameworkCapturingInput(),"Main panel owns input");
    mock.window.blocking=false;check(frameworkCapturingInput(),"Resumed main panel still owns editing keys");
    mock.window.open=false;mock.otherBlocking=true;check(frameworkCapturingInput(),"Other blocking windows inhibit Quickslot");
    mock.otherBlocking=false;check(!frameworkCapturingInput(),"Closed framework releases input");
    mock.window.open=true;mock.click=true;mock.render();
    check(requests==0&&mock.window.open,"No loaded game disables editor action");
    status.ready=true;status.groups=3;status.entries=6;accept=false;mock.click=true;mock.render();
    check(requests==1&&mock.window.open,"Rejected request preserves framework window");
    accept=true;mock.click=true;mock.render();
    check(requests==2&&status.pending&&!mock.window.open,"Accepted request closes framework via its atomic window API");
    mock.click=true;mock.render();check(requests==2,"Pending request prevents duplicate activation");
    status.pending=false;status.editing=true;mock.click=true;mock.render();check(requests==2,"Open editor disables relaunch");
    check(mock.texts>0&&!mock.disabled,"SDK renderer used exported UI functions and balanced disabled scope");
    std::printf("PASS: %u SKSE Menu Framework bridge ABI/registration/input/handoff checks. Simulated framework, no Skyrim engine.\n",checks);
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
