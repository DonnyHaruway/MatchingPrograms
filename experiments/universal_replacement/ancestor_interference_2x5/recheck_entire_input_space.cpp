// Stronger audit: all 1024 forbidden graphs for EVERY preference profile,
// without requiring a two-step interference path. A short collection of
// stable deletion paths certifies whole families of forbidden graphs.
#define UR_RECHECK_MODEL_ONLY
#include "recheck_all_graphs.cpp"
#undef UR_RECHECK_MODEL_ONLY
#include <bitset>

using GraphFamily = std::bitset<1024>;
std::array<GraphFamily,1024> any_edges, every_edge;

GraphFamily path_family(Solver &solver,const TreeAnswer &path) {
    int deleted=0,required=0;
    for(unsigned i=0;i<path.length;++i) {
        int contract=(path.packed>>(4*i))&15,f=contract/W,w=contract%W;
        const auto &z=solver.state(deleted);
        assert(z.a[w]==f&&z.teams[f].size()==2);
        int other=z.teams[f][0]==w?z.teams[f][1]:z.teams[f][0];
        required|=edge(w,other);
        deleted|=1<<contract;
    }
    const auto &leaf=solver.state(deleted);
    auto family=every_edge[required]&~any_edges[internal_pairs(leaf)];
    for(int f=0;f<F;++f)for(int w=0;w<W;++w)if(ordinary_blocker(solver.p,leaf,f,w)) {
        int mask=0;for(int v:leaf.teams[f])mask|=edge(v,w);
        family&=any_edges[mask];
    }
    return family;
}

int main(int argc,char **argv) {
    const std::string mode=argc>1?argv[1]:"complete";
    const std::string prefix=argc>2?argv[2]:"global_recheck";
    int k=0;for(int a=0;a<W;++a)for(int b=a+1;b<W;++b)pair_index[a][b]=pair_index[b][a]=k++;
    std::array<GraphFamily,10> has;
    for(int graph=0;graph<1024;++graph)for(int bit=0;bit<10;++bit)if(graph&(1<<bit))has[bit].set(graph);
    every_edge[0].set();
    for(int mask=1;mask<1024;++mask){int bit=__builtin_ctz(unsigned(mask)),rest=mask^(1<<bit);
        any_edges[mask]=any_edges[rest]|has[bit];every_edge[mask]=every_edge[rest]&has[bit];}
    const int base=mode=="complete"?2:5;
    int total=1;for(int w=0;w<W;++w)total*=base;
    const std::array<std::array<int,F>,5> types{{{2,1},{1,2},{1,-1},{-1,1},{-1,-1}}};
    std::array<int,W> perm{0,1,2,3,4};
    Solver solver;
    std::ofstream certificates(prefix+"_graph_cover.bin",std::ios::binary);
    std::ofstream failures(prefix+"_failures.jsonl");
    certificates.exceptions(std::ios::failbit|std::ios::badbit);
    certificates.write("URG1",4);
    uint64_t profiles=0,inputs=0,all_fail=0,all_fail_depth2=0,certificate_paths=0,searches=0;
    int perm_index=0;
    const auto start=std::chrono::steady_clock::now();
    do {
        for(int code=0;code<total;++code) {
            Problem p;
            for(int w=0;w<W;++w)p.firm[0][w]=W-w;
            for(int rank=0;rank<W;++rank)p.firm[1][perm[rank]]=W-rank;
            int n=code;for(int w=0;w<W;++w){p.worker[w]=types[n%base];n/=base;}
            solver.p=p;++solver.profile_generation;++profiles;inputs+=1024;
            std::vector<TreeAnswer> paths;
            std::vector<int> bad_graphs;
            TreeAnswer original;original.stable=true;
            paths.push_back(original);
            auto covered=path_family(solver,original);
            for(int graph=0;graph<1024;++graph)if(!covered[graph]) {
                ++searches;auto tree=solver.tree(graph);
                if(tree.stable) {
                    auto family=path_family(solver,tree);assert(family[graph]);
                    covered|=family;paths.push_back(tree);
                } else {
                    ++all_fail;all_fail_depth2+=tree.length<=2;
                    bad_graphs.push_back(graph);covered.set(graph);
                    if(all_fail<=1000) {
                        failures<<"{\"profile\":"<<perm_index*total+code<<",\"forbidden_mask\":"<<graph
                            <<",\"max_depth\":"<<tree.length<<",\"nodes\":"<<tree.nodes<<",\"leaves\":"<<tree.leaves<<"}\n";
                    }
                }
            }
            assert(covered.all());certificate_paths+=paths.size();
            little_endian(certificates,perm_index*total+code,4);
            little_endian(certificates,paths.size(),2);little_endian(certificates,bad_graphs.size(),2);
            for(const auto &path:paths){little_endian(certificates,path.length,1);little_endian(certificates,path.packed,5);}
            for(int graph:bad_graphs)little_endian(certificates,graph,2);
        }
        ++perm_index;
        if(perm_index%10==0)std::cout<<"permutations="<<perm_index<<" profiles="<<profiles
            <<" covered_inputs="<<inputs<<" all_fail="<<all_fail<<std::endl;
    }while(std::next_permutation(perm.begin(),perm.end()));
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::ofstream o(prefix+"_summary.json");
    o<<"{\"mode\":\""<<mode<<"\",\"preference_profiles\":"<<profiles<<",\"all_profile_graph_inputs\":"<<inputs
        <<",\"all_branches_fail_inputs\":"<<all_fail<<",\"all_fail_tree_depth_at_most_two\":"<<all_fail_depth2
        <<",\"stable_path_certificates\":"<<certificate_paths<<",\"explicit_tree_searches\":"<<searches
        <<",\"cached_da_runs\":"<<solver.da_runs<<",\"elapsed_seconds\":"<<seconds<<"}\n";
    std::cout<<"FINISHED mode="<<mode<<" inputs="<<inputs<<" all_fail="<<all_fail<<" seconds="<<seconds<<std::endl;
}
