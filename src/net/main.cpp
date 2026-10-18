#include "content.hpp"
#include "socket.hpp"
#include "wire.hpp"
#include "peer.hpp"
#include "world.hpp"
#include "world_client.hpp"
#include <iomanip>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <thread>

namespace {
using namespace mh4u::net;
volatile std::sig_atomic_t interrupted=0;
void signal_stop(int) { interrupted=1; }
std::uint64_t integer(const std::string& value) {
    std::uint64_t result=0;const auto [p,e]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(e!=std::errc{}||p!=value.data()+value.size())throw std::runtime_error("invalid_integer");return result;
}
struct Options {
    std::string mode,name="client";std::filesystem::path manifest,world;
    std::uint16_t port=0,selection=0xffff;
    int run_ms=5000,select_after_ms=0,handshake_ms=1000,idle_ms=1500;
    bool select=false,move=false;MotionInput motion;int move_after_ms=0,move_ms=60000;
};
Options options(int argc,char** argv) {
    if(argc<2)throw std::runtime_error("usage: mh4u-network-lab verify|server|client (--manifest FILE | --world FILE) [--port N --name NAME --select N --run-ms N --move-x N --move-z N --turn N --move-after-ms N --move-ms N]");
    Options out;out.mode=argv[1];
    if(out.mode!="verify"&&out.mode!="server"&&out.mode!="client")throw std::runtime_error("invalid_mode");
    for(int i=2;i<argc;++i){const std::string option=argv[i];if(i+1>=argc)throw std::runtime_error("missing_option_value");const std::string value=argv[++i];
        if(option=="--manifest")out.manifest=utf8_path(value);
        else if(option=="--world")out.world=utf8_path(value);
        else if(option=="--move-x"||option=="--move-z"||option=="--turn") {
            int axis{};const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),axis);
            if(error!=std::errc{}||end!=value.data()+value.size()||axis < -1000||axis>1000)throw std::runtime_error("motion_range");
            if(option=="--move-x")out.motion.x=axis;else if(option=="--move-z")out.motion.z=axis;else out.motion.turn=axis;out.move=true;
        }
        else if(option=="--name"){if(!valid_name(value))throw std::runtime_error("invalid_name");out.name=value;}
        else {const auto number=integer(value);
            if(option=="--port"){if(number>65535)throw std::runtime_error("port_range");out.port=static_cast<std::uint16_t>(number);}
            else if(option=="--select"){if(number>65535)throw std::runtime_error("selection_range");out.selection=static_cast<std::uint16_t>(number);out.select=true;}
            else if(option=="--run-ms"){if(number<1||number>86400000)throw std::runtime_error("run_range");out.run_ms=static_cast<int>(number);}
            else if(option=="--move-after-ms"){if(number>60000)throw std::runtime_error("delay_range");out.move_after_ms=static_cast<int>(number);}
            else if(option=="--move-ms"){if(number<1||number>86400000)throw std::runtime_error("motion_duration_range");out.move_ms=static_cast<int>(number);}
            else if(option=="--select-after-ms"){if(number>60000)throw std::runtime_error("delay_range");out.select_after_ms=static_cast<int>(number);}
            else if(option=="--handshake-ms"){if(number<100||number>5000)throw std::runtime_error("handshake_range");out.handshake_ms=static_cast<int>(number);}
            else if(option=="--idle-ms"){if(number<500||number>10000)throw std::runtime_error("idle_range");out.idle_ms=static_cast<int>(number);}
            else throw std::runtime_error("unknown_option");
        }
    }
    if(out.manifest.empty()==out.world.empty())throw std::runtime_error("exactly_one_manifest_or_world_required");
    if(out.move&&(out.world.empty()||out.mode!="client"))throw std::runtime_error("motion_requires_world_client");validate_motion(out.motion);if(out.mode=="client"&&!out.port)throw std::runtime_error("client_port_required");return out;
}

struct Participant {std::uint32_t id;std::string name;std::uint64_t acknowledged;};
struct Snapshot {std::uint64_t revision;std::uint16_t selection;std::vector<Participant> participants;};
Snapshot snapshot(Reader& body,std::uint16_t clips) {
    Snapshot state{};state.revision=body.number(8);state.selection=static_cast<std::uint16_t>(body.number(2));
    if(state.selection!=0xffff&&state.selection>=clips)throw std::runtime_error("invalid_state_selection");
    const auto count=body.number(1);if(count>4)throw std::runtime_error("invalid_state_count");
    std::uint32_t previous=0;
    for(std::uint64_t i=0;i<count;++i){const auto id=static_cast<std::uint32_t>(body.number(4));const auto name=body.text();const auto sequence=body.number(8);if(id<=previous||!valid_name(name))throw std::runtime_error("invalid_participant");previous=id;state.participants.push_back({id,name,sequence});}
    body.finish();return state;
}
void print_state(const char* who,const Snapshot& state) {
    std::cout<<"STATE who="<<who<<" revision="<<state.revision<<" selected="<<state.selection<<" members=";
    for(std::size_t i=0;i<state.participants.size();++i){if(i)std::cout<<',';const auto& p=state.participants[i];std::cout<<p.id<<':'<<p.name<<':'<<p.acknowledged;}
    std::cout<<'\n';
}
struct ServerPeer:Peer {
    std::optional<WorldActor> actor;
    ServerPeer(Socket socket,std::uint16_t version):Peer(std::move(socket),version){}
};
void print_world(const char* who,const WorldSnapshot& state) {
    std::cout<<"WORLD_STATE who="<<who<<" session="<<state.session<<" tick="<<state.tick<<" revision="<<state.revision<<" selected="<<state.selection<<" members=";
    for(std::size_t i=0;i<state.participants.size();++i) {
        if(i)std::cout<<',';const auto& p=state.participants[i];
        std::cout<<p.id<<':'<<p.name<<':'<<p.accepted<<':'<<p.applied<<':'<<p.state.position.x<<':'<<p.state.position.y<<':'<<p.state.position.z<<':'<<p.state.yaw<<':'<<p.blocked<<':'<<p.expired<<':'<<p.selection_acknowledged;
    }
    std::cout<<'\n';
}
int server(const Options& args,const Content& content,NetworkWorld* world=nullptr) {
    auto listener=Socket::listen_loopback(args.port);std::vector<std::unique_ptr<ServerPeer>> peers;
    const auto version=world?world_protocol_version:protocol_version;
    const auto started=Clock::now();std::uint64_t revision=0;std::uint32_t next_id=1;std::uint16_t selected=world?world_initial_selection:0xffff;
    std::random_device random;const std::uint64_t session=(std::uint64_t(random())<<32)|random();
    bool shutdown=false;Time shutdown_since{};std::uint64_t tick=0;auto next_tick=started+std::chrono::milliseconds(world_tick_ms);
    std::cout<<"READY port="<<listener.port()<<" protocol="<<version<<" content="<<content.hex<<" clips="<<content.clips.size()<<" session="<<session<<'\n';
    auto reject=[&](Peer& peer,const std::string& reason){
        if(peer.dead||peer.closing)return;peer.closing=true;peer.closing_since=Clock::now();peer.reason=reason;
        std::cout<<"REJECT id="<<peer.id<<" reason="<<reason<<'\n';
        try{Writer body;body.text(reason.substr(0,128));peer.queue(Message::reject,body);}catch(...){peer.dead=true;}
    };
    auto advance=[&]{if(revision==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("revision_overflow");++revision;};
    auto broadcast=[&]{
        // Connections are stored in accept order, but participant IDs are
        // assigned in HELLO order. Both protocols require ascending IDs.
        // Keep the poll-indexed peer vector intact and order this snapshot's
        // membership once for both wire formats.
        std::vector<const ServerPeer*> members;
        for(const auto& p:peers)if(p->ready&&!p->dead)members.push_back(p.get());
        std::sort(members.begin(),members.end(),[](const auto* a,const auto* b){return a->id<b->id;});
        Writer body;
        if(world) {
            WorldSnapshot state;state.session=session;state.digest=content.digest;state.tick=tick;state.revision=revision;state.selection=selected;
            for(const auto* p:members) {
                const auto& a=*p->actor;
                state.participants.push_back({p->id,p->name,p->last_intent,a.accepted,a.applied,a.state,a.blocked,a.expired});
            }
            body=write_world_snapshot(state);print_world("server",state);
        } else {
            body.number(revision,8);body.number(selected,2);body.number(members.size(),1);
            for(const auto* p:members){body.number(p->id,4);body.text(p->name);body.number(p->last_intent,8);}
            Reader reader{body.bytes};print_state("server",snapshot(reader,static_cast<std::uint16_t>(content.clips.size())));
        }
        for(auto& p:peers)if(p->ready&&!p->dead&&!p->closing)try{p->queue(Message::state,body);}catch(const std::exception& e){reject(*p,e.what());}
    };
    while(!shutdown||!peers.empty()) {
        const auto now=Clock::now();
        if(!shutdown&&(interrupted||milliseconds(now,started)>=args.run_ms)) {
            shutdown=true;shutdown_since=now;listener.close();
            for(auto& p:peers){p->closing=true;p->closing_since=now;p->reason="server_shutdown";try{p->queue(Message::bye);}catch(...){p->dead=true;}}
        }
        std::vector<PollItem> items;if(!shutdown)items.push_back({&listener,false});
        const std::size_t offset=items.size(),existing=peers.size();
        for(const auto& peer:peers)items.push_back({&peer->socket,!peer->output.empty()});
        if(!items.empty())poll(items,20);
        if(!shutdown&&items[0].readable)for(int accepted=0;accepted<8;++accepted){
            auto socket=listener.accept();if(!socket.valid())break;
            if(peers.size()>=8){std::cout<<"REJECT id=0 reason=pending_capacity\n";continue;}
            peers.push_back(std::make_unique<ServerPeer>(std::move(socket),version));
        }
        for(std::size_t i=0;i<existing;++i){auto& peer=*peers[i];if(peer.dead)continue;
            try {
                if(!peer.closing&&items[offset+i].readable)peer.read();
                if(!peer.closing)for(int processed=0;processed<32;++processed){
                    auto message=peer.next(now);if(!message)break;Reader body{message->payload};
                    if(!peer.ready){
                        if(message->type!=Message::hello)throw std::runtime_error("hello_required");
                        const auto digest=body.raw(32);const auto name=body.text();body.finish();
                        if(!std::equal(digest.begin(),digest.end(),content.digest.begin()))throw std::runtime_error("content_mismatch");
                        if(!valid_name(name))throw std::runtime_error("invalid_name");
                        unsigned active=0;for(const auto& p:peers)if(p->ready&&!p->dead)++active;
                        if(active>=4)throw std::runtime_error("session_full");
                        if(next_id==std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("participant_id_overflow");
                        peer.id=next_id++;peer.name=name;peer.ready=true;if(world)peer.actor=world->join();
                        Writer welcome;welcome.number(peer.id,4);welcome.number(content.clips.size(),2);welcome.number(session,8);welcome.raw(content.digest);if(world){welcome.number(world_tick_ms,2);welcome.number(world_hold_ms,2);}peer.queue(Message::welcome,welcome);
                        std::cout<<"JOIN id="<<peer.id<<" name="<<peer.name<<'\n';advance();broadcast();
                    } else if(message->type==Message::intent){
                        const auto sequence=body.number(8);const auto animation=static_cast<std::uint16_t>(body.number(2));body.finish();
                        if(peer.last_intent==std::numeric_limits<std::uint64_t>::max()||sequence!=peer.last_intent+1)throw std::runtime_error("intent_sequence");
                        if(animation!=0xffff&&animation>=content.clips.size())throw std::runtime_error("invalid_selection");
                        peer.last_intent=sequence;selected=animation;advance();
                        std::cout<<"INTENT revision="<<revision<<" id="<<peer.id<<" sequence="<<sequence<<" selected="<<selected<<'\n';broadcast();
                    } else if(world&&message->type==Message::motion) {
                        const auto sequence=body.number(8);const auto input=read_motion(body);body.finish();
                        world->accept(*peer.actor,sequence,input,now);
                        std::cout<<"MOTION id="<<peer.id<<" sequence="<<sequence<<" tick="<<tick<<" x="<<input.x<<" z="<<input.z<<" turn="<<input.turn<<'\n';
                    } else if(message->type==Message::ping){Writer pong;pong.number(body.number(8),8);body.finish();peer.queue(Message::pong,pong);}
                    else if(message->type==Message::bye){body.finish();peer.closing=true;peer.closing_since=now;peer.reason="client_bye";peer.queue(Message::bye);}
                    else throw std::runtime_error("message_not_allowed");
                    peer.last=now;if(peer.closing)break;
                }
                if(!peer.closing&&!peer.ready&&milliseconds(now,peer.connected)>args.handshake_ms)throw std::runtime_error("handshake_timeout");
                if(!peer.closing&&peer.ready&&milliseconds(now,peer.last)>args.idle_ms)throw std::runtime_error("idle_timeout");
                if(!peer.closing&&peer.partial_since!=Time{}&&milliseconds(now,peer.partial_since)>args.handshake_ms)throw std::runtime_error("partial_frame_timeout");
                if(items[offset+i].writable)peer.flush();
            } catch(const std::exception& e){
                const std::string reason=e.what();
                if(peer.closing||reason=="peer_eof"||reason.starts_with("recv_")||reason.starts_with("send_")) {peer.dead=true;if(peer.reason.empty())peer.reason=reason;}
                else reject(peer,reason);
            }
            if(peer.closing&&(peer.output.empty()||milliseconds(now,peer.closing_since)>100))peer.dead=true;
            if(shutdown&&milliseconds(now,shutdown_since)>150)peer.dead=true;
        }
        bool departed=false;
        for(auto& peer:peers)if(peer->dead){std::cout<<"LEAVE id="<<peer->id<<" reason="<<peer->reason<<'\n';if(peer->ready){peer->ready=false;departed=true;}peer->socket.close();}
        std::erase_if(peers,[](const auto& peer){return peer->dead;});
        if(departed&&!shutdown){advance();broadcast();}
        if(world&&!shutdown) {
            const auto current=Clock::now();unsigned catchup=0;
            while(current>=next_tick&&catchup<4) {
                if(tick==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("tick_overflow");++tick;++catchup;
                bool active=false;
                for(auto& p:peers)if(p->ready&&!p->dead&&!p->closing){world->tick(*p->actor,current);active=true;}
                next_tick+=std::chrono::milliseconds(world_tick_ms);
                if(active){advance();broadcast();}
            }
            if(current>=next_tick){next_tick=current+std::chrono::milliseconds(world_tick_ms);std::cout<<"WORLD_LAG discarded_wall_time=1 tick="<<tick<<'\n';}
        }
    }
    std::cout<<"STOP peers=0 sockets=0 revision="<<revision<<'\n';return 0;
}
int client(const Options& args,const Content& content) {
    if(args.select&&args.selection!=0xffff&&args.selection>=content.clips.size())throw std::runtime_error("local_selection_range");
    Peer peer(Socket::connect_loopback(args.port,2000));Writer hello;hello.raw(content.digest);hello.text(args.name);peer.queue(Message::hello,hello);
    bool sent=false,stopping=false,bye=false,confirmed_bye=false,have_state=false;std::uint64_t revision=0,ping=0;Time joined{},last_ping{},end{};
    std::uint32_t id=0;std::uint16_t clips=0;
    while(true){const auto now=Clock::now();
        if(interrupted&&!stopping){stopping=true;end=now;}
        std::vector<PollItem> items{{&peer.socket,!peer.output.empty()}};poll(items,20);
        if(items[0].writable)peer.flush();
        if(items[0].readable)peer.read();
        for(int n=0;n<32;++n){auto message=peer.next(now);if(!message)break;Reader body{message->payload};
            if(message->type==Message::reject){const auto reason=body.text();body.finish();std::cout<<"REJECTED reason="<<reason<<'\n';return 2;}
            if(!peer.ready){
                if(message->type!=Message::welcome)throw std::runtime_error("welcome_required");
                id=static_cast<std::uint32_t>(body.number(4));clips=static_cast<std::uint16_t>(body.number(2));const auto session=body.number(8);const auto digest=body.raw(32);body.finish();
                if(!id||clips!=content.clips.size()||!std::equal(digest.begin(),digest.end(),content.digest.begin()))throw std::runtime_error("welcome_identity");
                peer.ready=true;joined=now;std::cout<<"WELCOME id="<<id<<" name="<<args.name<<" session="<<session<<'\n';
            } else if(message->type==Message::state){
                const auto state=snapshot(body,clips);if(have_state&&state.revision<=revision)throw std::runtime_error("state_order");
                bool member=false;for(const auto& p:state.participants)if(p.id==id){if(p.name!=args.name||p.acknowledged>1)throw std::runtime_error("state_identity");member=true;}
                if(!member)throw std::runtime_error("self_missing_from_state");have_state=true;revision=state.revision;print_state(args.name.c_str(),state);
            } else if(message->type==Message::pong){const auto received=body.number(8);body.finish();if(!received||received>ping)throw std::runtime_error("invalid_pong");}
            else if(message->type==Message::bye){body.finish();confirmed_bye=true;break;}
            else throw std::runtime_error("unexpected_server_message");
            peer.last=now;
        }
        if(confirmed_bye)break;
        if(!peer.ready&&milliseconds(now,peer.connected)>args.handshake_ms)throw std::runtime_error("handshake_timeout");
        if(peer.ready&&milliseconds(now,peer.last)>args.idle_ms)throw std::runtime_error("server_timeout");
        if(peer.partial_since!=Time{}&&milliseconds(now,peer.partial_since)>args.handshake_ms)throw std::runtime_error("partial_frame_timeout");
        if(peer.ready&&!stopping&&milliseconds(now,joined)>=args.run_ms){stopping=true;end=now;}
        if(peer.ready&&stopping&&!bye){peer.queue(Message::bye);bye=true;}
        if(peer.ready&&!stopping){
            if(args.select&&!sent&&milliseconds(now,joined)>=args.select_after_ms){Writer intent;intent.number(1,8);intent.number(args.selection,2);peer.queue(Message::intent,intent);sent=true;}
            if(last_ping==Time{}||milliseconds(now,last_ping)>=250){Writer heartbeat;heartbeat.number(++ping,8);peer.queue(Message::ping,heartbeat);last_ping=now;}
        }
        if(stopping&&milliseconds(now,end)>500)throw std::runtime_error("bye_timeout");
    }
    peer.socket.close();std::cout<<"CLIENT_STOP id="<<id<<" confirmed="<<confirmed_bye<<" sockets=0\n";return 0;
}
int world_client(const Options& args,const Content& content) {
    WorldClient client(args.port,args.name,content,args.handshake_ms,args.idle_ms);
    Time joined{},last_motion{};std::uint64_t revision=0;bool selected=false,stopped_motion=false;
    while(!client.done()) {
        client.pump();const auto now=Clock::now();
        if(client.ready()&&joined==Time{}) {joined=now;std::cout<<"WELCOME id="<<client.id()<<" name="<<args.name<<" session="<<client.session()<<'\n';}
        if(client.snapshot()&&client.snapshot()->revision!=revision){revision=client.snapshot()->revision;print_world(args.name.c_str(),*client.snapshot());}
        if(interrupted||(joined!=Time{}&&milliseconds(now,joined)>=args.run_ms))client.close();
        if(client.ready()) {
            const auto elapsed=milliseconds(now,joined);
            if(args.select&&!selected&&elapsed>=args.select_after_ms){client.select(args.selection);selected=true;}
            if(args.move&&elapsed>=args.move_after_ms) {
                if(elapsed<args.move_after_ms+args.move_ms&&(last_motion==Time{}||milliseconds(now,last_motion)>=100)) {
                    client.motion(args.motion);last_motion=now;
                } else if(elapsed>=args.move_after_ms+args.move_ms&&!stopped_motion) {client.motion({});stopped_motion=true;}
            }
        }
        if(!client.done())std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if(!client.error().empty()){std::cout<<"CLIENT_ERROR reason="<<client.error()<<'\n';return 2;}
    std::cout<<"CLIENT_STOP id="<<client.id()<<" confirmed="<<client.confirmed_bye()<<" sockets=0\n";return 0;
}

}
int main(int argc,char** argv) {
    try{
        std::cout.setf(std::ios::unitbuf);std::cout<<std::setprecision(17);std::signal(SIGINT,signal_stop);std::signal(SIGTERM,signal_stop);
        const auto args=options(argc,argv);std::unique_ptr<NetworkWorld> world;
        if(!args.world.empty())world=std::make_unique<NetworkWorld>(args.world);
        const auto content=world?world->identity.content:mh4u::net::verify_manifest(args.manifest);
        if(world)std::cout<<"WORLD verified="<<content.hex<<" files="<<world->identity.file_count<<" bytes="<<world->identity.byte_count<<" protocol=2 tick_ms=50 hold_ms=250\n";
        std::cout<<"CONTENT verified="<<content.hex<<" clips="<<content.clips.size()<<'\n';
        if(args.mode=="verify")return 0;SocketSystem system;return args.mode=="server"?server(args,content,world.get()):(world?world_client(args,content):client(args,content));
    }catch(const std::exception& error){std::cerr<<"ERROR "<<error.what()<<'\n';return 1;}
}
