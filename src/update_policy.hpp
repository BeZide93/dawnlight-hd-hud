#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace twilight_hd_hud::update_policy {
struct Json {
    enum Kind { Null, Bool, Number, String, Object, Array } kind = Null;
    std::string text;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    const Json& at(const char* key) const {
        static const Json empty;
        const auto it = object.find(key);
        return it == object.end() ? empty : it->second;
    }
};
// Small bounded JSON reader: asset fields are read only from their own object,
// never from an author object, release body, or neighboring asset.
class Reader {
    std::string_view input;
    std::size_t pos = 0;
    void space() { while (pos < input.size() && (input[pos]==' ' || input[pos]=='\n' || input[pos]=='\r' || input[pos]=='\t')) ++pos; }
    bool take(char c) { space(); if (pos == input.size() || input[pos]!=c) return false; ++pos; return true; }
    bool string(std::string& out) {
        if (!take('"')) return false;
        while (pos < input.size()) {
            unsigned char c = input[pos++];
            if (c=='"') return true;
            if (c < 32) return false;
            if (c!='\\') { out += c; continue; }
            if (pos==input.size()) return false;
            c=input[pos++];
            switch(c) {
            case '"': case '\\': case '/': out+=c; break;
            case 'b': out+='\b'; break; case 'f': out+='\f'; break;
            case 'n': out+='\n'; break; case 'r': out+='\r'; break; case 't': out+='\t'; break;
            case 'u': {
                if (input.size()-pos<4) return false;
                unsigned code=0;
                for (int i=0;i<4;++i) {
                    c=input[pos++]; unsigned v;
                    if (c>='0' && c<='9') v=c-'0'; else if(c>='a'&&c<='f') v=c-'a'+10;
                    else if(c>='A'&&c<='F') v=c-'A'+10; else return false;
                    code=code*16+v;
                }
                // Relevant version, name, digest and URL fields are ASCII.
                // Non-ASCII text is ignored; it can never become an accepted URL.
                out+=code<128 ? static_cast<char>(code) : '?';
                break;
            }
            default: return false;
            }
        }
        return false;
    }
    bool value(Json& out, unsigned depth) {
        if (depth>32) return false;
        space(); if(pos==input.size()) return false;
        if(input[pos]=='"') { out.kind=Json::String; return string(out.text); }
        if(take('{')) {
            out.kind=Json::Object; if(take('}')) return true;
            do { std::string key; Json child;
                if(!string(key) || !take(':') || !value(child,depth+1) || out.object.count(key)) return false;
                out.object.emplace(std::move(key),std::move(child));
                if(take('}')) return true;
            } while(take(','));
            return false;
        }
        if(take('[')) {
            out.kind=Json::Array; if(take(']')) return true;
            do { Json child; if(!value(child,depth+1)) return false;
                out.array.push_back(std::move(child)); if(take(']')) return true;
            } while(take(','));
            return false;
        }
        for (const auto literal : {"true", "false", "null"}) {
            const std::string_view text(literal);
            if(input.substr(pos,text.size())==text) { pos+=text.size(); out.kind=text=="null"?Json::Null:Json::Bool; out.text=text; return true; }
        }
        const auto begin=pos;
        if(input[pos]=='-') ++pos;
        if(pos==input.size()) return false;
        if(input[pos]=='0') ++pos;
        else {
            if(input[pos]<'1'||input[pos]>'9') return false;
            while(pos<input.size() && input[pos]>='0'&&input[pos]<='9') ++pos;
        }
        if(pos<input.size() && input[pos]=='.') {
            const auto start=++pos;
            while(pos<input.size() && input[pos]>='0'&&input[pos]<='9') ++pos;
            if(pos==start) return false;
        }
        if(pos<input.size() && (input[pos]=='e'||input[pos]=='E')) {
            ++pos; if(pos<input.size() && (input[pos]=='+'||input[pos]=='-')) ++pos;
            const auto start=pos;
            while(pos<input.size() && input[pos]>='0'&&input[pos]<='9') ++pos;
            if(pos==start) return false;
        }
        out.kind=Json::Number; out.text=input.substr(begin,pos-begin); return true;
    }
public:
    explicit Reader(std::string_view text):input(text) {}
    bool read(Json& out) { if(input.size()>1024*1024 || !value(out,0)) return false; space(); return pos==input.size(); }
};
inline bool version(std::string_view text, std::array<std::uint32_t,3>& parts) {
    if(!text.empty() && text.front()=='v') text.remove_prefix(1);
    for(std::size_t i=0;i<parts.size();++i) {
        const auto dot=text.find('.');
        const auto part=text.substr(0,dot);
        if(part.empty() || (part.size()>1 && part.front()=='0')) return false;
        const auto result=std::from_chars(part.data(),part.data()+part.size(),parts[i]);
        if(result.ec!=std::errc{} || result.ptr!=part.data()+part.size()) return false;
        if(i==2) return dot==std::string_view::npos;
        if(dot==std::string_view::npos) return false;
        text.remove_prefix(dot+1);
    }
    return false;
}
inline bool newer(const std::string& latest, const std::string& current) {
    std::array<std::uint32_t,3> lhs{},rhs{};
    return version(latest,lhs) && version(current,rhs) && lhs>rhs;
}
struct Release { std::string tag, url, digest; std::uint64_t size=0; };
inline bool release(std::string_view text, const std::string& assetName, Release& out) {
    Json root; if(!Reader(text).read(root) || root.kind!=Json::Object) return false;
    if(root.at("draft").text=="true" || root.at("prerelease").text=="true") return false;
    const auto& tag=root.at("tag_name"); std::array<std::uint32_t,3> parts{};
    if(tag.kind!=Json::String || !version(tag.text,parts)) return false;
    const auto& assets=root.at("assets"); if(assets.kind!=Json::Array) return false;
    unsigned found=0;
    for(const auto& asset:assets.array) {
        if(asset.kind!=Json::Object || asset.at("name").kind!=Json::String || asset.at("name").text!=assetName) continue;
        ++found; const auto& url=asset.at("browser_download_url"); const auto& size=asset.at("size");
        const auto& digest=asset.at("digest");
        const std::string expected="https://github.com/OTPR26/twilight-hd-hud/releases/download/"+tag.text+"/"+assetName;
        if(url.kind!=Json::String || url.text!=expected || size.kind!=Json::Number || digest.kind!=Json::String) return false;
        std::uint64_t bytes=0;
        const auto parsed=std::from_chars(size.text.data(),size.text.data()+size.text.size(),bytes);
        if(parsed.ec!=std::errc{} || parsed.ptr!=size.text.data()+size.text.size() || bytes<1024 || bytes>512*1024*1024) return false;
        if(digest.text.size()!=71 || digest.text.substr(0,7)!="sha256:") return false;
        for(char c:digest.text.substr(7)) if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
        out={tag.text,url.text,digest.text.substr(7),bytes};
    }
    return found==1;
}
} // namespace twilight_hd_hud::update_policy
