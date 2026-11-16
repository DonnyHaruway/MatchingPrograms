// Independent verifier: simultaneous proposal rounds, explicit team arrays,
// and std::bitset graph families. Does not include the search implementation.
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

constexpr int N=6,G=1<<15,S=1<<12;
using Graphs=std::bitset<G>;
struct Matching {std::array<int,N> assignment;std::array<std::array<int,2>,2> team;std::array<int,2> size{};};
struct Model {
    int worker[N][2],employer[2][N],order[N][2],order_size[N];
    std::array<Matching,S> cache;
    std::array<uint64_t,S> epoch{};
    uint64_t generation=0,da_runs=0;
    void prepare(){++generation;for(int w=0;w<N;++w){order_size[w]=0;for(int h=0;h<2;++h)if(worker[w][h]>0)order[w][order_size[w]++]=h;
        if(order_size[w]==2&&worker[w][order[w][0]]<worker[w][order[w][1]])std::swap(order[w][0],order[w][1]);}}
    const Matching &run(int removed){
        if(epoch[removed]==generation)return cache[removed];
        Matching z;z.assignment.fill(-1);int cursor[N]{};
        while(true){
            int candidates[2][8],counts[2]{};bool active=false;
            for(int h=0;h<2;++h)for(int i=0;i<z.size[h];++i)candidates[h][counts[h]++]=z.team[h][i];
            for(int w=0;w<N;++w)if(z.assignment[w]==-1&&cursor[w]<order_size[w]){
                active=true;int h=order[w][cursor[w]++];if(!(removed&(1<<(h*N+w))))candidates[h][counts[h]++]=w;
            }
            if(!active)break;
            for(int h=0;h<2;++h){
                std::sort(candidates[h],candidates[h]+counts[h],[&](int a,int b){return employer[h][a]>employer[h][b];});
                for(int i=0;i<counts[h];++i)z.assignment[candidates[h][i]]=-1;
                z.size[h]=std::min(2,counts[h]);
                for(int i=0;i<z.size[h];++i){z.team[h][i]=candidates[h][i];z.assignment[candidates[h][i]]=h;}
            }
        }
        ++da_runs;epoch[removed]=generation;return cache[removed]=z;
    }
};

uint64_t read_integer(std::istream &in,int bytes){uint64_t n=0;for(int i=0;i<bytes;++i){int c=in.get();assert(c!=EOF);n|=uint64_t(unsigned(c))<<(8*i);}return n;}

int main(int argc,char **argv){
    if(argc!=3){std::cerr<<"Usage: verify_certificates CERTIFICATE OUTPUT_JSON\n";return 2;}
    std::ifstream in(argv[1],std::ios::binary);char magic[4];in.read(magic,4);assert(std::string(magic,4)=="URG2");
    assert(read_integer(in,1)==6);int base=int(read_integer(in,1));assert(base==2||base==5);
    int pair[6][6],bit=0;for(int a=0;a<N;++a)for(int b=a+1;b<N;++b)pair[a][b]=pair[b][a]=bit++;
    std::array<Graphs,15> has;
    for(int e=0;e<15;++e)for(int graph=0;graph<G;++graph)if(graph&(1<<e))has[e].set(graph);
    const int types[5][2]={{2,1},{1,2},{1,-1},{-1,1},{-1,-1}};
    int total=1;for(int w=0;w<N;++w)total*=base;
    uint64_t profiles=0,paths=0,covered_inputs=0;int permutation_id=0;
    std::array<int,N> permutation{0,1,2,3,4,5};Model model;
    const auto started=std::chrono::steady_clock::now();
    do {
        for(int code=0;code<total;++code){
            uint64_t expected=uint64_t(permutation_id)*total+code;
            assert(read_integer(in,4)==expected);int path_count=int(read_integer(in,2));assert(read_integer(in,2)==0);
            int n=code;for(int w=0;w<N;++w){int t=n%base;n/=base;model.worker[w][0]=types[t][0];model.worker[w][1]=types[t][1];model.employer[0][w]=N-w;}
            for(int rank=0;rank<N;++rank)model.employer[1][permutation[rank]]=N-rank;
            model.prepare();Graphs coverage;
            for(int p=0;p<path_count;++p){
                int length=int(read_integer(in,1));uint64_t packed=read_integer(in,6);assert(length<=12&&packed>>(4*length)==0);
                int removed=0;Graphs family;family.set();
                for(int i=0;i<length;++i){
                    int contract=(packed>>(4*i))&15;assert(contract<12&&!(removed&(1<<contract)));
                    int h=contract/N,w=contract%N;const auto &z=model.run(removed);
                    assert(z.assignment[w]==h&&z.size[h]==2);
                    int other=z.team[h][0]==w?z.team[h][1]:z.team[h][0];family&=has[pair[w][other]];removed|=1<<contract;
                }
                const auto &leaf=model.run(removed);
                for(int h=0;h<2;++h)if(leaf.size[h]==2)family&=~has[pair[leaf.team[h][0]][leaf.team[h][1]]];
                for(int h=0;h<2;++h)for(int w=0;w<N;++w){
                    if(leaf.assignment[w]==h)continue;
                    int current=leaf.assignment[w]<0?0:model.worker[w][leaf.assignment[w]];
                    if(model.worker[w][h]<=current)continue;
                    bool improves=leaf.size[h]<2;
                    for(int j=0;j<leaf.size[h];++j)if(model.employer[h][w]>model.employer[h][leaf.team[h][j]])improves=true;
                    if(!improves)continue;
                    Graphs blocked;for(int j=0;j<leaf.size[h];++j)blocked|=has[pair[w][leaf.team[h][j]]];family&=blocked;
                }
                assert(family.any());coverage|=family;
            }
            assert(coverage.all());++profiles;paths+=path_count;covered_inputs+=G;
        }
        ++permutation_id;
        if(permutation_id%30==0)std::cout<<"verified_permutations="<<permutation_id<<" profiles="<<profiles<<" inputs="<<covered_inputs<<std::endl;
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    assert(in.get()==EOF);
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::ofstream out(argv[2]);out<<"{\"workers\":6,\"base\":"<<base<<",\"preference_profiles_verified\":"<<profiles
        <<",\"profile_graph_inputs_verified\":"<<covered_inputs<<",\"stable_deletion_paths_verified\":"<<paths
        <<",\"all_32768_graphs_covered_for_every_profile\":true,\"all_branches_fail_inputs\":0,\"independent_batch_da_runs\":"<<model.da_runs
        <<",\"elapsed_seconds\":"<<seconds<<"}\n";
    std::cout<<"VERIFIED profiles="<<profiles<<" inputs="<<covered_inputs<<" seconds="<<seconds<<std::endl;
}
