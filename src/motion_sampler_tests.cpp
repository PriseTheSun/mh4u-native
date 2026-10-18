#include "motion_sampler.hpp"
#include <picosha2.h>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace {
namespace ms=mh4u::motion_sampler;
namespace mb=mh4u::motion_binding;
std::size_t checks=0;
void check(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
ms::Sample result(ms::Result value){
    if(const auto* error=std::get_if<ms::Error>(&value))throw std::runtime_error(std::string(error->message));
    return std::get<ms::Sample>(std::move(value));
}
void rejected(ms::Result value,ms::ErrorCode code){check(std::holds_alternative<ms::Error>(value)&&std::get<ms::Error>(value).code==code,"Unexpected sampler rejection");}
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
struct FloatingControl {
    std::uint64_t original;
    FloatingControl(){
#if defined(__aarch64__)
        __asm__ volatile("mrs %0, fpcr":"=r"(original));
#else
        original=_mm_getcsr();
#endif
    }
    void set(std::uint64_t value){
#if defined(__aarch64__)
        __asm__ volatile("msr fpcr, %0\n\tisb"::"r"(value):"memory");
#else
        _mm_setcsr(static_cast<unsigned>(value));
#endif
    }
    ~FloatingControl(){set(original);}
};
#endif
void direct(){
    mb::Channel channel{std::nullopt,0xffffffff,mb::BufferReference{99,123},0x7fa12345};
    auto out=result(ms::sample(channel,nullptr,0x7fa56789));
    check(out.value_bits==std::array<std::uint32_t,4>{0,0,0,0x3f800000},"Null fallback changed");
    check(out.channel==channel,"Null changed cursor");
    ms::TrackView track{{42,0,0,0},1,{0x80000000,0x7fa12345,0xffffffff,0x3f000000},std::nullopt,std::nullopt};
    rejected(ms::sample(channel,&track,0),ms::ErrorCode::resource_identity);
    channel.track=track.identity;
    rejected(ms::sample(channel,nullptr,0),ms::ErrorCode::unresolved_track);
    for(auto codec:{1U,2U}){track.metadata=codec;out=result(ms::sample(channel,&track,0x7fa56789));check(out.value_bits==track.reference_bits&&out.channel==channel,"Reference changed raw bits/cursor");}
    auto wrong=track;wrong.identity.ordinal++;
    rejected(ms::sample(channel,&wrong,0),ms::ErrorCode::resource_identity);
    track.metadata=255;rejected(ms::sample(channel,&track,0),ms::ErrorCode::unsupported_codec);
    track.metadata=5;rejected(ms::sample(channel,&track,0),ms::ErrorCode::unknown_key_data);
    const std::array<std::byte,8> bytes{std::byte{8},std::byte{9},std::byte{10},std::byte{3},std::byte{11},std::byte{12},std::byte{13},std::byte{0}};
    track.keys=ms::KeyBuffer{{42,1000},bytes};channel.key_data=mb::BufferReference{42,1000};channel.frame_bits=0;
    rejected(ms::sample(channel,&track,0),ms::ErrorCode::unknown_extremes);
    track.extremes_bits=std::array<std::uint32_t,8>{bits(1),bits(1),bits(1),0,0,0,0,0};
    auto saved=channel;
    rejected(ms::sample(channel,&track,bits(-1)),ms::ErrorCode::key_extent);
    rejected(ms::sample(channel,&track,0,{0}),ms::ErrorCode::resource_limit);
    rejected(ms::sample(channel,&track,0x7f800000),ms::ErrorCode::nonfinite_operand);
    channel.key_data->byte_offset=1001;rejected(ms::sample(channel,&track,0),ms::ErrorCode::cursor_extent);channel=saved;
    track.keys->bytes=std::span(bytes).first(7);rejected(ms::sample(channel,&track,bits(3)),ms::ErrorCode::key_extent);
    track.keys->bytes=std::span(bytes).first(4);rejected(ms::sample(channel,&track,bits(.5F)),ms::ErrorCode::key_extent);track.keys->bytes=bytes;
    track.keys->base.resource_identity=43;rejected(ms::sample(channel,&track,0),ms::ErrorCode::resource_identity);track.keys->base.resource_identity=42;
    (*track.extremes_bits)[0]=0x7f800000;rejected(ms::sample(channel,&track,0),ms::ErrorCode::nonfinite_operand);(*track.extremes_bits)[0]=bits(1);
    const int old=std::fegetround();check(std::fesetround(FE_UPWARD)==0,"Cannot set rounding for negative control");
    rejected(ms::sample(channel,&track,0),ms::ErrorCode::floating_environment);
    auto reference_only=track;reference_only.metadata=2;
    check(result(ms::sample(channel,&reference_only,0xffffffff)).value_bits==reference_only.reference_bits,"Reference unnecessarily required floating environment");
    check(std::fesetround(old)==0,"Cannot restore rounding");
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
    {
        FloatingControl control;
#if defined(__aarch64__)
        constexpr std::array<std::uint64_t,1> masks{1ULL<<24};
#else
        constexpr std::array<std::uint64_t,3> masks{1ULL<<15,1ULL<<6,(1ULL<<15)|(1ULL<<6)};
#endif
        for(auto mask:masks){
            control.set(control.original|mask);
            rejected(ms::sample(channel,&track,0),ms::ErrorCode::floating_environment);
            check(result(ms::sample(channel,&reference_only,0xffffffff)).value_bits==reference_only.reference_bits,"Reference incorrectly consulted FTZ/DAZ probe");
            mb::Channel absent;check(result(ms::sample(absent,nullptr,0xffffffff)).channel==absent,"Absent channel incorrectly consulted FTZ/DAZ probe");
        }
    }
#endif
    const std::array<std::byte,8> zero_quaternion{};
    auto degenerate=track;degenerate.metadata=6;degenerate.keys->bytes=zero_quaternion;degenerate.extremes_bits.reset();
    rejected(ms::sample(channel,&degenerate,0),ms::ErrorCode::degenerate_quaternion);
    auto missing_cursor=channel;missing_cursor.key_data.reset();rejected(ms::sample(missing_cursor,&track,0),ms::ErrorCode::unknown_key_data);
    check(channel==saved,"Rejected sampling changed caller state");
}
struct Reader {
    const std::vector<unsigned char>& bytes;std::size_t at{};
    std::uint32_t word(){if(at>bytes.size()||bytes.size()-at<4)throw std::runtime_error("Truncated fixture");std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(bytes[at++])<<(8*i);return v;}
    std::span<const std::byte> block(std::size_t n){if(at>bytes.size()||n>bytes.size()-at)throw std::runtime_error("Truncated fixture block");const auto* p=reinterpret_cast<const std::byte*>(bytes.data()+at);at+=n;return{p,n};}
};
void original(const char* filename,const char* expected_sha){
    std::ifstream stream(filename,std::ios::binary|std::ios::ate);check(bool(stream),"Cannot open original oracle");
    const auto size=stream.tellg();check(size>=12&&size<=64*1024*1024,"Oracle size outside bound");stream.seekg(0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));stream.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(stream),"Oracle read failed");
    check(picosha2::hash256_hex_string(bytes)==expected_sha,"Oracle digest mismatch");
    check(std::string(bytes.begin(),bytes.begin()+8)=="MH4UMS01","Oracle version mismatch");Reader read{bytes,8};const auto count=read.word();check(count<=100000,"Oracle count outside bound");
    for(std::size_t i=0;i<count;++i){
        const auto codec=read.word(),keys_present=read.word(),cursor_present=read.word(),offset=read.word(),frame=read.word(),weight=read.word(),query=read.word(),extremes_present=read.word(),key_size=read.word();
        std::array<std::uint32_t,4> reference;for(auto& v:reference)v=read.word();
        std::array<std::uint32_t,8> extremes;for(auto& v:extremes)v=read.word();const auto key_bytes=read.block(key_size);
        std::array<std::uint32_t,4> expected;for(auto& v:expected)v=read.word();const auto expected_frame=read.word(),expected_offset=read.word();
        ms::TrackView track{{42,0,0,0},codec,reference,std::nullopt,std::nullopt};
        if(keys_present)track.keys=ms::KeyBuffer{{42,1000},key_bytes};if(extremes_present)track.extremes_bits=extremes;
        mb::Channel before{codec==0xffffffffU?std::nullopt:std::optional(track.identity),frame,std::nullopt,weight};
        if(cursor_present)before.key_data=mb::BufferReference{42,1000+offset};
        const auto out=result(ms::sample(before,codec==0xffffffffU?nullptr:&track,query));
        if(out.value_bits!=expected)throw std::runtime_error("Value bits differ from ARM case "+std::to_string(i)+" codec "+std::to_string(codec));
        check(out.value_bits==expected,"Value bits differ");check(out.channel.frame_bits==expected_frame,"Cursor frame differs");
        check(out.channel.key_data==(cursor_present?std::optional(mb::BufferReference{42,1000+expected_offset}):std::nullopt),"Cursor pointer differs");
        check(out.channel.track==before.track&&out.channel.weight_bits==before.weight_bits,"Unmodified channel fields differ");
    }
    check(read.at==bytes.size(),"Oracle trailing bytes");std::cout<<"{\"original_queries\":"<<count<<",\"value_bit_mismatches\":0,\"cursor_mismatches\":0}\n";
}
}
int main(int argc,char** argv){try{direct();if(argc==3)original(argv[1],argv[2]);else if(argc!=1)throw std::runtime_error("Expected oracle path and SHA256");std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
