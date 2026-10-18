#include "test_support/model_animation_frame_fixture.hpp"
namespace {
using namespace mh4u::test_support::model_frame;
void rejection(const frame::Result& r,frame::ErrorCode code){const auto* e=std::get_if<frame::Error>(&r);check(e&&e->code==code,"Typed frame rejection");}
std::size_t oracle(const Bytes& fixture,const std::vector<Source>& loaded){
    Reader r{fixture};check(r.bytes(8)==Bytes({'M','H','4','U','F','R','0','1'}),"Fixture version");const auto count=r.integer(),source_count=r.integer();check(count>0&&count<=10000&&source_count==loaded.size(),"Fixture bounded counts");
    for(unsigned i=0;i<source_count;++i){const auto entry=r.integer(),base=r.integer(),size=r.integer();const auto digest=r.bytes(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);check(loaded[i].entry==entry&&loaded[i].base==base&&loaded[i].resource.byte_size()==size&&loaded[i].resource.sha256()==hex,"Pinned source identity");}
    Context context(loaded);std::uint64_t compared_bytes=0;std::size_t native_rejections=0;
    for(unsigned i=0;i<count;++i){
        const auto size=r.integer();check(size<=1024*1024,"Fixture record allocation bound");const auto record=r.bytes(size);Reader in{record};in.integer();const auto n=in.integer(),layers=in.integer(),trace_count=in.integer();check(n<=255&&layers<=4&&trace_count<=100000,"Fixture state extents");
        for(auto& bank:context.banks){bank=in.integer();check(bank<=loaded.size(),"Fixture bank identity");}context.getters.clear();
        const auto globals=in.bytes(68),map=in.bytes(256),matrix_raw=in.bytes(n*64);std::vector<jp::Matrix> matrices;for(unsigned j=0;j<n;++j)matrices.push_back(words<16>(matrix_raw,j*64));
        const auto before=raw(in,n,layers),after=raw(in,n,layers);std::vector<std::pair<std::uint32_t,std::uint32_t>> getters;unsigned calls=0,resets=0,pending_calls=0;std::vector<bool> initialized(layers,false);std::optional<mh4u::joint_update::Leaf> leaf;
        for(unsigned j=0;j<trace_count;++j){const auto pc=in.integer(),r0=in.integer(),r1=in.integer(),r2=in.integer(),s0=in.integer();(void)r2;(void)s0;
            if(pc==0xcd7540){check(r0>=0x2700000&&r0<0x2700100&&(r0-0x2700000)%0x80==0,"Getter wrapper identity");getters.emplace_back((r0-0x2700000)/0x80,r1);}
            else if(pc==0x95aa90){++calls;check(r1<layers,"Initializer layer identity");initialized[r1]=true;}else if(pc==0x95c7dc)++resets;else if(pc==0x9577ec)++pending_calls;
            else if(pc==0x95df38)leaf=mh4u::joint_update::Leaf::rotation_translation;else if(pc==0x95e6ec)leaf=mh4u::joint_update::Leaf::rotation_translation_scale;
        }
        check(in.p==record.size(),"Fixture record fully consumed");const auto state=frame_state(before,context);const auto pristine=state;
        frame::Parameters p;p.pending.model_reference=word(before.model,0xf0)?jp::ModelReference{jp::PresentModel{std::span<const jp::Matrix>(matrices)}}:jp::ModelReference{jp::Null{}};
        p.pending.initialization={word(before.model,0x14),{words<3>(globals,0),words<3>(globals,12),words<4>(globals,24),words<4>(globals,40)}};p.pending.scale_10e6380=words<3>(globals,56);
        std::vector<mh4u::joint_update::Metadata> metadata;for(unsigned j=0;j<n;++j)metadata.push_back({word(before.joints,j*168+0x94),word(before.joints,j*168+0x98)});p.metadata=metadata;
        const frame::Resolvers resolvers{{&context,Context::action},{&context,Context::sample},{&context,Context::event}};
        auto unchanged=[&]{equal(frame_raw(before,state,context),before);check(logical_identities(before,pristine,state,context,std::vector<bool>(layers,false)),"Input logical identity unchanged");};
        const auto result=frame::step(state,map,p,resolvers);const auto* out=std::get_if<frame::Output>(&result);
        if(!out){const auto& e=std::get<frame::Error>(result);throw std::runtime_error("ARM case "+std::to_string(i)+" rejected at "+std::to_string(static_cast<unsigned>(e.code))+": "+std::string(e.message));}
        try{
            check(out->pending_found==(pending_calls!=0),"Pending call observation");check(out->initializer_calls==calls&&out->restored_joints==(resets!=0),"Original initializer/reset calls");check(context.getters==getters,"Original getter order/source placement");check(out->joint_leaf==leaf,"Original joint leaf");
            check(logical_identities(after,state,out->state,context,initialized),"Logical track/event identities, including bank provenance");
            equal(frame_raw(before,out->state,context),after);unchanged();
        }catch(const std::exception& e){throw std::runtime_error("ARM case "+std::to_string(i)+": "+e.what());}
        compared_bytes+=before.model.size()+before.joints.size()+before.snapshots.size();for(const auto& x:before.channels)compared_bytes+=x.size();
        if(i==0){
            for(const auto [fault,code]:{std::pair{Context::Fault::action,frame::ErrorCode::pending},std::pair{Context::Fault::identity,frame::ErrorCode::pending},std::pair{Context::Fault::sampler,frame::ErrorCode::pending},std::pair{Context::Fault::event,frame::ErrorCode::clock}}){context.fault=fault;rejection(frame::step(state,map,p,resolvers),code);unchanged();++native_rejections;}context.fault=Context::Fault::none;
            auto missing=p;missing.pending.model_reference=jp::Unknown{};rejection(frame::step(state,map,missing,resolvers),frame::ErrorCode::unknown_model);++native_rejections;
            auto limit=frame::Limits{};limit.layers=0;rejection(frame::step(state,map,p,resolvers,limit),frame::ErrorCode::resource_limit);++native_rejections;
        }
        if(i==1){
            // Preserved logical placement need not match the current selector.
            // Change only native identity metadata: raw ARM pointer inputs are
            // unchanged and this frame has no initializer. This is a native
            // provenance control, not an additional original ARM case.
            check(!pending_calls,"Bank retention control skips initialization");auto remapped=state;
            auto rebank=[](auto& channel){if(channel.track)channel.track->bank=7;};
            for(auto& layer:remapped.animation.layers){auto& animation=layer.animation;rebank(animation.tracks.translation);rebank(animation.tracks.rotation);for(auto& joint:animation.tracks.joints)for(auto& channel:joint.channels)rebank(channel);for(auto& binding:animation.layer.bindings)if(binding)binding->bank=7;}
            const auto retained=frame::step(remapped,map,p,resolvers);const auto* retained_out=std::get_if<frame::Output>(&retained);check(retained_out!=nullptr,"Remapped identity provenance accepted");
            check(logical_identities(after,remapped,retained_out->state,context,initialized),"Nonselector bank provenance preserved");equal(frame_raw(before,retained_out->state,context),after);
            auto wrong=retained_out->state;bool changed=false;
            for(auto& layer:wrong.animation.layers)for(auto& joint:layer.animation.tracks.joints)for(auto& channel:joint.channels)if(channel.track){channel.track->bank=8;changed=true;}
            check(changed&&!logical_identities(after,remapped,wrong,context,initialized),"Bank mismatch is detected despite identical ARM pointer projection");equal(frame_raw(before,wrong,context),after);
            auto invalid=state;invalid.animation.layers[0].animation.layer.frame=std::bit_cast<float>(0x7fc12345U);rejection(frame::step(invalid,map,p,resolvers),frame::ErrorCode::preparation);unchanged();++native_rejections;}
    }
    check(r.p==fixture.size(),"No trailing fixture bytes");std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"real_arm_cases\":"<<count<<",\"compared_arena_bytes\":"<<compared_bytes<<",\"native_typed_rejections\":"<<native_rejections<<"}\n";return count;
}

} // namespace
int main(int argc,char** argv){
    try{
        if(argc<6||(argc-3)%3!=0)throw std::runtime_error("Usage: model-animation-frame-tests oracle.bin sha256 entry source.lmt sha256 [entry source.lmt sha256 ...]");
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate);check(bool(file),"Fixture open");const auto length=file.tellg();check(length>=0&&length<=128*1024*1024,"Fixture byte limit");file.seekg(0);Bytes fixture(static_cast<std::size_t>(length));file.read(reinterpret_cast<char*>(fixture.data()),length);check(bool(file),"Fixture read");check(picosha2::hash256_hex_string(fixture)==argv[2],"Fixture SHA256 mismatch");
        std::vector<Source> loaded;for(int i=3;i<argc;i+=3){const auto entry=std::stoul(argv[i]);check(entry<=0xffffffffU&&loaded.size()<2,"Resource argument extent");auto resource=mr::Resource::load(argv[i+1],argv[i+2]);if(const auto* e=std::get_if<mr::Error>(&resource))throw std::runtime_error(e->message);loaded.push_back({static_cast<std::uint32_t>(entry),loaded.empty()?0x5000000U:0x6000000U,std::get<mr::Resource>(std::move(resource))});}
        oracle(fixture,loaded);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
