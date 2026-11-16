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
int main(int argc,char **argv) {
    std::string mode=argc>1?argv[1]:"complete";
    std::string prefix=argc>2?argv[2]:"search";
    int cap0=argc>3?std::stoi(argv[3]):2,cap1=argc>4?std::stoi(argv[4]):2;
    const bool repeat_only=argc>5&&std::string(argv[5])=="repeat";
    int k=0;for(int a=0;a<W;++a)for(int b=a+1;b<W;++b)pair_index[a][b]=pair_index[b][a]=k++;
    std::ofstream witnesses(prefix+"_witnesses.jsonl");
    std::array<int,W> perm{0,1,2,3,4};
    uint64_t profiles=0,first_paths=0,second_paths=0,ordinary_ancestor=0,broad=0,clean=0,same=0,graphs=0;
    uint64_t newly=0,newly_full=0,newly_full_cross=0,newly_created=0,repeated_worker=0;
    uint64_t obstructed=0,obstructed_full=0,obstructed_repeated=0,obstructed_restored_feasible=0;
    uint64_t exact=0,exact_full=0,exact_cross=0,exact_repeated=0;
    uint64_t unmatched_contract_deletions=0,unmatched_contract_changes=0;
    std::ofstream obstruction_out(prefix+"_obstruction_witnesses.jsonl");
    std::ofstream exact_out(prefix+"_exact_witnesses.jsonl");
    std::ofstream focused_out(prefix+"_focused_witnesses.jsonl");
    int best_score=-1;std::ofstream best_out(prefix+"_improving_witnesses.jsonl");
    int base=mode=="complete"?2:5,total=1;for(int w=0;w<W;++w)total*=base;
    const std::array<std::array<int,F>,5> types{{{2,1},{1,2},{1,-1},{-1,1},{-1,-1}}};
    auto start=std::chrono::steady_clock::now();
    do {
        Problem p;p.cap={cap0,cap1};
        for(int w=0;w<W;++w)p.firm[0][w]=W-w;
        for(int rank=0;rank<W;++rank)p.firm[1][perm[rank]]=W-rank;
        for(int code=0;code<total;++code) {
            int n=code;for(int w=0;w<W;++w){p.worker[w]=types[n%base];n/=base;}
            ++profiles;auto a=da(p,0);
            for(int w=0;w<W;++w)for(int h=0;h<F;++h)if(a.a[w]!=h&&p.worker[w][h]>0){
                ++unmatched_contract_deletions;
                if(da(p,1<<(h*W+w)).a!=a.a)++unmatched_contract_changes;
            }
            for(int f=0;f<F;++f)for(int r:a.teams[f])for(int s:a.teams[f])if(r!=s) {
                ++first_paths;int d1=1<<(f*W+r), e1=edge(r,s);auto b=da(p,d1);
                for(int g=0;g<F;++g)for(int t:b.teams[g])for(int u:b.teams[g])if(t!=u) {
                    if(repeat_only&&(t!=r||g==f))continue;
                    ++second_paths;int d2=1<<(g*W+t);auto c=da(p,d1|d2);
                    if(!ordinary_blocker(p,c,f,r))continue;
                    ++ordinary_ancestor;
                    int required=e1|edge(t,u),invalid=internal_pairs(c);
                    if(required&invalid)continue;
                    if(!ur_blocker(p,c,required,f,r))continue;
                    ++broad;
                    auto restored=da(p,d2);
                    bool found_clean=false,found_new=false,found_exact=false;int best=required,best_count=100;
                    // Enumerate all common symmetric colleague rejection graphs.
                    for(int graph=0;graph<(1<<10);++graph) {
                        if((graph&required)!=required || (graph&invalid))continue;
                        if(!ur_blocker(p,c,graph,f,r))continue;
                        ++graphs;
                        if(ur_blocker(p,c,graph,g,t))continue;
                        bool is_new=!ur_blocker(p,b,graph,f,r);
                        bool is_exact=is_new&&restored.a[r]==f&&restored.a[s]!=f
                            &&!(internal_pairs(restored)&graph)&&!ur_blocker(p,restored,graph,g,t);
                        int bits=__builtin_popcount((unsigned)graph);
                        int priority=100*is_exact+10*is_new,old_priority=100*found_exact+10*found_new;
                        if(priority>old_priority||(priority==old_priority&&bits<best_count)){
                            best_count=bits;best=graph;found_clean=true;found_new=is_new;found_exact=is_exact;
                        }
                    }
                    if(found_clean){++clean;if(e1==edge(t,u))++same;}
                    if(found_new) {
                        ++newly;bool full=(int)c.teams[0].size()==cap0&&(int)c.teams[1].size()==cap1;
                        bool introduced=a.a[t]!=g||a.a[u]!=g;
                        newly_full+=full;newly_full_cross+=full&&f!=g;
                        newly_created+=introduced;repeated_worker+=r==t;
                        bool obstructs=restored.a[r]!=f||restored.a[s]!=f;
                        bool restored_feasible=!(internal_pairs(restored)&best);
                        if(obstructs){++obstructed;obstructed_full+=full;obstructed_repeated+=r==t;
                            obstructed_restored_feasible+=restored_feasible;
                            if(obstructed<=500)emit(obstruction_out,p,best,f,r,s,g,t,u,a,b,c,true);
                        }
                        if(found_exact){++exact;exact_full+=full;exact_cross+=f!=g;exact_repeated+=r==t;
                            if(exact<=1000)emit(exact_out,p,best,f,r,s,g,t,u,a,b,c,true);
                            if(full&&f!=g&&introduced)emit(focused_out,p,best,f,r,s,g,t,u,a,b,c,true);
                        }
                        int score=5000*found_exact+1000*obstructs+100*full+30*(f!=g)+40*introduced+60*(r==t)+20*(e1==edge(t,u))
                            +10*(b.a[s]==f&&c.a[s]!=f)-best_count;
                        if(score>best_score){best_score=score;emit(best_out,p,best,f,r,s,g,t,u,a,b,c,true);}
                    }
                    if(broad<=30 || (found_clean && clean<=200)) emit(witnesses,p,found_clean?best:required,f,r,s,g,t,u,a,b,c,found_clean);
                }
            }
        }
    } while(std::next_permutation(perm.begin(),perm.end()));
    double secs=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::ofstream summary(prefix+"_summary.json");
    summary<<"{\"mode\":\""<<mode<<"\",\"repeat_only\":"<<(repeat_only?"true":"false")<<",\"capacities\":["<<cap0<<','<<cap1<<"],\"profiles\":"<<profiles
        <<",\"first_paths\":"<<first_paths<<",\"second_paths\":"<<second_paths
        <<",\"ordinary_ancestor_blocker_paths\":"<<ordinary_ancestor<<",\"ancestor_ur_paths\":"<<broad
        <<",\"clean_ancestor_ur_paths\":"<<clean<<",\"clean_same_pair_paths\":"<<same
        <<",\"new_ancestor_ur_paths\":"<<newly<<",\"new_full_paths\":"<<newly_full
        <<",\"new_full_cross_firm_paths\":"<<newly_full_cross<<",\"new_introduced_conflict_paths\":"<<newly_created
        <<",\"new_repeated_worker_paths\":"<<repeated_worker
        <<",\"obstructed_parent_pair_paths\":"<<obstructed<<",\"obstructed_full_paths\":"<<obstructed_full
        <<",\"obstructed_repeated_worker_paths\":"<<obstructed_repeated
        <<",\"obstructed_restored_feasible_paths\":"<<obstructed_restored_feasible
        <<",\"exact_restoration_paths\":"<<exact<<",\"exact_full_paths\":"<<exact_full
        <<",\"exact_cross_firm_paths\":"<<exact_cross<<",\"exact_repeated_worker_paths\":"<<exact_repeated
        <<",\"unmatched_contract_deletions\":"<<unmatched_contract_deletions
        <<",\"unmatched_contract_changes\":"<<unmatched_contract_changes
        <<",\"admissible_graph_path_combinations\":"<<graphs<<",\"elapsed_seconds\":"<<secs<<"}\n";
    std::cout<<"mode="<<mode<<" profiles="<<profiles<<" second_paths="<<second_paths<<" broad="<<broad<<" clean="<<clean<<" same="<<same<<" seconds="<<secs<<std::endl;
}
