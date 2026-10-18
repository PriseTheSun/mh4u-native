#include "player_position_bridge.hpp"
#include "test_support/model_animation_frame_fixture.hpp"
#include <set>
namespace {
using namespace mh4u::test_support::model_frame;
namespace bridge=mh4u::player_position_bridge;
namespace mo=mh4u::model_animation_output;
namespace jo=mh4u::joint_output;
namespace pa=mh4u::player_animation;
namespace pcs=mh4u::player_controller_step;
namespace pf=mh4u::player_fields;
using Arenas=std::map<std::string,Bytes>;
void same(const Bytes& a,const Bytes& b,const std::string& label){
    check(a.size()==b.size(),label+" extent");if(a!=b){auto at=std::mismatch(a.begin(),a.end(),b.begin());std::ostringstream s;s<<label<<" byte+0x"<<std::hex<<std::distance(a.begin(),at.first)<<" got "<<unsigned(*at.first)<<" expected "<<unsigned(*at.second);throw std::runtime_error(s.str());}++checks;
}
void same(const Arenas& a,const Arenas& b){check(a.size()==b.size(),"Arena count");for(const auto& [name,bytes]:a)same(bytes,b.at(name),name);}
Raw motion_raw(const Arenas& a,unsigned layers){Raw out{a.at("model"),a.at("joints"),a.at("snapshots"),{}};for(unsigned i=0;i<layers;++i)out.channels.push_back(a.at("channels_"+std::to_string(i)));return out;}
void put_motion(Arenas& a,const Raw& raw){a.at("model")=raw.model;a.at("joints")=raw.joints;a.at("snapshots")=raw.snapshots;for(unsigned i=0;i<raw.channels.size();++i)a.at("channels_"+std::to_string(i))=raw.channels[i];}
struct State {frame::State frame;mo::Hierarchy hierarchy;pcs::State controller;pf::Fields shared;bridge::Fields fields;};
State decode(const Arenas& a,const Context& context,unsigned layers){
    State out;out.frame=frame_state(motion_raw(a,layers),context);const auto& m=a.at("model");const auto& p=a.at("pbw");auto& h=out.hierarchy;
    h.local_rt=words<16>(m,0x70);h.world=words<16>(m,0xb0);h.flags4=word(m,4);h.flags104=word(m,0x104);h.flags230=word(m,0x230);h.attachment_index=word(m,0x34);h.attachment_mode_high16=static_cast<std::uint16_t>(word(m,0x3a,2));h.dirty=static_cast<std::uint8_t>(word(m,0xc49,1));h.fieldc4c=word(m,0xc4c);h.order=a.at("order");
    for(std::size_t i=0;i<out.frame.animation.joints.size();++i){const auto pos=i*168;const auto& j=a.at("joints");const auto owner=word(j,pos+0xa0);h.joints.push_back({{word(j,pos+0x94),word(j,pos+0x98)},words<16>(j,pos+8),word(j,pos+0x90),word(j,pos+0x88)?jo::Callback::unsupported:jo::Callback::null,owner==0x2000000?jo::Owner::self:owner?jo::Owner::other:jo::Owner::null});}
    auto& c=out.controller;c.player.fieldsd10=words<3>(m,0xd10);c.player.pbw.field20=word(p,0x20);c.player.pbw.field24=word(p,0x24);c.player.pbw.field28=word(p,0x28);c.motion.fieldsd48=words<7>(m,0xd48);c.motion.fieldd68=word(m,0xd68);c.motion.fieldsd70=words<4>(m,0xd70);
    // Unrelated canonical fields are typed sentinels, not facts assigned to
    // unobserved source memory. The bridge must preserve them exactly.
    c.player.pw.fieldb0=0xdead1234;c.player.global.field5d=0x5a;c.player.selection_pw.field7fc=0x12345678;
    out.shared.pbw.field10_bits=word(p,0x10);out.shared.pbw.field14_bits=word(p,0x14);out.shared.pbw.field18_bits=word(p,0x18);out.shared.pbw.field1c_bits=word(p,0x1c);out.shared.pw.field154=0xcafe1234;
    out.fields={static_cast<std::uint8_t>(word(m,0xd2c,1)),static_cast<std::uint8_t>(word(m,0xd44,1)),word(m,0xd30),word(m,0xd34),word(m,0xd38),word(m,0xd64),static_cast<std::uint16_t>(word(m,0x38,2))};return out;
}
Arenas encode(const Arenas& before,const State& state,const Context& context,bool scratch){
    auto out=before;put_motion(out,frame_raw(motion_raw(before,static_cast<unsigned>(state.frame.animation.layers.size())),state.frame,context));auto& m=out.at("model");auto& p=out.at("pbw");const auto& h=state.hierarchy;
    put_words(m,0x70,h.local_rt);put_words(m,0xb0,h.world);put(m,4,h.flags4);put(m,0x104,h.flags104);put(m,0x230,h.flags230);put(m,0x34,h.attachment_index);put(m,0x3a,h.attachment_mode_high16,2);put(m,0xc49,h.dirty,1);put(m,0xc4c,h.fieldc4c);
    check(h.joints.size()*168==out.at("joints").size(),"Hierarchy extent");for(std::size_t i=0;i<h.joints.size();++i){const auto pos=i*168;const auto& j=h.joints[i];auto& bytes=out.at("joints");put_words(bytes,pos+8,j.matrix);put(bytes,pos+0x90,j.depth);put(bytes,pos+0x94,j.metadata.flags94);put(bytes,pos+0x98,j.metadata.mapping98);
        const auto expected_callback=word(before.at("joints"),pos+0x88)?jo::Callback::unsupported:jo::Callback::null;const auto pointer=word(before.at("joints"),pos+0xa0);const auto expected_owner=pointer==0x2000000?jo::Owner::self:pointer?jo::Owner::other:jo::Owner::null;
        check(j.callback==expected_callback&&j.owner==expected_owner,"Borrowed callback/owner identity preserved");}
    const auto* order=std::get_if<std::vector<std::uint8_t>>(&h.order);check(order&&order->size()==out.at("order").size(),"Present order preserved");out.at("order")=*order;
    const auto& c=state.controller;put_words(m,0xd10,c.player.fieldsd10);put_words(m,0xd48,c.motion.fieldsd48);put(m,0xd68,c.motion.fieldd68);put_words(m,0xd70,c.motion.fieldsd70);
    put(p,0x20,c.player.pbw.field20);put(p,0x24,c.player.pbw.field24);put(p,0x28,c.player.pbw.field28);
    const auto& shared=state.shared.pbw;check(shared.field10_bits&&shared.field14_bits&&shared.field18_bits&&shared.field1c_bits,"Projected PBW lanes known");put(p,0x10,*shared.field10_bits);put(p,0x14,*shared.field14_bits);put(p,0x18,*shared.field18_bits);put(p,0x1c,*shared.field1c_bits);
    const auto& f=state.fields;check(f.fieldd2c&&f.fieldd44&&f.fieldd30_bits&&f.fieldd34_bits&&f.fieldd38_bits&&f.fieldd64&&f.euler_mode,"Projected aux fields known");put(m,0xd2c,*f.fieldd2c,1);put(m,0xd44,*f.fieldd44,1);put(m,0xd30,*f.fieldd30_bits);put(m,0xd34,*f.fieldd34_bits);put(m,0xd38,*f.fieldd38_bits);put(m,0xd64,*f.fieldd64);put(m,0x38,*f.euler_mode,2);
    if(scratch){for(unsigned i=0;i<3;++i){put(m,0xd00+4*i,word(p,0x10+4*i));put(m,0xd20+4*i,word(p,0x20+4*i));}put(m,0xd0c,0);}
    return out;
}
State returned(const bridge::Output& o){return {o.frame,o.hierarchy,o.controller,o.shared,o.fields};}
void unrelated(const State& before,const State& after){
    auto player=after.controller.player;player.pbw.field20=before.controller.player.pbw.field20;player.pbw.field24=before.controller.player.pbw.field24;player.pbw.field28=before.controller.player.pbw.field28;
    check(player==before.controller.player,"Other controller fields preserved");auto shared=after.shared;shared.pbw.field10_bits=before.shared.pbw.field10_bits;shared.pbw.field14_bits=before.shared.pbw.field14_bits;shared.pbw.field18_bits=before.shared.pbw.field18_bits;shared.pbw.field1c_bits=before.shared.pbw.field1c_bits;check(shared==before.shared,"Other shared optional fields preserved");
}
std::uint64_t represented(unsigned n,unsigned layers){
    // Unique byte locations projected from native facts. Source callback/owner
    // identity is compared separately; scratch is NOT included in this count.
    std::set<unsigned> m;auto add=[&](unsigned start,unsigned size){for(unsigned i=0;i<size;++i)m.insert(start+i);};
    for(auto [off,size]:std::array<std::pair<unsigned,unsigned>,19>{{{4,4},{0x34,8},{0x40,48},{0x70,128},{0x104,4},{0x230,4},{0x238,8},{0xbd0,48},{0xc49,1},{0xc4c,4},{0xd10,12},{0xd2c,1},{0xd30,12},{0xd44,1},{0xd48,28},{0xd64,4},{0xd68,4},{0xd70,16},{0,0}}})add(off,size);
    for(unsigned i=0;i<layers;++i){const auto p=0x240+i*304;for(auto [off,size]:std::array<std::pair<unsigned,unsigned>,9>{{{4,6},{12,16},{0x20,80},{0x78,96},{0xd8,32},{0xf8,16},{0x108,16},{0x118,16},{0x64,16}}})add(p+off,size);}
    return m.size()+std::uint64_t(n)*(60+64+12+56+52*layers+1)+28;
}
void oracle(const Bytes& fixture,const std::vector<Source>& loaded){
    Reader r{fixture};check(r.bytes(8)==Bytes({'M','H','4','U','P','B','0','1'}),"Fixture schema");const auto count=r.integer(),sources=r.integer();check(count==150&&sources==loaded.size()&&sources==2,"Fixture bounded counts");
    for(unsigned i=0;i<sources;++i){const auto entry=r.integer(),base=r.integer(),size=r.integer();const auto digest=r.bytes(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);check(loaded[i].entry==entry&&loaded[i].base==base&&loaded[i].resource.byte_size()==size&&loaded[i].resource.sha256()==hex,"Pinned source identity");}
    Context context(loaded);std::size_t frames=0,publications=0,early=0,skipped=0,negative=0,lazy=0;bool tested_early=false,tested_skip=false;std::uint64_t full_bytes=0,native_bytes=0,scratch_bytes=0;
    for(unsigned case_index=0;case_index<count;++case_index){try{
        const auto length=r.integer();check(length<=1024*1024,"Fixture record bound");const auto blob=r.bytes(length);Reader in{blob};check(in.integer()==case_index,"Fixture case order");const auto n=in.integer(),layers=in.integer(),arena_count=in.integer(),trace_count=in.integer();check(n==19&&layers>=1&&layers<=2&&arena_count==15+layers&&trace_count<=10000,"Fixture state extents");Arenas before,after;
        for(unsigned i=0;i<arena_count;++i){const auto name_size=in.integer();check(name_size>0&&name_size<=32,"Arena name bound");const auto name_bytes=in.bytes(name_size);std::string name(name_bytes.begin(),name_bytes.end());const auto size=in.integer();check(size<=8192,"Arena byte bound");check(before.emplace(name,in.bytes(size)).second,"Duplicate arena");after.emplace(name,in.bytes(size));}
        const std::map<std::string,std::size_t> fixed{{"model",8192},{"joints",n*168},{"snapshots",n*56},{"pbw",256},{"order",n},{"map",256},{"matrices",n*64},{"rmodel",128},{"wrappers",256},{"global_10e62c0",12},{"global_10e6370",12},{"global_10e6560",16},{"global_10e6570",16},{"global_10e6380",12},{"world_basis",64}};
        for(const auto& [name,size]:fixed)check(before.at(name).size()==size,"Named arena contract");for(unsigned i=0;i<layers;++i)check(before.at("channels_"+std::to_string(i)).size()==n*52,"Channel arena extent");
        const auto& m=before.at("model");check(word(m,0x220)==n&&word(m,0x234)==layers,"Source counts agree");check(word(m,0x30)==0,"Retained parent null profile");check(word(m,0xf0)==0x2400000,"Retained model resource identity");
        for(unsigned bank=0;bank<16;++bank){const auto ptr=word(m,0xc04+4*bank);check(!ptr||ptr==0x2700000||ptr==0x2700080,"Bank pointer owner");context.banks[bank]=ptr?1+(ptr-0x2700000)/0x80:0;}
        std::vector<bool> initialized(layers,false);std::vector<std::pair<std::uint32_t,std::uint32_t>> getters;unsigned frame_calls=0,output_calls=0,init_calls=0;bool reset=false;
        for(unsigned i=0;i<trace_count;++i){const auto pc=in.integer(),r0=in.integer(),r1=in.integer();in.integer();in.integer();if(pc==0x9573b8)++frame_calls;if(pc==0x95b940)++output_calls;if(pc==0x95aa90){check(r1<layers,"Trace layer extent");initialized[r1]=true;++init_calls;}if(pc==0x95c7dc)reset=true;if(pc==0xcd7540){check(r0==0x2700000||r0==0x2700080,"Trace source wrapper");getters.emplace_back((r0-0x2700000)/0x80,r1);}}
        check(in.p==blob.size(),"Fixture record exhausted");const auto arenas_snapshot=before;const auto state=decode(before,context,layers);const auto pristine=state;std::vector<jp::Matrix> matrices;for(unsigned j=0;j<n;++j)matrices.push_back(words<16>(before.at("matrices"),j*64));
        pa::FrameContext fc;fc.bone_to_joint=before.at("map");fc.pending.model_reference=jp::PresentModel{std::span<const jp::Matrix>(matrices)};fc.pending.initialization={word(m,0x14),{words<3>(before.at("global_10e62c0"),0),words<3>(before.at("global_10e6370"),0),words<4>(before.at("global_10e6560"),0),words<4>(before.at("global_10e6570"),0)}};fc.pending.scale_10e6380=words<3>(before.at("global_10e6380"),0);fc.resolvers={{&context,Context::action},{&context,Context::sample},{&context,Context::event}};fc.output.model=jp::PresentModel{jp::Unknown{}};fc.output.parent=jo::Null{};fc.output.reference_basis=words<12>(before.at("world_basis"),0);fc.output.reference_translation=words<4>(before.at("world_basis"),48);
        bridge::Context bc{fc,bridge::OrientationCallback::b05328,bridge::OutputCallbacks::standard_95b490_95b940};context.getters.clear();
        const auto run=[&](const State& s,const bridge::Context& c){return bridge::apply(s.frame,s.hierarchy,s.controller,s.shared,s.fields,c);};
        const auto borrowed_unchanged=[&]{same(before,arenas_snapshot);check(matrices.size()==n,"Borrowed matrix extent unchanged");for(unsigned j=0;j<n;++j)check(matrices[j]==words<16>(arenas_snapshot.at("matrices"),j*64),"Borrowed matrix storage unchanged");};
        const auto unchanged=[&]{borrowed_unchanged();same(encode(before,state,context,false),before);check(logical_identities(motion_raw(before,layers),pristine.frame,state.frame,context,std::vector<bool>(layers,false)),"Input logical identity preserved");check(state.shared==pristine.shared&&state.fields==pristine.fields&&state.controller.player==pristine.controller.player&&state.controller.motion==pristine.controller.motion,"Canonical input fields unchanged");check(state.hierarchy.joints==pristine.hierarchy.joints,"Input hierarchy metadata unchanged");};
        const auto result=run(state,bc);const auto* out=std::get_if<bridge::Output>(&result);if(!out){const auto& e=std::get<bridge::Error>(result);throw std::runtime_error("Native rejection "+std::to_string(static_cast<unsigned>(e.code))+" "+std::string(e.field));}
        const auto actual=returned(*out);same(encode(before,actual,context,true),after);unchanged();unrelated(state,actual);check(logical_identities(motion_raw(after,layers),state.frame,out->frame,context,initialized),"Output track/event logical identities");check(context.getters==getters,"Original source getter sequence");
        check(out->effects.animation.has_value()==(frame_calls==1),"Frame gate matches original");check(out->effects.early_return==(frame_calls==0),"Early gate");if(out->effects.animation){check(out->effects.animation->initializer_calls==init_calls&&out->effects.animation->restored_joints==reset,"Initializer/reset effects");check(out->effects.publication_skipped==(output_calls==0),"Publication gate");}
        frames+=frame_calls;publications+=output_calls;early+=out->effects.early_return;skipped+=out->effects.publication_skipped;
        for(const auto& [name,bytes]:after){(void)name;full_bytes+=bytes.size();}native_bytes+=represented(n,layers);scratch_bytes+=28;
        if(case_index==1){
            check(init_calls==0,"Retained-bank control has no initializer");auto rebanked=state;
            auto set_bank=[](auto& channel){if(channel.track)channel.track->bank=7;};
            for(auto& l:rebanked.frame.animation.layers){auto& a=l.animation;set_bank(a.tracks.translation);set_bank(a.tracks.rotation);for(auto& j:a.tracks.joints)for(auto& channel:j.channels)set_bank(channel);for(auto& b:a.layer.bindings)if(b)b->bank=7;}
            const auto retained=run(rebanked,bc);borrowed_unchanged();check(std::holds_alternative<bridge::Output>(retained),"Retained source bank accepted");const auto& retained_out=std::get<bridge::Output>(retained);
            check(logical_identities(motion_raw(after,layers),rebanked.frame,retained_out.frame,context,initialized),"Bank7 metadata survives despite current selector bank0");same(encode(before,returned(retained_out),context,true),after);
            auto corrupt=retained_out.frame;bool changed=false;for(auto& l:corrupt.animation.layers)for(auto& j:l.animation.tracks.joints)for(auto& channel:j.channels)if(channel.track){channel.track->bank=8;changed=true;}
            check(changed&&!logical_identities(motion_raw(after,layers),rebanked.frame,corrupt,context,initialized),"Metadata-only corruption rejected despite equal ARM address projection");
        }
        if(case_index==0){
            auto reject=[&](const State& input,const bridge::Context& ctx,bridge::ErrorCode wanted,const std::string& field){const auto pre=encode(before,input,context,false);const auto kept=input;const auto fail=run(input,ctx);borrowed_unchanged();const auto* error=std::get_if<bridge::Error>(&fail);check(error&&error->code==wanted&&error->field==field,"Typed bridge rejection "+field);same(encode(before,input,context,false),pre);check(input.shared==kept.shared&&input.fields==kept.fields&&input.controller.player==kept.controller.player,"Rejection preserves optional/canonical fields");check(logical_identities(motion_raw(pre,layers),kept.frame,input.frame,context,std::vector<bool>(layers,false)),"Rejected logical identities unchanged");++negative;};
            auto bad=state;bad.shared.pbw.field10_bits.reset();auto rr=run(bad,bc);borrowed_unchanged();check(std::get<bridge::Error>(rr).code==bridge::ErrorCode::unknown_field&&std::get<bridge::Error>(rr).field=="PBW10","Unknown PBW input");check(!bad.shared.pbw.field10_bits,"Unknown remains unknown");++negative;
            auto missing=bc;missing.orientation.reset();reject(state,missing,bridge::ErrorCode::unknown_field,"virtual80");
            auto no_publication=bc;no_publication.publication.reset();context.getters.clear();reject(state,no_publication,bridge::ErrorCode::unknown_field,"virtual4c/50");check(!context.getters.empty(),"Late publication failure after real resource binding");
            bad=state;bad.fields.fieldd38_bits=0x7fc12345;reject(bad,bc,bridge::ErrorCode::arithmetic,"position blend");
            bad=state;bad.controller.motion.fieldsd48[6]=0x8000;bad.frame.animation.layers[0].animation.layer.remaining_interpolation=1.0F;reject(bad,bc,bridge::ErrorCode::unsupported_route,"b06b38");
            auto output_unknown=fc;output_unknown.output.parent=jo::Unknown{};bridge::Context late{output_unknown,bc.orientation,bc.publication};reject(state,late,bridge::ErrorCode::publication,"95b490/95b940");
            for(const auto fault:{Context::Fault::action,Context::Fault::identity,Context::Fault::sampler,Context::Fault::event}){context.fault=fault;reject(state,bc,bridge::ErrorCode::animation,"9573b8");}context.fault=Context::Fault::none;
            // Unrelated optional absence is preserved without making zero facts.
            bad=state;bad.shared.pw.field154.reset();const auto passed=run(bad,bc);borrowed_unchanged();check(std::holds_alternative<bridge::Output>(passed)&&!std::get<bridge::Output>(passed).shared.pw.field154,"Unused optional remains absent");++lazy;
        }
        if(out->effects.early_return&&!tested_early){auto absent=state;absent.fields.euler_mode.reset();absent.fields.fieldd30_bits.reset();absent.fields.fieldd34_bits.reset();absent.fields.fieldd38_bits.reset();auto missing=bc;missing.orientation.reset();missing.publication.reset();context.fault=Context::Fault::action;const auto passed=run(absent,missing);borrowed_unchanged();context.fault=Context::Fault::none;const auto* lazy_out=std::get_if<bridge::Output>(&passed);check(lazy_out&&lazy_out->effects.early_return&&!lazy_out->fields.euler_mode&&!lazy_out->fields.fieldd38_bits,"Early gate does not consume absent orientation/timers/blend/resolver");tested_early=true;++lazy;}
        if(out->effects.publication_skipped&&!tested_skip){auto missing=bc;missing.publication.reset();const auto passed=run(state,missing);borrowed_unchanged();check(std::holds_alternative<bridge::Output>(passed),"Output gate ignores missing publication callback");tested_skip=true;++lazy;}
    }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(case_index)+": "+e.what());}}
    check(r.p==fixture.size(),"Fixture fully consumed");check(frames==144&&publications==141&&early==6&&skipped==3&&negative==10&&lazy==3,"Executed coverage counts");
    std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"checks\":"<<checks<<",\"frame_calls\":"<<frames<<",\"publication_calls\":"<<publications<<",\"native_typed_rejections\":"<<negative<<",\"lazy_controls\":"<<lazy<<",\"full_arena_bytes\":"<<full_bytes<<",\"native_projected_bytes\":"<<native_bytes<<",\"scratch_projection_bytes\":"<<scratch_bytes<<",\"opaque_borrowed_bytes\":"<<(full_bytes-native_bytes-scratch_bytes)<<"}\n";
}
}
int main(int argc,char** argv){try{
    if(argc!=9)throw std::runtime_error("Usage: player-position-bridge-tests fixture sha256 entry source sha256 entry source sha256");std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(bool(in),"Fixture open");const auto size=in.tellg();check(size>=0&&size<=16*1024*1024,"Fixture allocation bound");in.seekg(0);Bytes bytes(static_cast<std::size_t>(size));in.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(in),"Fixture read");check(picosha2::hash256_hex_string(bytes)==argv[2],"Fixture SHA256 mismatch");std::vector<Source> loaded;
    for(int i=3;i<argc;i+=3){const auto id=std::stoul(argv[i]);check(id<=0xffffffff,"Resource ID extent");auto result=mr::Resource::load(argv[i+1],argv[i+2]);if(const auto* e=std::get_if<mr::Error>(&result))throw std::runtime_error(e->message);loaded.push_back({static_cast<std::uint32_t>(id),loaded.empty()?0x5000000U:0x6000000U,std::get<mr::Resource>(std::move(result))});}oracle(bytes,loaded);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
