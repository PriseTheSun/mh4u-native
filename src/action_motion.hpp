#pragma once
#include "model_motion.hpp"
#include "motion_events.hpp"
#include "root_motion.hpp"

namespace mh4u::action_motion {
// This policy is authored for inspection. It does not recover the game's
// action controller, gameplay event consumption, transitions, speed or initial flags.
struct DiagnosticPolicy {
    float initial_frame,source_frames_per_tick;
    bool repeat;
    root_motion::Model initial_model;
};
struct Frame {
    root_motion::TickState state;
    float joint_source_frame;
    std::uint32_t loop_iterations;
};
class Player {
public:
    // Immutable packages must outlive this player. Event tables are optional;
    // their identity and frame count must match the selected model motion.
    Player(const model_motion::Package&,const model_motion::ActionIdentity&,const DiagnosticPolicy&,
           const motion_events::Package* events=nullptr);
    const Frame& current() const { return frame_; }
    const model_motion::ActionInfo& action() const { return action_; }
    const root_motion::AdvanceParameters& parameters() const { return parameters_; }
    bool events_bound() const { return events_!=nullptr; }
    Frame tick();
private:
    const model_motion::Package* package_;
    const motion_events::Package* events_;
    motion_events::ActionIdentity event_identity_;
    root_motion::PreUpdateScope scope_{true,0,{false,false,false,false}};
    model_motion::ActionInfo action_;
    root_motion::AdvanceParameters parameters_;
    Frame frame_;
    root_motion::Sample sample(float source_frame) const;
    Frame calculate_next() const;
};
} // namespace mh4u::action_motion
