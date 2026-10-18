#pragma once
// Explicit diagnostic inputs shared by the persistent dispatch checkpoints.
#include "player_update_actor_fixture.hpp"
#include "spatial_query.hpp"
namespace mh4u::test_dispatch_actor_fixture {
namespace na=native_actor;namespace oq=player_object_query;namespace sq=spatial_query;
namespace ad=player_update_admission;
using test_update_actor_fixture::bits;using test_update_actor_fixture::collection;
using test_update_actor_fixture::continuation_seed;using U=std::uint32_t;
inline oq::Collection dispatch_collection(){auto out=collection();out.data[0].field34c=0;return out;}
inline oq::Collection spatial_collection(){auto out=dispatch_collection();out.data[0].field14c=3;return out;}
template<class Check>sq::Scene spatial_scene(const mh4u::collision_resource::Resource& collision,Check&& check){
 sq::Scene scene;
 for(auto& family:scene.direct)family=std::vector<sq::Node>{};
 for(auto& family:scene.other_empty)for(auto& empty:family)empty=true;
 for(auto& family:scene.extra_c8_empty)for(auto& empty:family)empty=true;
 scene.extra_c8_enabled=scene.extra_c9_enabled=scene.extra_c9_empty=true;
 scene.extra_tail_count=scene.status_selector_a=scene.status_selector_b=0;
 scene.classification_masks={0,0,0};scene.normal_thresholds={bits(-2),bits(-2),bits(-2)};
 check(collision.groups().size()==1,"Expected one-group SBC");const auto& group=collision.groups()[0];
 sq::Instance object;object.enabled_5=1;object.bypass_pair_box_33=0;object.object_mask=1;object.area_mask=8;object.exclusion_tag=0;
 object.resource_present=object.grid_present=true;object.tree_present=false;object.resource=collision;
 object.status_count=1;object.grid_state=0;object.status={1,0,0,false,false};scene.instances.push_back(object);
 scene.grid_states.push_back({collision,0,0,0,std::vector<std::optional<U>>(group.pairs.count,0)});
 scene.direct[3]=std::vector<sq::Node>{{0,group.bbox_min[0],group.bbox_max[2]}};return scene;
}
inline na::PlayerInitial action_seed(U action,U substate=0){
 auto x=continuation_seed();auto& p=x.controller.player;auto& f=x.shared_fields;
 p.pbw.field372=std::uint16_t(action);p.pbw.fields5[0]=std::uint8_t(substate);p.selection_pbw.field376=0xa5;
 p.selection_pbw.field6f9=0;p.selection_pbw.field358=bits(500);
 f.pw.field220_bits=f.pw.field838_bits=0;f.pbw.field354_bits=bits(200);
 f.pw.field150=1;f.pw.field14c=0x4000;f.pw.field7f0=0;f.pbw.field478=0;
 x.state1_fields.field50_bits=bits(20);return x;
}
inline na::PlayerInitial spatial_seed(float y){
 auto x=action_seed(2,1);auto& p=x.controller.player;auto& f=x.shared_fields;
 p.pbw.magnitudes[0]=110;p.pbw.field3d4=1;p.pw.fieldf0|=0x4100;p.selection_pbw.field9=3;
 f.pw.field13c_bits=0;f.pbw.field464_bits=0;f.pw.field9dc=f.pw.field9de=f.pw.field9dd=0;
 f.pbw.field10_bits=bits(-6662.23779296875f);f.pbw.field14_bits=bits(y);f.pbw.field18_bits=bits(-1482.9044189453125f);
 return x;
}
inline void preserve_input(ad::Parameters& context){
 auto& f=context.prefix.filter;f.mode2=mh4u::player_input_filter::Absent{};f.manager.ad24=0;f.manager.ad50=0;f.manager.ad64=0;
 f.byte108b9b8=f.byte1083850=f.byte109bd29=0;
}
inline void post_inputs(na::PlayerInitial& initial){
 initial.controller.player.pbw.field39c=bits(3);initial.controller.player.pw.fieldc4=bits(-2);
 initial.controller.player.selection_pbw.field6fa=0xff; // Poison: original begin-update clears it.
 initial.controller.player.pw.policy.table=player_policy_table::Absent{};
 initial.shared_fields.pw.field10_bits=test_update_actor_fixture::one;
 initial.shared_fields.pw.fieldc0_bits=0x7fa23456;
}
inline na::PlayerInitial adjustment_seed(){
 auto initial=action_seed(0xffff,255);post_inputs(initial);
 initial.controller.player.pbw.field371=3;initial.shared_fields.pw.field10_bits.reset();
 initial.controller.motion.fieldd68=bits(6);
 auto& primary=initial.layers[0].animation.layer;primary.interpolation=4;primary.remaining_interpolation=8;
 initial.layers[1].animation.layer.interpolation=std::bit_cast<float>(0x7fa98765U);return initial;
}
} // namespace mh4u::test_dispatch_actor_fixture
