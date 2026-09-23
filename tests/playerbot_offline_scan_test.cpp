#include <cassert>
#include <memory>
#include <set>
#include "../linux-port/overlays/playerbot/src/game/src/playerbot_offline_policy.h"
struct Shop {
    unsigned owner;
    std::map<unsigned,int> items;
    unsigned GetOwnerPID()const{return owner;}
    const auto& GetItems()const{return items;}
};
int main(){
    auto a=std::make_shared<Shop>(); a->owner=1;
    auto b=std::make_shared<Shop>(); b->owner=2;
    for(unsigned i=1;i<=100;++i)a->items[i]=1;
    for(unsigned i=101;i<=120;++i)b->items[i]=1;
    std::vector<std::pair<int,std::shared_ptr<Shop>>> shops={{0,a},{1,b}};
    playerbot_offline::State state;
    std::set<unsigned> seen;
    auto visit=[&](auto,unsigned id,auto){seen.insert(id);};
    assert(playerbot_offline::BrowseLines(shops,state,64,visit)==64);
    assert(!seen.count(90));
    assert(playerbot_offline::BrowseLines(shops,state,64,visit)==56);
    assert(seen.size()==120 && seen.count(90));
    state.browseOwner=1; state.browseItem=64;
    a->items.erase(64); a->items.erase(65);
    seen.clear(); playerbot_offline::BrowseLines(shops,state,1,visit);
    assert(seen.count(66));
    shops.erase(shops.begin()); // former owner disappeared
    seen.clear(); playerbot_offline::BrowseLines(shops,state,64,visit);
    assert(seen.size()==20);
    shops.clear(); assert(playerbot_offline::BrowseLines(shops,state,64,visit)==0);
}
