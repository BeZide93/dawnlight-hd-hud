#include "dpad_map_policy.hpp"
#include "menu_input_state.hpp"
#include <cassert>
using namespace twilight_hd_hud;
int main() {
    constexpr unsigned up=1,left=2,right=4;
    auto fixed=dpad_map_masks(up,left|right,false,false,0,up,left,right);
    assert(fixed.map==up && fixed.minimap==(left|right) && fixed.released==0);
    auto follow=dpad_map_masks(up,right,false,false,0,up,left,right);
    assert(follow.map==up && follow.minimap==right);
    auto combined=dpad_map_masks(up,right,false,true,0,up,left,right);
    assert(combined.map==up && combined.minimap==up && combined.combined);
    assert(combined.released==(left|right));
    auto midna=dpad_map_masks(right,right,true,true,up,up,left,right);
    assert(midna.map==0 && midna.minimap==0);
    auto moved=dpad_map_masks(up,left,false,false,0,up,left,right);
    assert(moved.minimap==left);
    MinimapReturnState state;
    state.begin(true); state.choose(false);
    assert(!state.close(true,true)->visible);
    state.choose(true); assert(!state.close(true,true));
    state.reset(); state.begin(true);
    assert(state.close(true,true)->visible);
}
