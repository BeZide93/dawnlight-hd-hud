#include "dpad_map_policy.hpp"
#include "menu_input_state.hpp"
#include <cassert>
#include <initializer_list>
using namespace twilight_hd_hud;
int main() {
    constexpr unsigned up=1,left=2,right=4;
    for(bool l:{false,true}) for(bool r:{false,true}) {
        auto fixed=dpad_map_masks(up,left|right,false,false,l,r,0,up,left,right);
        assert(fixed.map==up && fixed.minimap==((l?left:0)|(r?right:0)));
        auto follow=dpad_map_masks(up,right,false,false,l,r,0,up,left,right);
        assert(follow.map==up && follow.minimap==(r?right:0));
        auto combined=dpad_map_masks(up,right,false,true,l,r,0,up,left,right);
        assert(combined.map==up && combined.minimap==up && combined.combined);
        assert(combined.released==(left|right));
    }
    auto midna=dpad_map_masks(right,right,true,true,true,true,up,up,left,right);
    assert(midna.map==0 && midna.minimap==0);
    auto moved=dpad_map_masks(up,left,false,false,false,true,0,up,left,right);
    assert(moved.minimap==0);
    MinimapReturnState state;
    state.begin(true); state.choose(false);
    assert(!state.close(true,true)->visible);
    state.choose(true); assert(!state.close(true,true));
    state.reset(); state.begin(true);
    assert(state.close(true,true)->visible);
}
