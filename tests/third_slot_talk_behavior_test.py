"""Exercise the actual third-slot event hook with a small native-event fixture."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/item_slot_hooks.cpp').read_text()
function=source.split('HookAction before_talk_item_check(',1)[1].split('\nvoid after_talk_queue_entry',1)[0]
function='HookAction before_talk_item_check('+function
fixture=r'''
#include <cassert>
#include <cstdint>
using u8=std::uint8_t;
struct ModContext {};
enum HookAction { HOOK_CONTINUE, HOOK_SKIP_ORIGINAL };
constexpr int dEvt_type_SHOWITEM_X_e=6, dEvtCnd_CANTALKITEM_e=1, dEvtCnd_CANTALK_e=2, dEvtCmd_INTALK_e=3, dEvt_mode_TALK_e=4;
constexpr u8 dItemNo_NONE_e=255;
struct EventInfo { bool canTalk=true; bool chkCondition(int) const { return canTalk; } };
struct fopAc_ac_c { EventInfo eventInfo; };
struct daAlink_c: fopAc_ac_c {};
struct dEvt_order_c { int mEventType=6; fopAc_ac_c *mpRequestActor=nullptr,*mpTargetActor=nullptr; };
struct dEvt_control_c {
    bool commonAllowed=true; int checked=0,mTalkXyType=0,mMode=0,mEventId=-1;
    u8 mPreItemNo=0xfe;
    bool commonCheck(dEvt_order_c*,int,int) { ++checked; return commonAllowed; }
};
struct Manager {
    int ordered=0;
    int getEventIdx(const char*,int,int) { return 42; }
    bool order(int id) { assert(id==42); ++ordered; return true; }
} manager;
Manager& dComIfGp_getEventManager() { return manager; }
namespace mods { template<class T> T arg(void* args,int index) { return static_cast<T>(static_cast<void**>(args)[index]); } }
struct ThirdSlotTalk { dEvt_order_c* order=nullptr; daAlink_c* player=nullptr; fopAc_ac_c* target=nullptr; u8 item=255; } s_thirdSlotTalk;
'''
checks=r'''
int main() {
    daAlink_c player; fopAc_ac_c telma,otherNpc;
    dEvt_order_c order{6,&player,&telma}; dEvt_control_c events;
    void* args[]={&events,&order}; int result=-1;
    s_thirdSlotTalk={&order,&player,&telma,0x80}; // Renado's Letter, irrespective of X equipment.
    assert(before_talk_item_check(nullptr,args,&result,nullptr)==HOOK_SKIP_ORIGINAL);
    assert(result==1 && events.mPreItemNo==0x80 && events.mTalkXyType==1);
    assert(events.mMode==4 && manager.ordered==1 && events.checked==1);
    assert(s_thirdSlotTalk.player==nullptr);
    // An ordinary X interaction does not inherit a previous R presentation.
    assert(before_talk_item_check(nullptr,args,&result,nullptr)==HOOK_CONTINUE);
    for(u8 item:{u8(0x80),u8(0x81),u8(0x82)}) {
        events={}; manager.ordered=0;
        s_thirdSlotTalk={&order,&player,&telma,item};
        before_talk_item_check(nullptr,args,&result,nullptr);
        assert(result==1 && events.mPreItemNo==item && manager.ordered==1);
    }
    events={}; manager.ordered=0;
    telma.eventInfo.canTalk=false;
    s_thirdSlotTalk={&order,&player,&telma,0x80};
    before_talk_item_check(nullptr,args,&result,nullptr);
    assert(result==0 && events.mPreItemNo==0xfe && manager.ordered==0 && events.checked==0);
    telma.eventInfo.canTalk=true; events.commonAllowed=false;
    s_thirdSlotTalk={&order,&player,&telma,0x80};
    before_talk_item_check(nullptr,args,&result,nullptr);
    assert(result==0 && manager.ordered==0);
    s_thirdSlotTalk={&order,&player,&otherNpc,0x80};
    assert(before_talk_item_check(nullptr,args,&result,nullptr)==HOOK_CONTINUE);
    dEvt_order_c otherOrder=order; args[1]=&otherOrder;
    s_thirdSlotTalk={&order,&player,&telma,0x80};
    assert(before_talk_item_check(nullptr,args,&result,nullptr)==HOOK_CONTINUE);
}
'''
with tempfile.TemporaryDirectory(prefix='hud-third-slot-test-') as folder:
    path=Path(folder); (path/'test.cpp').write_text(fixture+'\n'+function+'\n'+checks)
    subprocess.run(['c++','-std=c++20','-include','initializer_list',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: actual R-slot hook, letter identity, native checks, and isolation from X/other requests')
