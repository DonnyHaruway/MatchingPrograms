// Serialize the unmodified repository algorithms for arbitrary input matrices.
// This experiment-local bridge avoids changing historical profiles or core code.
#include "algorithms.hpp"
#include <fstream>
#include <iostream>
#include <string>

template<class Values> void array(std::ostream &o,const Values &v,int offset=0) {
    o<<'[';bool first=true;for(auto x:v){if(!first)o<<',';first=false;o<<x+offset;}o<<']';
}
void write(const std::string &file,const DASearchTree &tree) {
    std::ofstream o(file);o.exceptions(std::ios::badbit|std::ios::failbit);
    o<<"{\"schema_version\":1,\"algorithm\":\"recursive_doctor\",\"mode\":\"recursive\",\"proposing_side\":\"doctor\",\"seed\":null,\"nodes\":[";
    for(size_t id=0;id<tree.nodes.size();++id){
        if(id)o<<',';const auto &n=tree.nodes[id];
        o<<"{\"id\":"<<id<<",\"da_run\":"<<n.deletions.size()+1<<",\"assignment\":";
        array(o,n.matching.get_agent_matchs(),1);o<<",\"conflicts\":[";
        for(size_t i=0;i<n.conflicts.size();++i){if(i)o<<',';auto p=n.conflicts[i];o<<"{\"firm\":"<<p.firm+1<<",\"left\":"<<p.left+1<<",\"right\":"<<p.right+1<<'}';}
        o<<"],\"deletions\":[";
        for(size_t i=0;i<n.deletions.size();++i){if(i)o<<',';auto d=n.deletions[i];o<<"{\"firm\":"<<d.firm+1<<",\"worker\":"<<d.worker+1<<'}';}
        o<<"],\"original_ur_stable\":"<<(n.original_ur_stable?"true":"false")<<",\"reduced_ur_stable\":"<<(n.reduced_ur_stable?"true":"false")<<",\"original_blocking_pairs\":[";
        for(size_t i=0;i<n.original_blocking_pairs.size();++i){if(i)o<<',';auto [w,f]=n.original_blocking_pairs[i];o<<"{\"firm\":"<<f+1<<",\"worker\":"<<w+1<<'}';}
        o<<"]}";
    }
    o<<"],\"branches\":[";
    for(size_t id=0;id<tree.branches.size();++id){
        if(id)o<<',';const auto &b=tree.branches[id];const auto &n=tree.nodes[b.from];const auto &p=n.conflicts[b.conflict_index];
        int kept=b.removed_worker==p.left?p.right:p.left;const auto &a=tree.nodes[*b.to].matching.get_agent_matchs();
        o<<"{\"id\":"<<id<<",\"from\":"<<b.from<<",\"to\":"<<*b.to<<",\"conflict_index\":"<<b.conflict_index
         <<",\"firm\":"<<p.firm+1<<",\"removed_worker\":"<<b.removed_worker+1<<",\"retained_worker\":"<<kept+1
         <<",\"removed_before\":"<<p.firm+1<<",\"retained_before\":"<<p.firm+1<<",\"removed_after\":"<<a[b.removed_worker]+1
         <<",\"retained_after\":"<<a[kept]+1<<",\"retained_moved\":"<<(a[kept]!=p.firm?"true":"false")<<'}';
    }
    o<<"],\"leaves\":";array(o,tree.leaves);o<<"}\n";
}
int main(int argc,char **argv) {
    if(argc!=2){std::cerr<<"Usage: repository_bridge OUTPUT_JSON < problem.txt\n";return 2;}
    int w,f;if(!(std::cin>>w>>f)||w<1||f<1)return 2;
    std::vector<int> cap(f);for(int &v:cap)std::cin>>v;
    std::vector<std::vector<int>> wp(w,std::vector<int>(f)),fp(f,std::vector<int>(w)),cp(w,std::vector<int>(w));
    for(auto &m:{&wp,&fp,&cp})for(auto &row:*m)for(int &v:row)std::cin>>v;
    if(!std::cin){std::cerr<<"Invalid input\n";return 2;}
    write(argv[1],recursive_doctor_proposing_DA_algorithm(w,f,cap,wp,fp,cp));
}
