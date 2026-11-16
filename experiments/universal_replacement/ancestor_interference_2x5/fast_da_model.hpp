#pragma once
// Extracted from the previously independently checked search.cpp model.
// Exhaustive normalized strict-preference profiles, independent of repository DA.
// Compile: c++ -std=c++17 -O3 search.cpp -o /tmp/ancestor_search
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

constexpr int W = 5, F = 2;
using Assignment = std::array<int, W>; // -1 = unmatched
struct Problem {
    std::array<std::array<int,F>,W> worker{};
    std::array<std::array<int,W>,F> firm{};
    std::array<int,F> cap{2,2};
};
struct Result { Assignment a; std::array<std::vector<int>,F> teams; };
int pair_index[W][W];
int edge(int a,int b) { return 1 << pair_index[a][b]; }
Result da(const Problem &p, int deleted) {
    Result out; out.a.fill(-1);
    std::array<int,W> next{};
    int queue[W * (F+1) + 1],head=0,tail=0;
    for(int w=0;w<W;++w) queue[tail++]=w;
    while(head<tail) {
        int w=queue[head++];
        while(next[w]<F) {
            int f=next[w]++;
            // next is a preference-order cursor, rather than a firm ID.
            if(p.worker[w][0] < p.worker[w][1]) f=1-f;
            if(p.worker[w][f]<=0) break;
            if(deleted & (1 << (f*W+w))) continue;
            auto &t=out.teams[f];
            if((int)t.size()<p.cap[f]) {
                t.push_back(w);out.a[w]=f;break;
            }
            if(t.empty()) continue;
            auto worst=std::min_element(t.begin(),t.end(),[&](int a,int b){return p.firm[f][a]<p.firm[f][b];});
            if(p.firm[f][w] <= p.firm[f][*worst]) continue;
            int kicked=*worst; *worst=w; out.a[kicked]=-1; out.a[w]=f;
            queue[tail++]=kicked;break;
        }
    }
    return out;
}
int internal_pairs(const Result &r) {
    int mask=0;
    for(const auto &t:r.teams) for(int a:t) for(int b:t) if(a<b) mask|=edge(a,b);
    return mask;
}
bool ordinary_blocker(const Problem &p,const Result &r,int f,int w) {
    if(r.a[w]==f) return false;
    int current=r.a[w]<0?0:p.worker[w][r.a[w]];
    if(p.worker[w][f]<=current) return false;
    if((int)r.teams[f].size()<p.cap[f]) return true;
    for(int v:r.teams[f]) if(p.firm[f][w]>p.firm[f][v]) return true;
    return false;
}
bool ur_blocker(const Problem &p,const Result &r,int forbidden,int f,int w) {
    if(!ordinary_blocker(p,r,f,w)) return false;
    const auto &t=r.teams[f];
    if((int)t.size()<p.cap[f]) {
        for(int v:t) if(forbidden&edge(v,w)) return false;
        return true;
    }
    for(int removed:t) {
        for(int v:t) if(v!=removed && (forbidden&edge(v,w))) return false;
        for(int v:t)for(int z:t)if(v<z&&v!=removed&&z!=removed&&(forbidden&edge(v,z)))return false;
    }
    return true;
}
template<class T> void json_array(std::ostream &o,const T &v,int add=0) {
    o<<'[';bool first=true;for(auto x:v){if(!first)o<<',';first=false;o<<x+add;}o<<']';
}
void emit(std::ostream &o,const Problem &p,int graph,int f,int r,int s,int g,int t,int u,const Result &a,const Result &b,const Result &c,bool clean) {
    o<<"{\"worker_scores\":[";for(int w=0;w<W;++w){if(w)o<<',';json_array(o,p.worker[w]);}
    o<<"],\"firm_scores\":[";for(int h=0;h<F;++h){if(h)o<<',';json_array(o,p.firm[h]);}
    o<<"],\"capacities\":";json_array(o,p.cap);
    o<<",\"forbidden_mask\":"<<graph<<",\"first\":["<<f+1<<','<<r+1<<','<<s+1<<"],\"second\":["<<g+1<<','<<t+1<<','<<u+1<<"],\"assignments\":[";
    json_array(o,a.a,1);o<<',';json_array(o,b.a,1);o<<',';json_array(o,c.a,1);
    o<<"],\"last_not_ur_blocker\":"<<(clean?"true":"false")<<"}\n";
}
