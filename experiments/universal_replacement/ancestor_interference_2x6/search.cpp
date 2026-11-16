// Exhaustive two-firm, six-worker, capacity-(2,2) recursive doctor DA audit.
// Each stable deletion path certifies its exact family of forbidden graphs.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

constexpr int W=6,F=2,E=W*(W-1)/2,GRAPHS=1<<E,WORDS=GRAPHS/64,STATES=1<<(F*W);
using Family=std::array<uint64_t,WORDS>;
int edge_bit[W][W],inside_mask[1<<W],compatibility_mask[W][1<<W];
std::array<Family,E> has_edge;
std::array<int,GRAPHS> any_index;
std::vector<Family> any_families;
const std::array<std::array<int,F>,5> TYPES{{{2,1},{1,2},{1,-1},{-1,1},{-1,-1}}};

struct Problem {std::array<std::array<int,F>,W> worker;std::array<std::array<int,W>,F> firm;};
struct State {std::array<int8_t,W> assignment;std::array<unsigned,F> team{};};
struct Answer {bool stable=false;unsigned length=0;uint64_t packed=0,nodes=0,leaves=0;};

void initialize() {
    int k=0;for(int a=0;a<W;++a)for(int b=a+1;b<W;++b)edge_bit[a][b]=edge_bit[b][a]=1<<k++;
    for(int team=0;team<(1<<W);++team){
        if(__builtin_popcount(unsigned(team))==2){int a=__builtin_ctz(unsigned(team));int b=__builtin_ctz(unsigned(team^(1<<a)));inside_mask[team]=edge_bit[a][b];}
        for(int w=0;w<W;++w)for(int v=0;v<W;++v)if(v!=w&&(team&(1<<v)))compatibility_mask[w][team]|=edge_bit[w][v];
    }
    for(int bit=0;bit<E;++bit)for(int graph=0;graph<GRAPHS;++graph)
        if(graph&(1<<bit))has_edge[bit][graph/64]|=uint64_t(1)<<(graph%64);
    any_index.fill(-1);
    for(int mask=0;mask<GRAPHS;++mask)if(__builtin_popcount(unsigned(mask))<=2){
        Family family{};
        for(int bits=mask;bits;bits&=bits-1){int bit=__builtin_ctz(unsigned(bits));for(int j=0;j<WORDS;++j)family[j]|=has_edge[bit][j];}
        any_index[mask]=int(any_families.size());any_families.push_back(family);
    }
}
const Family &any_edges(int mask){assert(any_index[mask]>=0);return any_families[any_index[mask]];}

struct Solver {
    Problem p;
    std::array<State,STATES> state_cache;
    std::array<Answer,STATES> answer_cache;
    std::array<uint64_t,STATES> state_epoch{},answer_epoch{};
    uint64_t profile_epoch=0,graph_epoch=0,da_runs=0;
    int graph=0;

    const State &state(int deleted) {
        if(state_epoch[deleted]==profile_epoch)return state_cache[deleted];
        State out;out.assignment.fill(-1);
        std::array<int,W> cursor{};
        int queue[W*(F+1)+1],head=0,tail=0;
        for(int w=0;w<W;++w)queue[tail++]=w;
        while(head<tail){int w=queue[head++];
            while(cursor[w]<F){
                int f=cursor[w]++;if(p.worker[w][0]<p.worker[w][1])f=1-f;
                if(p.worker[w][f]<=0)break;
                if(deleted&(1<<(f*W+w)))continue;
                auto &team=out.team[f];
                if(__builtin_popcount(team)<2){team|=1<<w;out.assignment[w]=int8_t(f);break;}
                int a=__builtin_ctz(team),b=__builtin_ctz(team^(1u<<a));
                int worst=p.firm[f][a]<p.firm[f][b]?a:b;
                if(p.firm[f][w]<=p.firm[f][worst])continue;
                team^=(1u<<worst)|(1u<<w);out.assignment[worst]=-1;out.assignment[w]=int8_t(f);
                assert(tail<int(sizeof(queue)/sizeof(queue[0])));queue[tail++]=worst;break;
            }
        }
        ++da_runs;state_epoch[deleted]=profile_epoch;return state_cache[deleted]=out;
    }
    bool ordinary(const State &z,int f,int w)const {
        if(z.assignment[w]==f)return false;
        int current=z.assignment[w]<0?0:p.worker[w][z.assignment[w]];
        if(p.worker[w][f]<=current)return false;
        if(__builtin_popcount(z.team[f])<2)return true;
        for(unsigned team=z.team[f];team;team&=team-1){int v=__builtin_ctz(team);if(p.firm[f][w]>p.firm[f][v])return true;}
        return false;
    }
    bool ur(const State &z,int forbidden,int f,int w)const {
        return ordinary(z,f,w)&&!(forbidden&compatibility_mask[w][z.team[f]]);
    }
    bool stable(const State &z,int forbidden)const {
        if(forbidden&(inside_mask[z.team[0]]|inside_mask[z.team[1]]))return false;
        for(int f=0;f<F;++f)for(int w=0;w<W;++w)if(ur(z,forbidden,f,w))return false;
        return true;
    }
    Answer visit(int deleted) {
        if(answer_epoch[deleted]==graph_epoch)return answer_cache[deleted];
        const auto &z=state(deleted);Answer out;out.nodes=1;bool conflict=false;
        for(int f=0;f<F;++f)if(graph&inside_mask[z.team[f]]){
            conflict=true;
            for(unsigned team=z.team[f];team;team&=team-1){
                int w=__builtin_ctz(team),contract=f*W+w;assert(!(deleted&(1<<contract)));
                auto child=visit(deleted|(1<<contract));
                if(child.stable){out.stable=true;out.length=child.length+1;out.packed=(child.packed<<4)|unsigned(contract);answer_epoch[deleted]=graph_epoch;return answer_cache[deleted]=out;}
                out.length=std::max(out.length,child.length+1);out.nodes+=child.nodes;out.leaves+=child.leaves;
            }
        }
        if(!conflict){out.leaves=1;out.stable=stable(z,graph);}
        answer_epoch[deleted]=graph_epoch;return answer_cache[deleted]=out;
    }
    Answer tree(int forbidden){graph=forbidden;++graph_epoch;return visit(0);}
    Family family(const Answer &path) {
        int deleted=0,required=0;
        for(unsigned i=0;i<path.length;++i){
            int contract=(path.packed>>(4*i))&15,f=contract/W,w=contract%W;
            const auto &z=state(deleted);assert(z.assignment[w]==f&&__builtin_popcount(z.team[f])==2);
            int other=__builtin_ctz(z.team[f]^(1u<<w));required|=edge_bit[w][other];deleted|=1<<contract;
        }
        const auto &leaf=state(deleted);Family result;result.fill(~uint64_t(0));
        for(int bits=required;bits;bits&=bits-1){const auto &part=has_edge[__builtin_ctz(unsigned(bits))];for(int j=0;j<WORDS;++j)result[j]&=part[j];}
        const auto &invalid=any_edges(inside_mask[leaf.team[0]]|inside_mask[leaf.team[1]]);
        for(int j=0;j<WORDS;++j)result[j]&=~invalid[j];
        for(int f=0;f<F;++f)for(int w=0;w<W;++w)if(ordinary(leaf,f,w)){
            const auto &blocked=any_edges(compatibility_mask[w][leaf.team[f]]);for(int j=0;j<WORDS;++j)result[j]&=blocked[j];
        }
        return result;
    }
};

struct Target {bool exists=false,obstructed=false,exact=false,full=false;int f=-1,r=-1,s=-1,g=-1,t=-1,u=-1;};
Target target(Solver &solver,int graph) {
    Target best;int best_score=-1;const auto &a=solver.state(0);
    for(int f=0;f<F;++f)if(graph&inside_mask[a.team[f]])for(unsigned first=a.team[f];first;first&=first-1){
        int r=__builtin_ctz(first),s=__builtin_ctz(a.team[f]^(1u<<r)),d1=1<<(f*W+r);
        const auto &b=solver.state(d1);
        for(int g=0;g<F;++g)if(graph&inside_mask[b.team[g]])for(unsigned second=b.team[g];second;second&=second-1){
            int t=__builtin_ctz(second),u=__builtin_ctz(b.team[g]^(1u<<t)),d2=1<<(g*W+t);
            const auto &c=solver.state(d1|d2);
            if(graph&(inside_mask[c.team[0]]|inside_mask[c.team[1]]))continue;
            if(!solver.ur(c,graph,f,r)||solver.ur(c,graph,g,t)||solver.ur(b,graph,f,r))continue;
            const auto &restored=solver.state(d2);
            bool obstructed=restored.assignment[r]!=f||restored.assignment[s]!=f;
            bool exact=restored.assignment[r]==f&&restored.assignment[s]!=f&&solver.stable(restored,graph);
            bool full=__builtin_popcount(c.team[0])==2&&__builtin_popcount(c.team[1])==2;
            int score=100*exact+20*obstructed+10*full+(f!=g);
            if(score>best_score){best_score=score;best={true,obstructed,exact,full,f,r,s,g,t,u};}
        }
    }
    return best;
}
void integer(std::ostream &o,uint64_t value,int bytes){for(int i=0;i<bytes;++i)o.put(char((value>>(8*i))&255));}
template<class T>void array(std::ostream&o,const T&row){o<<'[';bool first=true;for(auto v:row){if(!first)o<<',';first=false;o<<int(v);}o<<']';}
void emit(std::ostream &o,const Problem&p,uint64_t profile,int graph,const Answer&tree,const Target&t){
    o<<"{\"profile\":"<<profile<<",\"worker_scores\":[";for(int w=0;w<W;++w){if(w)o<<',';array(o,p.worker[w]);}
    o<<"],\"firm_scores\":[";array(o,p.firm[0]);o<<',';array(o,p.firm[1]);
    o<<"],\"capacities\":[2,2],\"forbidden_mask\":"<<graph<<",\"tree_max_depth\":"<<tree.length
        <<",\"nodes\":"<<tree.nodes<<",\"leaves\":"<<tree.leaves<<",\"interference\":"<<(t.exists?"true":"false")
        <<",\"obstructed\":"<<(t.obstructed?"true":"false")<<",\"exact_restoration\":"<<(t.exact?"true":"false")<<",\"target_full\":"<<(t.full?"true":"false");
    if(t.exists)o<<",\"first\":["<<t.f+1<<','<<t.r+1<<','<<t.s+1<<"],\"second\":["<<t.g+1<<','<<t.t+1<<','<<t.u+1<<']';
    o<<"}\n";
}

int main(int argc,char **argv) {
    if(argc<3){std::cerr<<"Usage: search MODE PREFIX [--begin ID] [--end ID] [--stop-target]\n";return 2;}
    std::string mode=argv[1],prefix=argv[2];assert(mode=="complete"||mode=="incomplete");
    const int base=mode=="complete"?2:5;int total=1;for(int w=0;w<W;++w)total*=base;
    uint64_t begin=0,end=uint64_t(720)*total;bool stop_target=false;
    for(int i=3;i<argc;++i){std::string flag=argv[i];if(flag=="--stop-target")stop_target=true;else if(flag=="--begin"&&i+1<argc)begin=std::stoull(argv[++i]);else if(flag=="--end"&&i+1<argc)end=std::stoull(argv[++i]);else return 2;}
    initialize();Solver solver;
    std::ofstream certificates(prefix+"_graph_cover.bin",std::ios::binary),failures(prefix+"_failures.jsonl"),improved(prefix+"_improving_failures.jsonl");
    certificates.exceptions(std::ios::failbit|std::ios::badbit);certificates.write("URG2",4);integer(certificates,W,1);integer(certificates,base,1);
    std::array<int,W> permutation{0,1,2,3,4,5};uint64_t perm_index=0;
    uint64_t profiles=0,inputs=0,all_fail=0,depth2_fail=0,interference_fail=0,depth2_interference_fail=0,exact_fail=0,depth2_exact_fail=0,certificate_paths=0,searches=0;
    int best_score=std::numeric_limits<int>::min();bool stopped=false;
    const auto started=std::chrono::steady_clock::now();
    do {
        for(int code=0;code<total;++code){
            uint64_t id=perm_index*total+code;if(id<begin||id>=end)continue;
            Problem p;for(int w=0;w<W;++w)p.firm[0][w]=W-w;
            for(int rank=0;rank<W;++rank)p.firm[1][permutation[rank]]=W-rank;
            int n=code;for(int w=0;w<W;++w){p.worker[w]=TYPES[n%base];n/=base;}
            solver.p=p;++solver.profile_epoch;++profiles;inputs+=GRAPHS;
            std::vector<Answer> paths(1);paths[0].stable=true;
            std::vector<int> bad;auto covered=solver.family(paths[0]);
            for(int word=0;word<WORDS;++word){
                while(covered[word]!=~uint64_t(0)){
                    int graph=word*64+__builtin_ctzll(~covered[word]);++searches;
                    auto tree=solver.tree(graph);
                    if(tree.stable){
                        auto family=solver.family(tree);assert(family[word]&(uint64_t(1)<<(graph%64)));
                        for(int j=0;j<WORDS;++j)covered[j]|=family[j];paths.push_back(tree);
                    }else{
                        ++all_fail;depth2_fail+=tree.length<=2;auto t=target(solver,graph);
                        interference_fail+=t.exists;depth2_interference_fail+=t.exists&&tree.length<=2;
                        exact_fail+=t.exact;depth2_exact_fail+=t.exact&&tree.length<=2;
                        if(all_fail<=1000)emit(failures,p,id,graph,tree,t);
                        int score=100000*(tree.length<=2)+10000*t.exact+1000*t.obstructed+100*t.exists+10*t.full-int(tree.nodes)-__builtin_popcount(unsigned(graph));
                        if(score>best_score){best_score=score;emit(improved,p,id,graph,tree,t);std::ofstream best(prefix+"_best_failure.json");emit(best,p,id,graph,tree,t);}
                        bad.push_back(graph);covered[word]|=uint64_t(1)<<(graph%64);
                        if(stop_target&&tree.length<=2&&t.exact&&t.full){stopped=true;break;}
                    }
                }
                if(stopped)break;
            }
            if(!stopped){
                certificate_paths+=paths.size();integer(certificates,id,4);integer(certificates,paths.size(),2);integer(certificates,bad.size(),2);
                for(const auto &path:paths){integer(certificates,path.length,1);integer(certificates,path.packed,6);}for(int graph:bad)integer(certificates,graph,2);
            }
            if(stopped)break;
        }
        ++perm_index;
        if(perm_index%10==0)std::cout<<"permutations="<<perm_index<<" profiles="<<profiles<<" inputs="<<inputs<<" all_fail="<<all_fail<<" depth2_target="<<depth2_exact_fail<<std::endl;
        if(stopped||perm_index*total>=end)break;
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::ofstream o(prefix+"_summary.json");
    o<<"{\"workers\":6,\"mode\":\""<<mode<<"\",\"begin_profile\":"<<begin<<",\"end_profile\":"<<end
        <<",\"completed_requested_range\":"<<(stopped?"false":"true")<<",\"stopped_on_target\":"<<(stopped?"true":"false")
        <<",\"preference_profiles\":"<<profiles<<",\"profile_graph_inputs\":"<<inputs<<",\"all_branches_fail_inputs\":"<<all_fail
        <<",\"all_fail_tree_depth_at_most_two\":"<<depth2_fail<<",\"all_fail_with_two_step_interference\":"<<interference_fail
        <<",\"depth_two_all_fail_with_interference\":"<<depth2_interference_fail<<",\"all_fail_with_exact_restoration\":"<<exact_fail
        <<",\"depth_two_all_fail_with_exact_restoration\":"<<depth2_exact_fail<<",\"stable_path_certificates\":"<<certificate_paths
        <<",\"explicit_tree_searches\":"<<searches<<",\"cached_da_runs\":"<<solver.da_runs<<",\"elapsed_seconds\":"<<elapsed<<"}\n";
    std::cout<<"FINISHED mode="<<mode<<" profiles="<<profiles<<" all_fail="<<all_fail<<" stopped="<<stopped<<" seconds="<<elapsed<<std::endl;
}
