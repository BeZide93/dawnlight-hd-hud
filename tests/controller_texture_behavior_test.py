"""Run the real prompt selectors with distinguishable native texture fixtures."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/item_slot_hooks.cpp').read_text()
sdk_include = next(path for path in (root / 'dusklight/sdk/include',
    root.parent / 'dusklight-sdk/sdk/include') if path.exists())
def function(name):
    start = source.index('ResTIMG const* ' + name + '(')
    end = source.index('\n}\n', start) + 3
    return source[start:end]
fixture = r'''
#include "controller_prompts.hpp"
#include <cassert>
#include <initializer_list>
using namespace twilight_hd_hud;
struct ResTIMG { int id; } textures[128];
struct ResourceBuffer { int id; };
ButtonLayout selected; ButtonStyle style;
ButtonLayout twilight_hd_hud::button_layout() { return selected; }
ButtonStyle twilight_hd_hud::button_style() { return style; }
ResTIMG const* resource_texture(ResourceBuffer resource) { return &textures[resource.id]; }
ResourceBuffer s_playStationL1ButtonResources[2] = {{1}, {5}};
ResourceBuffer s_blackProShoulderButtonResource{90}, s_blackProLShoulderButtonResource{91};
ResourceBuffer s_blackProZlShoulderButtonResource{92}, s_blackProZrShoulderButtonResource{93};
ResourceBuffer s_lShoulderButtonResource{94}, s_zlShoulderButtonResource{95}, s_zrShoulderButtonResource{96};
ResTIMG const* playstation_shoulder_button_texture(int i) { return &textures[2 + i + (uses_dark_buttons(style) ? 4 : 0)]; }
ResTIMG const* xbox_shoulder_button_texture(ShoulderPrompt i) { return &textures[50 + int(i)]; }
ResTIMG const* archive_texture(const char*) { return &textures[99]; }
ResTIMG const* styled_face_button_texture(char letter) { return &textures[int(letter)]; }
ResTIMG const* playstation_face_button_texture(char position) { return &textures[int(position) + 20]; }
ResTIMG const* styled_blank_face_button_texture() { return &textures[0]; }
'''
selectors = '\n'.join(function(name) for name in (
    'styled_r_button_texture', 'styled_l_button_texture', 'styled_zl_button_texture',
    'styled_zr_button_texture', 'menu_face_button_texture', 'item_assignment_button_texture'))
checks = r'''
int main() {
    for (auto s : {ButtonStyle::Silver, ButtonStyle::BlackPro}) {
        style = s;
        int offset = s == ButtonStyle::BlackPro ? 4 : 0;
        for (auto layout : {ButtonLayout::SteamDeck, ButtonLayout::SteamDeckBotw,
                ButtonLayout::PlayStationBotw, ButtonLayout::PlayStationFlippedBotw}) {
            selected = layout;
            assert(styled_l_button_texture() == &textures[1 + offset]);
            assert(styled_r_button_texture() == &textures[3 + offset]);
            assert(styled_zl_button_texture() == &textures[2 + offset]);
            assert(styled_zr_button_texture() == &textures[4 + offset]);
            for (bool first : {false, true}) {
                auto menu = menu_face_button_texture(first);
                auto item = item_assignment_button_texture(first);
                if (is_playstation_layout(layout)) {
                    assert(menu == playstation_face_button_texture(face_position_for_action(layout, first ? 'A' : 'B')));
                    assert(item == playstation_face_button_texture(face_position_for_action(layout, first ? 'X' : 'Y')));
                } else {
                    assert(menu == styled_face_button_texture(face_letter_for_action(layout, first ? 'A' : 'B')));
                    assert(item == styled_face_button_texture(face_letter_for_action(layout, first ? 'X' : 'Y')));
                }
            }
        }
    }
    // Existing Xbox shoulders must retain LB/RB/LT/RT routing.
    selected = ButtonLayout::BayxFlipped;
    assert(styled_l_button_texture() == &textures[50]);
    assert(styled_r_button_texture() == &textures[51]);
    assert(styled_zl_button_texture() == &textures[52]);
    assert(styled_zr_button_texture() == &textures[53]);
}
'''
with tempfile.TemporaryDirectory(prefix='hud-controller-test-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(fixture + selectors + checks)
    subprocess.run(['c++', '-std=c++20', '-I' + str(root / 'src'),
                    '-I' + str(sdk_include),
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual Deck/DualSense shoulder, menu and item selectors; Xbox unchanged')
