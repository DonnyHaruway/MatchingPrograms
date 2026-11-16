// Recheck EVERY qualifying forbidden graph, rather than one representative.
// Reuses only the previous fast preference/DA model; adds a complete DAG search.
// Stable-path certificates are verified by a separate Python implementation.
#include "fast_da_model.hpp"
#include <cassert>
#include <limits>

struct TreeAnswer {
    bool stable = false;
    unsigned length = 0;       // Stable suffix length, or max depth if all fail.
    uint64_t packed = 0;       // First deletion in the low four bits.
    uint64_t nodes = 0, leaves = 0;
};

struct Solver {
    Problem p;
    std::array<Result,1024> states;
    std::array<uint64_t,1024> state_generation{}, answer_generation{};
    std::array<TreeAnswer,1024> answers;
    uint64_t profile_generation = 0, graph_generation = 0, da_runs = 0;
    int graph = 0;

    const Result &state(int deleted) {
        if (state_generation[deleted] != profile_generation) {
            states[deleted] = da(p, deleted);
            state_generation[deleted] = profile_generation;
            ++da_runs;
        }
        return states[deleted];
    }

    TreeAnswer visit(int deleted) {
        if (answer_generation[deleted] == graph_generation) return answers[deleted];
        const auto &z = state(deleted);
        TreeAnswer out;
        out.nodes = 1;
        bool conflict = false;
        for (int f=0; f<F; ++f) {
            const auto &team = z.teams[f];
            if (team.size()!=2 || !(edge(team[0],team[1]) & graph)) continue;
            conflict = true;
            std::array<int,2> endpoints{team[0],team[1]};
            std::sort(endpoints.begin(),endpoints.end());
            for (int w:endpoints) {
                int contract = f*W+w;
                assert(!(deleted & (1<<contract)));
                auto child = visit(deleted | (1<<contract));
                if (child.stable) {
                    out.stable = true;
                    out.length = child.length+1;
                    out.packed = (child.packed<<4) | unsigned(contract);
                    // An actual stable leaf disproves all-branches failure.
                    answer_generation[deleted] = graph_generation;
                    return answers[deleted] = out;
                }
                out.length = std::max(out.length,child.length+1);
                out.nodes += child.nodes;
                out.leaves += child.leaves;
            }
        }
        if (!conflict) {
            out.leaves = 1;
            out.stable = true;
            for (int f=0;f<F;++f) for(int w=0;w<W;++w)
                if (ur_blocker(p,z,graph,f,w)) out.stable = false;
        }
        answer_generation[deleted] = graph_generation;
        return answers[deleted] = out;
    }

    TreeAnswer tree(int forbidden) {
        graph = forbidden;
        ++graph_generation;
        return visit(0);
    }
};

void little_endian(std::ostream &o,uint64_t value,int bytes) {
    for(int i=0;i<bytes;++i)o.put(char((value>>(8*i))&255));
}

#ifndef UR_RECHECK_MODEL_ONLY
int main(int argc,char **argv) {
    const std::string mode=argc>1?argv[1]:"complete";
    const std::string prefix=argc>2?argv[2]:"recheck";
    int k=0;for(int a=0;a<W;++a)for(int b=a+1;b<W;++b)pair_index[a][b]=pair_index[b][a]=k++;
    const int base=mode=="complete"?2:5;
    int total=1;for(int w=0;w<W;++w)total*=base;
    const std::array<std::array<int,F>,5> types{{{2,1},{1,2},{1,-1},{-1,1},{-1,-1}}};
    std::array<int,W> perm{0,1,2,3,4};
    Solver solver;
    std::ofstream certificates(prefix+"_stable_paths.bin",std::ios::binary);
    std::ofstream failures(prefix+"_all_fail_witnesses.jsonl");
    std::ofstream improved(prefix+"_improving_all_fail.jsonl");
    certificates.exceptions(std::ios::failbit|std::ios::badbit);
    certificates.write("URC1",4);
    uint64_t profiles=0,first_paths=0,second_paths=0,ordinary_paths=0,ancestor_paths=0;
    uint64_t admissible=0,new_combos=0,obstructed_combos=0,exact_combos=0;
    uint64_t unique=0,all_fail=0,all_fail_depth2=0,stable_certificates=0;
    uint64_t all_fail_full=0,all_fail_cross=0,all_fail_focused=0,all_fail_exact=0;
    uint64_t all_fail_exact_depth2=0,all_fail_exact_focused=0;
    uint64_t all_fail_obstructed=0,all_fail_obstructed_depth2=0;
    uint64_t max_tree_depth=0;
    int perm_index=0,best_score=std::numeric_limits<int>::min();
    const auto start=std::chrono::steady_clock::now();
    do {
        for(int code=0;code<total;++code) {
            Problem p;
            for(int w=0;w<W;++w)p.firm[0][w]=W-w;
            for(int rank=0;rank<W;++rank)p.firm[1][perm[rank]]=W-rank;
            int n=code;for(int w=0;w<W;++w){p.worker[w]=types[n%base];n/=base;}
            solver.p=p;++solver.profile_generation;++profiles;
            const unsigned profile_id=perm_index*total+code;
            std::array<bool,1024> checked{};
            std::array<unsigned,1024> flags{};
            std::array<TreeAnswer,1024> trees{};
            const auto &a=solver.state(0);
            for(int f=0;f<F;++f)for(int r:a.teams[f])for(int s:a.teams[f])if(r!=s) {
                ++first_paths;
                const int d1=1<<(f*W+r),e1=edge(r,s);
                const auto &b=solver.state(d1);
                for(int g=0;g<F;++g)for(int t:b.teams[g])for(int u:b.teams[g])if(t!=u) {
                    ++second_paths;
                    const int d2=1<<(g*W+t);
                    const auto &c=solver.state(d1|d2);
                    if(!ordinary_blocker(p,c,f,r))continue;
                    ++ordinary_paths;
                    int required=e1|edge(t,u),invalid=internal_pairs(c);
                    if((required&invalid)||!ur_blocker(p,c,required,f,r))continue;
                    ++ancestor_paths;
                    const auto &restored=solver.state(d2);
                    const bool full=c.teams[0].size()==2&&c.teams[1].size()==2;
                    const bool cross=f!=g,introduced=a.a[t]!=g||a.a[u]!=g;
                    for(int graph=0;graph<1024;++graph) {
                        if((graph&required)!=required||(graph&invalid)||!ur_blocker(p,c,graph,f,r))continue;
                        ++admissible;
                        if(ur_blocker(p,c,graph,g,t)||ur_blocker(p,b,graph,f,r))continue;
                        ++new_combos;
                        const bool obstruction=restored.a[r]!=f||restored.a[s]!=f;
                        const bool exact=restored.a[r]==f&&restored.a[s]!=f
                            &&!(internal_pairs(restored)&graph)&&!ur_blocker(p,restored,graph,g,t);
                        obstructed_combos+=obstruction;exact_combos+=exact;
                        unsigned mask=1;
                        if(full)mask|=2;
                        if(cross)mask|=4;
                        if(full&&cross&&introduced)mask|=8;
                        if(exact)mask|=16;
                        if(exact&&full&&cross&&introduced)mask|=32;
                        if(obstruction)mask|=64;
                        flags[graph]|=mask;
                        if(!checked[graph]) {
                            checked[graph]=true;++unique;
                            trees[graph]=solver.tree(graph);
                            const auto &tree=trees[graph];
                            if(tree.stable) {
                                ++stable_certificates;
                                // 12 bytes: profile u32, graph u16, length u8,
                                // stable deletion path packed into 5 bytes.
                                little_endian(certificates,profile_id,4);
                                little_endian(certificates,graph,2);
                                little_endian(certificates,tree.length,1);
                                little_endian(certificates,tree.packed,5);
                            } else {
                                ++all_fail;
                                max_tree_depth=std::max<uint64_t>(max_tree_depth,tree.length);
                                all_fail_depth2+=tree.length<=2;
                            }
                        }
                        const auto &tree=trees[graph];
                        if(!tree.stable) {
                            if(all_fail<=1000)emit(failures,p,graph,f,r,s,g,t,u,a,b,c,true);
                            int score=100000*(tree.length<=2)+10000*exact+1000*(full&&cross&&introduced)
                                +100*full+10*cross-__builtin_popcount(unsigned(graph))-int(tree.nodes);
                            if(score>best_score){best_score=score;emit(improved,p,graph,f,r,s,g,t,u,a,b,c,true);}
                        }
                    }
                }
            }
            for(int graph=0;graph<1024;++graph)if(checked[graph]&&!trees[graph].stable) {
                auto mask=flags[graph];
                all_fail_full+=bool(mask&2);all_fail_cross+=bool(mask&4);
                all_fail_focused+=bool(mask&8);all_fail_exact+=bool(mask&16);
                all_fail_exact_depth2+=bool(mask&16)&&trees[graph].length<=2;
                all_fail_exact_focused+=bool(mask&32);
                all_fail_obstructed+=bool(mask&64);
                all_fail_obstructed_depth2+=bool(mask&64)&&trees[graph].length<=2;
            }
        }
        ++perm_index;
        if(perm_index%10==0)std::cout<<"permutations="<<perm_index<<" profiles="<<profiles
            <<" graph_inputs="<<unique<<" all_fail="<<all_fail<<std::endl;
    }while(std::next_permutation(perm.begin(),perm.end()));
    double secs=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::ofstream o(prefix+"_summary.json");
    o<<"{\"mode\":\""<<mode<<"\",\"profiles\":"<<profiles
     <<",\"first_paths\":"<<first_paths<<",\"second_paths\":"<<second_paths
     <<",\"ordinary_ancestor_blocker_paths\":"<<ordinary_paths<<",\"ancestor_ur_paths\":"<<ancestor_paths
     <<",\"admissible_graph_path_combinations\":"<<admissible
     <<",\"new_ancestor_graph_path_combinations\":"<<new_combos
     <<",\"obstructed_graph_path_combinations\":"<<obstructed_combos
     <<",\"exact_graph_path_combinations\":"<<exact_combos
     <<",\"unique_qualifying_profile_graph_inputs\":"<<unique
     <<",\"stable_path_certificates\":"<<stable_certificates
     <<",\"all_branches_fail_inputs\":"<<all_fail
     <<",\"all_branches_fail_tree_depth_at_most_two\":"<<all_fail_depth2
     <<",\"all_fail_with_full_target\":"<<all_fail_full
     <<",\"all_fail_with_cross_firm_target\":"<<all_fail_cross
     <<",\"all_fail_with_focused_target\":"<<all_fail_focused
     <<",\"all_fail_with_exact_target\":"<<all_fail_exact
     <<",\"all_fail_exact_tree_depth_at_most_two\":"<<all_fail_exact_depth2
     <<",\"all_fail_with_exact_focused_target\":"<<all_fail_exact_focused
     <<",\"all_fail_with_obstructed_target\":"<<all_fail_obstructed
     <<",\"all_fail_obstructed_tree_depth_at_most_two\":"<<all_fail_obstructed_depth2
     <<",\"max_all_fail_tree_depth\":"<<max_tree_depth
     <<",\"cached_da_runs\":"<<solver.da_runs<<",\"elapsed_seconds\":"<<secs<<"}\n";
    std::cout<<"FINISHED mode="<<mode<<" inputs="<<unique<<" all_fail="<<all_fail<<" seconds="<<secs<<std::endl;
}
#endif
