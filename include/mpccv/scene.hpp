#pragma once
#include "graph.hpp"
#include <cstring>
#include <limits>

namespace mpccv {
// Canonical LE binary scene, fixed ceiling; decode is control-plane only.
// Header: MCVS, u32 payload length, u32 FNV-1a corruption checksum (not authentication).
struct SceneBytes { std::array<uint8_t,4096> bytes{}; size_t size=0; };
inline uint32_t checksum(const uint8_t* p,size_t n) noexcept {
    uint32_t h=2166136261u; for(size_t i=0;i<n;++i) {h^=p[i];h*=16777619u;} return h;
}
inline bool save_scene(const Scene& s,SceneBytes& result) noexcept {
    Plan p; if(compile(s,p)!=GraphError::none) return false;
    SceneBytes out; out.size=12;
    auto put=[&](uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.bytes[out.size++]=uint8_t(v>>(i*8));};
    auto real=[&](double v){uint64_t bits;std::memcpy(&bits,&v,8);put(bits,8);};
    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,"IEEE64 required");
    put(s.version,4);put(s.count,1);put(s.enabled,1);put(s.sync?1:0,1);
    for(auto c:s.controls)put(c,1);
    for(unsigned i=0;i<s.count;++i) {
        const auto& n=s.nodes[i];put(unsigned(n.kind),1);put(n.input,1);put(n.modulator,1);put(n.seed,4);
        for(double v:{n.rate,n.scale,n.offset,n.amount,n.slew_per_second,n.quantum})real(v);
        for(auto v:n.steps)real(v);
    }
    for(const auto& r:s.routes) {put(r.source,1);for(double v:{r.scale,r.offset,r.low,r.high})real(v);}
    out.bytes[0]='M';out.bytes[1]='C';out.bytes[2]='V';out.bytes[3]='S';
    auto len=uint32_t(out.size-12),hash=checksum(out.bytes.data()+12,len);
    for(unsigned i=0;i<4;++i) {out.bytes[4+i]=uint8_t(len>>(i*8));out.bytes[8+i]=uint8_t(hash>>(i*8));}
    result=out;return true;
}
inline bool load_scene(const uint8_t* data,size_t size,Scene& result) noexcept {
    if(!data || size<23 || size>4096 || std::memcmp(data,"MCVS",4))return false;
    size_t cursor=4;bool ok=true;
    auto get=[&](unsigned n)->uint64_t {
        if(cursor+n>size) {ok=false;return 0;}
        uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(data[cursor++])<<(i*8);return v;
    };
    auto len=get(4),hash=get(4);
    if(len!=size-12 || hash!=checksum(data+12,size-12))return false;
    auto real=[&](){uint64_t bits=get(8);double v;std::memcpy(&v,&bits,8);return v;};
    Scene s;s.version=uint32_t(get(4));s.count=uint8_t(get(1));s.enabled=uint8_t(get(1));
    auto sync=get(1);if(sync>1)return false;s.sync=sync!=0;
    for(auto& c:s.controls)c=uint8_t(get(1));
    if(s.count>max_nodes)return false;
    for(unsigned i=0;i<s.count;++i) {
        auto& n=s.nodes[i];n.kind=static_cast<Kind>(get(1));n.input=uint8_t(get(1));n.modulator=uint8_t(get(1));n.seed=uint32_t(get(4));
        n.rate=real();n.scale=real();n.offset=real();n.amount=real();n.slew_per_second=real();n.quantum=real();
        for(auto& v:n.steps)v=real();
    }
    for(auto& r:s.routes) {r.source=uint8_t(get(1));r.scale=real();r.offset=real();r.low=real();r.high=real();}
    Plan p;if(!ok || cursor!=size || compile(s,p)!=GraphError::none)return false;
    result=s;return true;
}
}
