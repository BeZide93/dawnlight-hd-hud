#include "update_policy.hpp"
#include "update_hash.hpp"
#include <cassert>
#include <sstream>
using namespace twilight_hd_hud::update_policy;
int main() {
    assert(newer("v2.5.0","2.4.4")); assert(newer("v2.10.0","2.9.9"));
    assert(!newer("v2.4.4","2.4.4"));
    for(const auto bad:{"v2.4.4-preview.1","2","2.4","02.4.4","2.4.4x","4294967296.0.0"}) {
        std::array<std::uint32_t,3> parts{}; assert(!version(bad,parts));
    }
    const std::string digest(64,'a');
    const std::string json=R"({"tag_name":"v2.5.0","body":"fake \"name\" metadata","assets":[{"name":"Wrong.dusk","browser_download_url":"https://example.com"},{"name":"Twilight-HD-HUD.dusk","uploader":{"name":"Another author"},"size":2048,"digest":"sha256:)"+digest+R"(","browser_download_url":"https://github.com/OTPR26/twilight-hd-hud/releases/download/v2.5.0/Twilight-HD-HUD.dusk"}]})";
    Release releaseInfo;
    assert(release(json,"Twilight-HD-HUD.dusk",releaseInfo));
    assert(releaseInfo.tag=="v2.5.0" && releaseInfo.size==2048 && releaseInfo.digest==digest);
    assert(!release(json,"Twilight-HD-HUD-tvOS.dusk",releaseInfo));
    auto wrong=json; wrong.replace(wrong.find("github.com/OTPR26"),16,"github.com/Other");
    assert(!release(wrong,"Twilight-HD-HUD.dusk",releaseInfo));
    assert(!release(json+"junk","Twilight-HD-HUD.dusk",releaseInfo));
    assert(!release(json.substr(0,json.size()-1),"Twilight-HD-HUD.dusk",releaseInfo));
    auto prerelease=json; prerelease.insert(1,"\"prerelease\":true,");
    assert(!release(prerelease,"Twilight-HD-HUD.dusk",releaseInfo));
    std::istringstream empty("");
    assert(sha256(empty)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    std::istringstream abc("abc");
    assert(sha256(abc)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::istringstream million(std::string(1000000,'a'));
    assert(sha256(million)=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
