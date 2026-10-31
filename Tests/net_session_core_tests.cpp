#include "NetSessionCore.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using namespace ImperialNet;

int Checks=0;
int Failures=0;

void Expect(bool condition,const char* name)
{
    ++Checks;
    if(!condition)
    {
        ++Failures;
        std::cerr<<"FAIL "<<name<<'\n';
    }
}

InputPacket MakePacket(int seat,std::uint64_t sequence,double clientTimeS,double roll=0.0,bool armDown=false,bool disarmDown=false)
{
    InputPacket packet{};
    packet.SeatIndex=seat;
    packet.Sequence=sequence;
    packet.ClientTimeS=clientTimeS;
    packet.Command.RollNormalized=roll;
    packet.Command.PitchNormalized=-0.25;
    packet.Command.YawNormalized=0.50;
    packet.Command.ThrottleNormalized=0.60;
    packet.ArmButtonDown=armDown;
    packet.DisarmButtonDown=disarmDown;
    return packet;
}

bool Snapshot(const AuthoritativeSession& session,int seat,SeatSnapshot& snapshot)
{
    return session.GetSeatSnapshot(seat,snapshot)==Rejection::None;
}

void TestConfigurationAndOwnership()
{
    AuthoritativeSession session;
    SessionConfig invalid=session.GetConfig();
    invalid.InputTimeoutS=std::numeric_limits<double>::quiet_NaN();
    Expect(session.Configure(invalid)==Rejection::ConfigurationInvalid,"nonfinite configuration is rejected");
    Expect(session.GetConfig().InputTimeoutS==0.250,"invalid configuration preserves default safety timeout");
    invalid=session.GetConfig();
    invalid.MaximumClientFutureS=-0.001;
    Expect(session.Configure(invalid)==Rejection::ConfigurationInvalid,"negative future window is rejected");

    SessionConfig config{};
    config.InputTimeoutS=0.200;
    config.MaximumClientFutureS=0.100;
    config.MaximumClientPastS=0.500;
    Expect(session.Configure(config)==Rejection::None,"valid simulator-selected configuration is accepted");
    Expect(session.JoinClient(0U,ClientRole::Player)==Rejection::InvalidClientId,"zero client id is rejected");
    Expect(session.JoinClient(1U,static_cast<ClientRole>(99))==Rejection::InvalidRole,"invalid client role is rejected");
    Expect(session.JoinClient(1U,ClientRole::Player)==Rejection::None,"player joins");
    Expect(session.JoinClient(2U,ClientRole::Player)==Rejection::None,"second player joins");
    Expect(session.JoinClient(3U,ClientRole::Referee)==Rejection::None,"referee joins");
    Expect(session.JoinClient(4U,ClientRole::Administrator)==Rejection::None,"administrator joins");
    Expect(session.JoinClient(1U,ClientRole::Player)==Rejection::DuplicateClient,"duplicate client join is rejected");
    Expect(session.RequestSeatOwnership(1U,-1)==Rejection::InvalidSeat,"negative seat index is rejected");
    Expect(session.RequestSeatOwnership(1U,10)==Rejection::InvalidSeat,"seat index beyond ten aircraft is rejected");
    Expect(session.RequestSeatOwnership(3U,0)==Rejection::RoleCannotOwnSeat,"referee cannot claim pilot seat");
    Expect(session.RequestSeatOwnership(1U,0)==Rejection::None,"first player claims one aircraft");

    SeatSnapshot snapshot{};
    Expect(Snapshot(session,0,snapshot)&&snapshot.Owner==1U&&snapshot.OwnerConnected&&
        snapshot.State==SeatControlState::AwaitingInput&&!snapshot.Armed,"claimed seat awaits an explicit fresh arm press");
    Expect(session.RequestSeatOwnership(2U,0)==Rejection::SeatAlreadyOwned,"second player cannot steal aircraft");
    Expect(session.ReleaseSeatOwnership(2U,0)==Rejection::NotOwner,"non-owner cannot release another player seat");
    Expect(session.ReleaseSeatOwnership(3U,0)==Rejection::None,"referee can release a player seat");
    Expect(Snapshot(session,0,snapshot)&&snapshot.Owner==0U&&snapshot.State==SeatControlState::Unowned,"referee release clears ownership and arm state");
    Expect(session.RequestSeatOwnership(2U,0)==Rejection::None,"released aircraft may be claimed by another player");
    Expect(session.ReleaseSeatOwnership(4U,0)==Rejection::None,"administrator can release aircraft");
}

void TestInputValidationAndFailsafe()
{
    AuthoritativeSession session;
    SessionConfig config{};
    config.InputTimeoutS=0.200;
    config.MaximumClientFutureS=0.100;
    config.MaximumClientPastS=0.500;
    Expect(session.Configure(config)==Rejection::None,"input test configuration accepted");
    Expect(session.JoinClient(10U,ClientRole::Player)==Rejection::None,"owner joins for input test");
    Expect(session.JoinClient(11U,ClientRole::Player)==Rejection::None,"intruder joins for input test");
    Expect(session.RequestSeatOwnership(10U,1)==Rejection::None,"owner claims input test seat");

    InputPacket packet=MakePacket(1,1U,0.0,0.25,false);
    Expect(session.SubmitInput(10U,packet,0.0)==Rejection::None,"first neutral arm-button release input is accepted");
    packet=MakePacket(1,2U,0.050,0.25,true);
    Expect(session.SubmitInput(10U,packet,0.050)==Rejection::None,"fresh arm press input is accepted");
    SeatSnapshot snapshot{};
    Expect(Snapshot(session,1,snapshot)&&snapshot.Armed&&snapshot.LastAcceptedSequence==2U&&
        snapshot.AppliedCommand.RollNormalized==0.25,"accepted owner input and fresh arm edge are recorded");
    const InputPacket unauthorized=MakePacket(1,3U,0.060,0.99,false);
    Expect(session.SubmitInput(11U,unauthorized,0.060)==Rejection::NotOwner,"non-owner input is rejected");
    Expect(Snapshot(session,1,snapshot)&&snapshot.LastAcceptedSequence==2U&&snapshot.Armed&&
        snapshot.AppliedCommand.RollNormalized==0.25,"rejected non-owner input never changes applied command or arm state");
    Expect(session.SubmitInput(10U,packet,0.060)==Rejection::DuplicateSequence,"duplicate sequence is rejected");
    packet.Sequence=1U;
    Expect(session.SubmitInput(10U,packet,0.070)==Rejection::StaleSequence,"older sequence is rejected");
    packet=MakePacket(1,3U,0.500,0.25,false);
    Expect(session.SubmitInput(10U,packet,0.080)==Rejection::TooFarInFuture,"client timestamp too far in future is rejected");
    packet=MakePacket(1,3U,0.000,0.25,false);
    Expect(session.SubmitInput(10U,packet,0.600)==Rejection::TooFarInPast,"client timestamp too far in past is rejected");
    packet=MakePacket(1,3U,0.610,0.25,false);
    packet.Command.RollNormalized=std::numeric_limits<double>::quiet_NaN();
    Expect(session.SubmitInput(10U,packet,0.610)==Rejection::NonFiniteValue,"nonfinite normalized input is rejected");
    packet=MakePacket(1,3U,0.610,0.25,false);
    packet.Command.ThrottleNormalized=1.001;
    Expect(session.SubmitInput(10U,packet,0.610)==Rejection::ValueOutOfRange,"out-of-range throttle is rejected");
    packet=MakePacket(1,0U,0.610,0.25,false);
    Expect(session.SubmitInput(10U,packet,0.610)==Rejection::InvalidSequence,"zero sequence is rejected");
    Expect(session.Tick(0.610)==Rejection::None,"server time can advance after rejected input");
    Expect(session.Tick(0.610)==Rejection::None,"equal server timestamp is deterministic");
    Expect(session.Tick(0.609)==Rejection::ServerTimeWentBackward,"server time cannot move backward");
    Expect(session.Tick(0.250)==Rejection::ServerTimeWentBackward,"late tick remains rejected after monotonic advance");

    AuthoritativeSession timeoutSession;
    Expect(timeoutSession.Configure(config)==Rejection::None,"timeout configuration accepted");
    Expect(timeoutSession.JoinClient(20U,ClientRole::Player)==Rejection::None,"timeout owner joins");
    Expect(timeoutSession.RequestSeatOwnership(20U,2)==Rejection::None,"timeout owner claims seat");
    Expect(timeoutSession.SubmitInput(20U,MakePacket(2,1U,0.0,0.40,false),0.0)==Rejection::None,"timeout setup arm release accepted");
    Expect(timeoutSession.SubmitInput(20U,MakePacket(2,2U,0.010,0.40,true),0.010)==Rejection::None,"timeout setup arms seat");
    Expect(timeoutSession.Tick(0.211)==Rejection::None,"timeout tick executes");
    Expect(Snapshot(timeoutSession,2,snapshot)&&snapshot.State==SeatControlState::InputLost&&!snapshot.Armed&&
        snapshot.AppliedCommand.ThrottleNormalized==0.0,"timeout neutralizes and disarms with no automatic re-arm");
    Expect(timeoutSession.SubmitInput(20U,MakePacket(2,3U,0.220,0.10,true),0.220)==Rejection::None,"post-timeout held arm packet can be accepted as control data");
    Expect(Snapshot(timeoutSession,2,snapshot)&&!snapshot.Armed&&snapshot.State==SeatControlState::ReceivingInput,
        "held arm packet after timeout does not re-arm");
    Expect(timeoutSession.SubmitInput(20U,MakePacket(2,4U,0.230,0.10,false),0.230)==Rejection::None,"post-timeout arm release accepted");
    Expect(timeoutSession.SubmitInput(20U,MakePacket(2,5U,0.240,0.10,true),0.240)==Rejection::None,"post-timeout new arm press accepted");
    Expect(Snapshot(timeoutSession,2,snapshot)&&snapshot.Armed,"only a fresh press re-arms after timeout");

    AuthoritativeSession holdSession;
    config.TimeoutFailsafe=FailsafePolicy::HoldLastAndDisarm;
    Expect(holdSession.Configure(config)==Rejection::None,"hold-last failsafe configuration accepted");
    Expect(holdSession.JoinClient(21U,ClientRole::Player)==Rejection::None&&holdSession.RequestSeatOwnership(21U,3)==Rejection::None,
        "hold-last owner joins and claims");
    Expect(holdSession.SubmitInput(21U,MakePacket(3,1U,0.0,-0.70,false),0.0)==Rejection::None&&
        holdSession.SubmitInput(21U,MakePacket(3,2U,0.010,-0.70,true),0.010)==Rejection::None,"hold-last setup input accepted");
    Expect(holdSession.Tick(0.220)==Rejection::None,"hold-last timeout tick executes");
    Expect(Snapshot(holdSession,3,snapshot)&&snapshot.State==SeatControlState::InputLost&&!snapshot.Armed&&
        snapshot.AppliedCommand.RollNormalized==-0.70,"hold-last policy preserves command but still disarms");
}

void TestReconnectAndEventLog()
{
    AuthoritativeSession session;
    Expect(session.JoinClient(30U,ClientRole::Player)==Rejection::None,"reconnect player joins");
    Expect(session.JoinClient(31U,ClientRole::Player)==Rejection::None,"other player joins");
    Expect(session.JoinClient(32U,ClientRole::Administrator)==Rejection::None,"administrator joins reconnect test");
    Expect(session.RequestSeatOwnership(30U,4)==Rejection::None,"reconnect player claims seat");
    Expect(session.SubmitInput(30U,MakePacket(4,1U,0.0,0.20,false),0.0)==Rejection::None&&
        session.SubmitInput(30U,MakePacket(4,2U,0.010,0.20,true),0.010)==Rejection::None,"reconnect seat is armed before disconnect");
    Expect(session.LeaveClient(30U,0.020)==Rejection::None,"disconnect is accepted and logged by server");
    SeatSnapshot snapshot{};
    Expect(Snapshot(session,4,snapshot)&&snapshot.Owner==30U&&!snapshot.OwnerConnected&&snapshot.ReclaimRequired&&
        snapshot.State==SeatControlState::ReservedForReconnect&&!snapshot.Armed,"disconnect reserves ownership but disarms and requires reclaim");
    Expect(session.RequestSeatOwnership(31U,4)==Rejection::OwnershipReservedForReconnect,"disconnected seat cannot be stolen");
    Expect(session.JoinClient(30U,ClientRole::Player)==Rejection::DuplicateClient,"reconnect must use explicit reconnect operation");
    Expect(session.ReconnectClient(30U,0.030)==Rejection::None,"known disconnected player reconnects");
    Expect(session.SubmitInput(30U,MakePacket(4,3U,0.030,0.20,false),0.030)==Rejection::OwnershipReclaimRequired,
        "reconnected client cannot control before explicit reclaim");
    Expect(session.ReclaimSeatOwnership(30U,4)==Rejection::None,"server approves explicit ownership reclaim");
    Expect(Snapshot(session,4,snapshot)&&snapshot.OwnerConnected&&!snapshot.ReclaimRequired&&!snapshot.Armed&&
        snapshot.State==SeatControlState::AwaitingInput,"reclaim restores ownership only and never restores arm state");
    Expect(session.SubmitInput(30U,MakePacket(4,3U,0.040,0.20,true),0.040)==Rejection::None,"held arm state packet arrives after reclaim");
    Expect(Snapshot(session,4,snapshot)&&!snapshot.Armed,"held arm state cannot auto-arm after reconnect");
    Expect(session.SubmitInput(30U,MakePacket(4,4U,0.050,0.20,false),0.050)==Rejection::None&&
        session.SubmitInput(30U,MakePacket(4,5U,0.060,0.20,true),0.060)==Rejection::None,"fresh release then press accepted after reconnect");
    Expect(Snapshot(session,4,snapshot)&&snapshot.Armed,"fresh arm press is required after reconnect");
    Expect(session.ReleaseSeatOwnership(32U,4)==Rejection::None,"administrator can release reconnect reservation or active owner");

    AuthoritativeSession events;
    Expect(events.JoinClient(40U,ClientRole::Player)==Rejection::None,"event source client joins");
    SessionEvent goal{};
    SessionEvent setEnd{};
    SessionEvent forfeit{};
    Expect(events.PublishServerEvent({EventType::Goal,0.100,0,0,0U},goal)==Rejection::None&&goal.Id==1U,"server publishes first goal event");
    Expect(events.PublishServerEvent({EventType::SetEnded,0.100,-1,-1,0U},setEnd)==Rejection::None&&setEnd.Id==2U,"server publishes ordered set-end event");
    Expect(events.PublishServerEvent({EventType::Forfeit,0.200,1,-1,40U},forfeit)==Rejection::None&&forfeit.Id==3U,
        "server publishes ordered forfeit event");
    Expect(events.PublishServerEvent({EventType::Goal,0.300,2,0,0U},goal)==Rejection::EventDataInvalid,"invalid authoritative event payload is rejected");
    EventCatchup catchup{};
    Expect(events.GetEventCatchup(40U,0U,catchup)==Rejection::None&&catchup.Count==3U&&
        catchup.Events[0U].Id==1U&&catchup.Events[2U].Type==EventType::Forfeit,"joined client receives monotonic catch-up log");
    Expect(events.AcknowledgeEvents(40U,2U)==Rejection::None&&events.GetClientAcknowledgement(40U)==2U,"client acknowledgement is recorded");
    Expect(events.AcknowledgeEvents(40U,1U)==Rejection::AcknowledgementWentBackward,"acknowledgement cannot move backward");
    Expect(events.AcknowledgeEvents(40U,4U)==Rejection::AcknowledgementBeyondLatest,"acknowledgement cannot exceed server event id");
    Expect(events.JoinClient(41U,ClientRole::Player)==Rejection::None,"late joining client joins");
    Expect(events.GetEventCatchup(41U,0U,catchup)==Rejection::None&&catchup.Count==3U&&catchup.LatestEventId==3U,
        "late joiner catches up complete retained log");

    AuthoritativeSession history;
    Expect(history.JoinClient(50U,ClientRole::Player)==Rejection::None,"history client joins");
    bool historyPublishSucceeded=true;
    for(std::size_t index=0U;index<kMaxEventHistory+2U;++index)
    {
        SessionEvent event{};
        const double eventTimeS=static_cast<double>(index)*0.001;
        if(history.PublishServerEvent({EventType::SetEnded,eventTimeS,-1,-1,0U},event)!=Rejection::None)historyPublishSucceeded=false;
    }
    Expect(historyPublishSucceeded&&history.GetLastEventId()==static_cast<EventId>(kMaxEventHistory+2U),"bounded history keeps monotonically assigned event ids");
    Expect(history.GetEventCatchup(50U,0U,catchup)==Rejection::EventHistoryExpired,"catch-up explicitly reports expired bounded history");
    const EventId recentStart=history.GetLastEventId()-2U;
    Expect(history.GetEventCatchup(50U,recentStart,catchup)==Rejection::None&&catchup.Count==2U&&
        catchup.Events[0U].Id==recentStart+1U,"recent retained event range catches up in order");
}

struct DeterministicPrng
{
    std::uint32_t State=0U;
    std::uint32_t Next()
    {
        State=State*1664525U+1013904223U;
        return State;
    }
    bool Drop(std::uint32_t lossPercent)
    {
        return Next()%100U<lossPercent;
    }
};

struct Replica
{
    EventId LastEventId=0U;
    std::array<EventId,kMaxEventHistory> EventIds{};
    std::size_t Count=0U;
    bool Ordered=true;
};

struct ScheduledDelivery
{
    std::uint32_t DueMs=0U;
    std::size_t ReplicaIndex=0U;
    EventCatchup Catchup{};
};

void ApplyDelivery(Replica& replica,const EventCatchup& catchup)
{
    for(std::size_t index=0U;index<catchup.Count;++index)
    {
        const EventId id=catchup.Events[index].Id;
        if(id<=replica.LastEventId)continue;
        if(id!=replica.LastEventId+1U||replica.Count>=replica.EventIds.size())
        {
            replica.Ordered=false;
            continue;
        }
        replica.EventIds[replica.Count]=id;
        ++replica.Count;
        replica.LastEventId=id;
    }
}

bool ReplicaMatchesServerLog(const Replica& replica,EventId lastId)
{
    if(!replica.Ordered||replica.LastEventId!=lastId||replica.Count!=static_cast<std::size_t>(lastId))return false;
    for(std::size_t index=0U;index<replica.Count;++index)
    {
        if(replica.EventIds[index]!=static_cast<EventId>(index+1U))return false;
    }
    return true;
}

void TestDeterministicNetworkConvergence()
{
    const std::array<std::uint32_t,3U> rttValuesMs{{50U,100U,150U}};
    const std::array<std::uint32_t,3U> lossValuesPercent{{1U,3U,5U}};
    for(const std::uint32_t rttMs:rttValuesMs)
    {
        for(const std::uint32_t lossPercent:lossValuesPercent)
        {
        AuthoritativeSession session;
        SessionConfig config{};
        config.InputTimeoutS=0.500;
        config.MaximumClientFutureS=0.300;
        config.MaximumClientPastS=1.000;
        Expect(session.Configure(config)==Rejection::None,"deterministic network configuration accepted");
        Expect(session.JoinClient(60U,ClientRole::Player)==Rejection::None&&session.JoinClient(61U,ClientRole::Player)==Rejection::None&&
            session.JoinClient(62U,ClientRole::Player)==Rejection::None,"network clients join authoritative session");
        Expect(session.RequestSeatOwnership(60U,0)==Rejection::None,"network owner claims aircraft");
        Expect(session.SubmitInput(60U,MakePacket(0,1U,0.0,0.15,false),0.0)==Rejection::None,"authorized network input is applied");
        const InputPacket attack=MakePacket(0,2U,0.010,-0.95,false);
        Expect(session.SubmitInput(61U,attack,0.010)==Rejection::NotOwner,"unauthorized network input is rejected under loss scenario");
        SeatSnapshot snapshot{};
        Expect(Snapshot(session,0,snapshot)&&snapshot.LastAcceptedSequence==1U&&snapshot.AppliedCommand.RollNormalized==0.15,
            "unauthorized network input is never applied");

        bool eventPublishSucceeded=true;
        for(std::size_t index=0U;index<9U;++index)
        {
            SessionEvent event{};
            const EventType type=index%3U==0U?EventType::Goal:(index%3U==1U?EventType::SetEnded:EventType::Forfeit);
            ServerEventRequest request{};
            request.Type=type;
            request.ServerTimeS=0.020+static_cast<double>(index)*0.010;
            request.TeamIndex=type==EventType::Goal||type==EventType::Forfeit?static_cast<int>(index%2U):-1;
            request.SeatIndex=type==EventType::Goal?static_cast<int>(index%kAircraftSeatCount):-1;
            request.SubjectClient=type==EventType::Forfeit?60U:0U;
            if(session.PublishServerEvent(request,event)!=Rejection::None)eventPublishSucceeded=false;
        }
        Expect(eventPublishSucceeded&&session.GetLastEventId()==9U,"server emits deterministic authoritative event log");

        std::array<Replica,3U> replicas{};
        const std::array<ClientId,3U> clientIds{{60U,61U,62U}};
        std::vector<ScheduledDelivery> deliveries;
        DeterministicPrng random{static_cast<std::uint32_t>(rttMs*17U+lossPercent)};
        const std::uint32_t halfRttMs=rttMs/2U;
        for(std::uint32_t nowMs=0U;nowMs<=2500U;nowMs+=20U)
        {
            for(std::size_t replicaIndex=0U;replicaIndex<replicas.size();++replicaIndex)
            {
                EventCatchup catchup{};
                const Rejection catchupResult=session.GetEventCatchup(clientIds[replicaIndex],replicas[replicaIndex].LastEventId,catchup);
                if(catchupResult==Rejection::None&&catchup.Count>0U&&!random.Drop(lossPercent))
                {
                    deliveries.push_back({nowMs+halfRttMs,replicaIndex,catchup});
                }
            }
            std::vector<ScheduledDelivery> pending;
            pending.reserve(deliveries.size());
            for(const ScheduledDelivery& delivery:deliveries)
            {
                if(delivery.DueMs<=nowMs)
                {
                    ApplyDelivery(replicas[delivery.ReplicaIndex],delivery.Catchup);
                    const Rejection acknowledgement=session.AcknowledgeEvents(clientIds[delivery.ReplicaIndex],replicas[delivery.ReplicaIndex].LastEventId);
                    if(acknowledgement!=Rejection::None)replicas[delivery.ReplicaIndex].Ordered=false;
                }
                else pending.push_back(delivery);
            }
            deliveries.swap(pending);
        }
        bool allConverged=deliveries.empty();
        for(std::size_t replicaIndex=0U;replicaIndex<replicas.size();++replicaIndex)
        {
            allConverged=allConverged&&ReplicaMatchesServerLog(replicas[replicaIndex],session.GetLastEventId())&&
                session.GetClientAcknowledgement(clientIds[replicaIndex])==session.GetLastEventId();
        }
        Expect(allConverged,"all clients converge to the same event log at configured RTT and loss");
        }
    }
}
}

int main()
{
    std::cout<<"Pure C++ authoritative-session tests; no Unreal build, sockets, real LAN/Internet, NAT, server deployment, other PC, physical controller, or human-playtest verification.\n";
    TestConfigurationAndOwnership();
    TestInputValidationAndFailsafe();
    TestReconnectAndEventLog();
    TestDeterministicNetworkConvergence();
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0?0:1;
}
