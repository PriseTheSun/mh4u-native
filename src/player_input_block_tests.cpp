#include "player_input_block.hpp"
#include <iostream>
#include <stdexcept>
namespace ib=mh4u::player_input_block;
namespace is=mh4u::player_input_storage;
namespace pcs=mh4u::player_controller_step;
namespace pf=mh4u::player_fields;
namespace {
std::size_t checks{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool required(std::size_t offset){return (offset>=0x3c0&&offset<0x3c8)||(offset>=0x3d4&&offset<0x3d8)||offset==0x451;}
bool half(std::size_t offset){return (offset>=0x3a8&&offset<0x3aa)||(offset>=0x3ac&&offset<0x3ae)||(offset>=0x3b8&&offset<0x3ba)||(offset>=0x3bc&&offset<0x3be);}
void run(){
    pcs::PlayerFields player{};pf::Fields fields{};
    player.global.fieldd72=0x57;player.pbw.field371=37;player.pw.fielde8=0x67890123;
    fields.pbw.field54_bits=0x3f876543;fields.pbw.field464_bits=0x7fc01234;
    fields.pw.field13c_bits=0x80000000;fields.pw.field9dc=0x35;
    const auto original_player=player;const auto original_fields=fields;
    ib::Block block{};
    for(std::size_t i=0;i<block.size();++i)block[i]=std::uint8_t(i*37+19);
    auto result=ib::publish(block,player,fields);
    check(std::holds_alternative<ib::Output>(result),"Known block rejected");
    const auto initial=std::get<ib::Output>(result);
    check(ib::project(initial.player,initial.fields)==block,"196-byte known roundtrip");
    check(initial.player.global==player.global&&initial.player.pw==player.pw,"Outside controller changed");
    check(initial.fields.pbw.field54_bits==fields.pbw.field54_bits&&initial.fields.pbw.field464_bits==fields.pbw.field464_bits&&initial.fields.pw==fields.pw,"Outside extra fields changed");
    check(player==original_player&&fields==original_fields,"Publish mutated input");
    // Each unknown lane must either survive with its neighboring bytes intact
    // or fail at the explicitly narrower canonical scalar boundary.
    for(std::size_t i=0;i<block.size();++i){
        auto partial=block;partial[i].reset();const auto offset=is::first+i;
        result=ib::publish(partial,initial.player,initial.fields);
        if(required(offset)||half(offset)){
            const auto* error=std::get_if<ib::Error>(&result);check(error!=nullptr,"Scalar lost partial knowledge");
            check(error->code==(required(offset)?ib::ErrorCode::unknown_required_alias:ib::ErrorCode::partial_optional_alias),"Wrong unknown boundary");
        }else{
            check(std::holds_alternative<ib::Output>(result),"Byte owner rejected unknown");
            const auto& out=std::get<ib::Output>(result);
            check(ib::project(out.player,out.fields)==partial,"Byte owner lost neighboring values");
        }
        check(ib::project(initial.player,initial.fields)==block,"Failed/successful call mutated original");
    }
    for(const std::size_t offset:{0x3a8,0x3ac,0x3b8,0x3bc}){
        auto unknown=block;unknown[offset-is::first].reset();unknown[offset-is::first+1].reset();
        result=ib::publish(unknown,initial.player,initial.fields);
        check(std::holds_alternative<ib::Output>(result),"Unknown halfword rejected");
        const auto& out=std::get<ib::Output>(result);check(ib::project(out.player,out.fields)==unknown,"Unknown halfword fabricated");
    }
    // The same lane remains unknown to word consumers until its final byte is
    // supplied; no per-word shadow may make a partial original store appear full.
    const auto complete=is::split(0x12345678);check(is::join(complete)==0x12345678,"Word endian mismatch");
    for(unsigned i=0;i<4;++i){auto partial=complete;partial[i].reset();check(!is::join(partial),"Partial word exposed as known");}
    check(!is::join(is::split(std::nullopt)),"Unknown word fabricated");
}
}
int main(){try{run();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
