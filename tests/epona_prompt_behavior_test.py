"""Exercise the Epona prompt hook without changing its native geometry or alpha."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/item_slot_hooks.cpp').read_text()
start = source.index('HookAction before_horse_spur_draw(')
end = source.index('\n}\n', start) + 3
hook = source[start:end]
fixture = r'''
#include <cassert>
struct ModContext {};
enum HookAction { HOOK_CONTINUE };
constexpr int BIND15=15, MIRROR0=0;
struct ResTIMG {} textures[15];
struct J2DPane {};
struct J2DPicture: J2DPane {
    const ResTIMG* texture=nullptr;
    int alpha=128, geometry=42, coordinates=0;
    bool visible=false, neutral=false;
    void changeTexture(const ResTIMG* value,int index) { assert(index==0); texture=value; }
    J2DPicture* getTexture(int index) { assert(index==0); return this; }
    void setTexCoord(J2DPicture*,int bind,int mirror,bool rotated) {
        assert(bind==15 && mirror==0 && !rotated); ++coordinates;
    }
    void show() { visible=true; }
};
struct CPaneMgr { J2DPane* pane; J2DPane* getPanePtr() { return pane; } };
struct dMeterHakusha_c { CPaneMgr* mpButtonA; };
namespace mods { template<class T> T arg(void* args,int index) { return static_cast<T>(static_cast<void**>(args)[index]); } }
J2DPicture* picture=nullptr;
int selected=0, hidden=0;
bool resourceAvailable=true;
J2DPicture* first_picture_pane(J2DPane* pane) { return pane ? picture : nullptr; }
const ResTIMG* menu_face_button_texture(bool action) { assert(action); return resourceAvailable ? &textures[selected] : nullptr; }
void set_neutral_picture_colors(J2DPicture* pane) { pane->neutral=true; }
void hide_other_pictures(J2DPane*,J2DPicture* keep) { assert(keep==picture); ++hidden; }
'''
checks = r'''
int main() {
    J2DPane group; CPaneMgr manager{&group}; dMeterHakusha_c spurs{&manager};
    void* args[]={&spurs}; J2DPicture button;
    for(selected=0;selected<15;++selected) {
        button={}; picture=&button;
        assert(before_horse_spur_draw(nullptr,args,nullptr,nullptr)==HOOK_CONTINUE);
        assert(button.texture==&textures[selected] && button.visible && button.neutral);
        assert(button.alpha==128 && button.geometry==42 && button.coordinates==1);
        before_horse_spur_draw(nullptr,args,nullptr,nullptr);
        assert(button.alpha==128 && button.geometry==42); // Repeated draws cannot drift.
    }
    int previous=hidden;
    resourceAvailable=false;
    before_horse_spur_draw(nullptr,args,nullptr,nullptr);
    picture=nullptr; resourceAvailable=true;
    before_horse_spur_draw(nullptr,args,nullptr,nullptr);
    spurs.mpButtonA=nullptr;
    before_horse_spur_draw(nullptr,args,nullptr,nullptr);
    args[0]=nullptr;
    before_horse_spur_draw(nullptr,args,nullptr,nullptr);
    assert(hidden==previous);
}
'''
with tempfile.TemporaryDirectory(prefix='hud-epona-test-') as directory:
    path=Path(directory)
    (path/'test.cpp').write_text(fixture+hook+checks)
    subprocess.run(['c++','-std=c++20',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: Epona action prompt routing, native geometry/alpha, repeated draws and missing resources')
