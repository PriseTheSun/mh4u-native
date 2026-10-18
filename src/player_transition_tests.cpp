#include "player_transition.hpp"
#include "picosha2.h"
#include <cfenv>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef MH4U_PLAYER_TRANSITION_GENERATED
#include "player_transition_constants.hpp"
#include "player_transition_fixtures.hpp"
#endif
namespace {
namespace pt=mh4u::player_transition;
std::size_t assertions=0;
void check(bool good,const char* message){++assertions;if(!good)throw std::runtime_error(message);}
template<class T>T value(pt::Result<T> result){if(auto* e=std::get_if<pt::Error>(&result))throw std::runtime_error(std::string(pt::error_name(e->code)));return std::get<T>(std::move(result));}
template<class T>void error(pt::Result<T> result,pt::ErrorCode code){check(std::holds_alternative<pt::Error>(result)&&std::get<pt::Error>(result).code==code,"Wrong rejection");}
std::vector<std::uint8_t> synthetic_constants(){
    std::vector<std::uint8_t> b{'M','H','4','U','P','T','0','1'};
    const std::string sha="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435";
    for(std::size_t i=0;i<sha.size();i+=2)b.push_back(static_cast<std::uint8_t>(std::stoul(sha.substr(i,2),nullptr,16)));
    for(auto n:{17U,0x3f8ccccdU,0x3f866666U,0x1555U})for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i)));
    for(unsigned i=0;i<32;++i)b.push_back(static_cast<std::uint8_t>(i*7));return b;
}
pt::ExpectedConstants pin(const std::vector<std::uint8_t>& b){return {picosha2::hash256_hex_string(b.begin(),b.end()),"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"};}
void synthetic(){
    const auto bytes=synthetic_constants();const auto expected=pin(bytes);const auto constants=value(pt::Constants::decode(bytes,expected));
    pt::State input;input.pbw.field30=1;input.global.field2b=2;input.pbw.field33=0;input.pbw.field371=3;
    input.pbw.field372=0x1234;input.pw.fieldf0=0x11000007;input.pw.fieldf4=0x12345678;input.global.field22=0;
    input.pw.fieldbc=0xabcd;input.player.field3b8=0x87654321;input.pw.field490=1;input.pw.field4c2=0x1234;
    const auto untouched=input;
    auto out=value(pt::apply(input,{0,147,0x20},constants));
    check(input==untouched,"Input mutated");check(out.state.pbw.field377==3&&out.state.pbw.field378==0x1234,"Previous action lost");
    check(out.state.global.field22==176&&out.state.pw.fieldbc==0xcd,"Global update/order or low-byte preserve changed");
    check(out.returned_dispatch_call==pt::ObservedDispatchCall{9,0x20},"Full argument selection lost");
    auto narrow=value(pt::apply(input,{0x100,0x10093,0},constants));check(narrow.state.pbw.field371==0&&narrow.state.pbw.field372==147,"Stores did not narrow");
    check(narrow.returned_dispatch_call==pt::ObservedDispatchCall{1,0},"Arguments narrowed before branch");
    auto six=value(pt::apply(input,{6,0,2},constants));auto six_high=value(pt::apply(input,{262,0,2},constants));
    check(six.state.pw.field504==0x1234&&six_high.state.pw.field504==65535,"Full state6 test changed");
    check(!six.returned_dispatch_call,"Excluded by flag2 but dispatch recorded");
    check(value(pt::apply(input,{1,2,1},constants)).state.player.field3b8==input.player.field3b8,"Flag1 preservation lost");
    check(value(pt::apply(input,{1,2,0},constants)).state.player.field3b8==0,"Inline b04e40 store not applied");
    const std::array<std::pair<pt::ErrorCode,pt::State>,6> bad{{
        {pt::ErrorCode::type_table_extent,[&]{auto s=input;s.pbw.field31=16;return s;}()},
        {pt::ErrorCode::optional_dispatch_body,[&]{auto s=input;s.global.field5d=1;return s;}()},
        {pt::ErrorCode::early_optional_helpers,[&]{auto s=input;s.pbw.field31=4;s.pbw.field3d4=0;return s;}()},
        {pt::ErrorCode::old_state7_helpers,[&]{auto s=input;s.pbw.field371=7;return s;}()},
        {pt::ErrorCode::type13_helpers,[&]{auto s=input;s.pbw.field31=13;return s;}()},
        {pt::ErrorCode::angular_reference,[&]{auto s=input;s.pbw.field451=1;return s;}()},
    }};
    for(const auto& [code,state]:bad){const auto original=state;error(pt::apply(state,{2,147,0x9000},constants),code);check(state==original,"Rejected input changed");}
    auto changed=bytes;changed[56]^=1;error(pt::Constants::decode(changed,expected),pt::ErrorCode::constants_identity);
    changed=bytes;changed[0]='X';error(pt::Constants::decode(changed,pin(changed)),pt::ErrorCode::unsupported_constants);
    changed=bytes;changed[8]^=1;error(pt::Constants::decode(changed,pin(changed)),pt::ErrorCode::constants_identity);
    changed=bytes;changed[44]^=1;error(pt::Constants::decode(changed,pin(changed)),pt::ErrorCode::unsupported_constants);
    auto bad_pin=expected;bad_pin.package_sha256="";error(pt::Constants::decode(bytes,bad_pin),pt::ErrorCode::invalid_expected);
    for(std::size_t cut=0;cut<bytes.size();++cut)error(pt::Constants::decode(std::span(bytes).first(cut),expected),pt::ErrorCode::unsupported_constants);
    changed=bytes;changed.push_back(0);error(pt::Constants::decode(changed,pin(changed)),pt::ErrorCode::unsupported_constants);
    const auto rounding=std::fegetround();std::fesetround(FE_DOWNWARD);
    error(pt::apply(input,{0,0,0},constants),pt::ErrorCode::unsupported_float_environment);std::fesetround(rounding);
}
#ifdef MH4U_PLAYER_TRANSITION_GENERATED
void original(){
    const auto constants=value(pt::Constants::decode(pt::generated::constants_bytes,pt::generated::expected_constants()));
    std::size_t count=0;
    for(const auto& c:transition_oracle::cases){
        const auto result=value(pt::apply(c.before,c.request,constants));
        if(result.state!=c.after)throw std::runtime_error("Full declared state differs from ARM at case "+std::to_string(count));
        check(result.state==c.after,"State mismatch");check(result.effective_flags==c.flags,"Effective flags mismatch");
        check(result.returned_dispatch_call.has_value()==c.called,"Dispatch presence mismatch");
        if(c.called)check(result.returned_dispatch_call==pt::ObservedDispatchCall{c.selector,c.call_flags},"Dispatch arguments mismatch");
        ++count;
    }
    check(count==transition_oracle::cases.size()&&count>500,"Incomplete original coverage");
    std::cout<<"{\"original_transition_cases\":"<<count<<",\"full_state_mismatches\":0}\n";
}
#endif
}
int main(){try{synthetic();
#ifdef MH4U_PLAYER_TRANSITION_GENERATED
original();
#endif
std::cout<<"{\"status\":\"passed\",\"assertions\":"<<assertions<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
