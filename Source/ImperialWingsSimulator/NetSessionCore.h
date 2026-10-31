#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Transport-agnostic, server-authoritative session state. This core opens no
// sockets and never trusts client-side ownership, time, score, or event ids.
namespace ImperialNet
{
constexpr std::size_t kAircraftSeatCount=10U;
constexpr std::size_t kMaxClientCount=16U;
constexpr std::size_t kMaxEventHistory=128U;
constexpr double kMaximumSessionTimeS=1000000000.0;

using ClientId=std::uint32_t;
using EventId=std::uint64_t;

enum class ClientRole { Player, Referee, Administrator };
enum class FailsafePolicy { NeutralAndDisarm, HoldLastAndDisarm };
enum class SeatControlState { Unowned, AwaitingInput, ReceivingInput, InputLost, ReservedForReconnect };
enum class EventType { Goal, SetEnded, Forfeit, Disconnect };

enum class Rejection
{
    None,
    ConfigurationInvalid,
    InvalidClientId,
    InvalidRole,
    DuplicateClient,
    AlreadyConnected,
    ClientCapacityReached,
    UnknownClient,
    ClientNotConnected,
    InvalidSeat,
    RoleCannotOwnSeat,
    SeatUnowned,
    SeatAlreadyOwned,
    OwnershipReservedForReconnect,
    NotOwner,
    InsufficientAuthority,
    OwnershipReclaimRequired,
    InvalidSequence,
    StaleSequence,
    DuplicateSequence,
    NonFiniteValue,
    ValueOutOfRange,
    ServerTimeWentBackward,
    TooFarInPast,
    TooFarInFuture,
    EventTypeInvalid,
    EventDataInvalid,
    EventIdExhausted,
    AcknowledgementWentBackward,
    AcknowledgementBeyondLatest,
    EventHistoryExpired
};

struct SessionConfig
{
    // All are simulator-selected safety bounds, not a measured network profile.
    double InputTimeoutS=0.250;
    double MaximumClientFutureS=0.250;
    double MaximumClientPastS=2.000;
    FailsafePolicy TimeoutFailsafe=FailsafePolicy::NeutralAndDisarm;
};

struct ControlCommand
{
    double RollNormalized=0.0;
    double PitchNormalized=0.0;
    double YawNormalized=0.0;
    double ThrottleNormalized=0.0;
};

struct InputPacket
{
    int SeatIndex=-1;
    std::uint64_t Sequence=0U;
    double ClientTimeS=0.0;
    ControlCommand Command{};
    bool ArmButtonDown=false;
    bool DisarmButtonDown=false;
};

struct SeatSnapshot
{
    ClientId Owner=0U;
    bool OwnerConnected=false;
    bool ReclaimRequired=false;
    bool Armed=false;
    bool HasAcceptedInput=false;
    SeatControlState State=SeatControlState::Unowned;
    ControlCommand AppliedCommand{};
    std::uint64_t LastAcceptedSequence=0U;
    double LastClientTimeS=0.0;
    double LastInputServerTimeS=0.0;
};

struct ServerEventRequest
{
    EventType Type=EventType::Goal;
    double ServerTimeS=0.0;
    int TeamIndex=-1;
    int SeatIndex=-1;
    ClientId SubjectClient=0U;
};

struct SessionEvent
{
    EventId Id=0U;
    EventType Type=EventType::Goal;
    double ServerTimeS=0.0;
    int TeamIndex=-1;
    int SeatIndex=-1;
    ClientId SubjectClient=0U;
};

struct EventCatchup
{
    std::array<SessionEvent,kMaxEventHistory> Events{};
    std::size_t Count=0U;
    EventId LatestEventId=0U;
};

class AuthoritativeSession
{
public:
    AuthoritativeSession()
    {
        ResetRuntimeState();
    }

    Rejection Configure(const SessionConfig& configuration)
    {
        if(!IsValidConfiguration(configuration))return Rejection::ConfigurationInvalid;
        Config=configuration;
        ResetRuntimeState();
        return Rejection::None;
    }

    void Reset()
    {
        Config=SessionConfig{};
        ResetRuntimeState();
    }

    Rejection JoinClient(ClientId clientId,ClientRole role)
    {
        if(clientId==0U)return Rejection::InvalidClientId;
        if(!IsValidRole(role))return Rejection::InvalidRole;
        if(FindClient(clientId)!=nullptr)return Rejection::DuplicateClient;
        for(ClientRecord& client:Clients)
        {
            if(!client.Known)
            {
                client.Known=true;
                client.Connected=true;
                client.Id=clientId;
                client.Role=role;
                client.LastAcknowledgedEventId=0U;
                return Rejection::None;
            }
        }
        return Rejection::ClientCapacityReached;
    }

    Rejection ReconnectClient(ClientId clientId,double serverTimeS)
    {
        const Rejection timeResult=AdvanceServerTime(serverTimeS);
        if(timeResult!=Rejection::None)return timeResult;
        ClientRecord* const client=FindClient(clientId);
        if(client==nullptr)return Rejection::UnknownClient;
        if(client->Connected)return Rejection::AlreadyConnected;
        client->Connected=true;
        return Rejection::None;
    }

    Rejection LeaveClient(ClientId clientId,double serverTimeS)
    {
        const Rejection timeResult=AdvanceServerTime(serverTimeS);
        if(timeResult!=Rejection::None)return timeResult;
        ClientRecord* const client=FindClient(clientId);
        if(client==nullptr)return Rejection::UnknownClient;
        if(!client->Connected)return Rejection::ClientNotConnected;
        client->Connected=false;
        for(SeatRecord& seat:Seats)
        {
            if(seat.Owner==clientId)
            {
                seat.OwnerConnected=false;
                seat.ReclaimRequired=true;
                ApplyFailsafe(seat);
                seat.State=SeatControlState::ReservedForReconnect;
            }
        }
        const ServerEventRequest disconnect{EventType::Disconnect,ServerTimeS,-1,-1,clientId};
        return AppendEventUnchecked(disconnect,nullptr)?Rejection::None:Rejection::EventIdExhausted;
    }

    Rejection RequestSeatOwnership(ClientId clientId,int seatIndex)
    {
        ClientRecord* const client=FindConnectedClient(clientId);
        if(client==nullptr)return ClientConnectionRejection(clientId);
        if(client->Role!=ClientRole::Player)return Rejection::RoleCannotOwnSeat;
        if(!IsValidSeat(seatIndex))return Rejection::InvalidSeat;
        SeatRecord& seat=Seats[static_cast<std::size_t>(seatIndex)];
        if(seat.Owner!=0U)
        {
            return seat.ReclaimRequired?Rejection::OwnershipReservedForReconnect:Rejection::SeatAlreadyOwned;
        }
        AssignSeatToOwner(seat,clientId);
        return Rejection::None;
    }

    Rejection ReclaimSeatOwnership(ClientId clientId,int seatIndex)
    {
        if(FindConnectedClient(clientId)==nullptr)return ClientConnectionRejection(clientId);
        if(!IsValidSeat(seatIndex))return Rejection::InvalidSeat;
        SeatRecord& seat=Seats[static_cast<std::size_t>(seatIndex)];
        if(seat.Owner==0U)return Rejection::SeatUnowned;
        if(seat.Owner!=clientId)return Rejection::NotOwner;
        if(!seat.ReclaimRequired)return Rejection::OwnershipReclaimRequired;
        seat.OwnerConnected=true;
        seat.ReclaimRequired=false;
        seat.Armed=false;
        seat.HasAcceptedInput=false;
        seat.State=SeatControlState::AwaitingInput;
        RequireFreshArmPress(seat);
        return Rejection::None;
    }

    Rejection ReleaseSeatOwnership(ClientId actorId,int seatIndex)
    {
        ClientRecord* const actor=FindConnectedClient(actorId);
        if(actor==nullptr)return ClientConnectionRejection(actorId);
        if(!IsValidSeat(seatIndex))return Rejection::InvalidSeat;
        SeatRecord& seat=Seats[static_cast<std::size_t>(seatIndex)];
        if(seat.Owner==0U)return Rejection::SeatUnowned;
        const bool privileged=actor->Role==ClientRole::Referee||actor->Role==ClientRole::Administrator;
        if(seat.Owner!=actorId&&!privileged)return Rejection::NotOwner;
        seat=SeatRecord{};
        return Rejection::None;
    }

    Rejection SubmitInput(ClientId clientId,const InputPacket& packet,double serverTimeS)
    {
        const Rejection packetResult=ValidateInputPacket(packet);
        if(packetResult!=Rejection::None)return packetResult;
        const Rejection timeResult=AdvanceServerTime(serverTimeS);
        if(timeResult!=Rejection::None)return timeResult;
        if(FindConnectedClient(clientId)==nullptr)return ClientConnectionRejection(clientId);
        SeatRecord& seat=Seats[static_cast<std::size_t>(packet.SeatIndex)];
        if(seat.Owner!=clientId)return Rejection::NotOwner;
        if(!seat.OwnerConnected||seat.ReclaimRequired)return Rejection::OwnershipReclaimRequired;
        if(packet.Sequence<seat.LastAcceptedSequence)return Rejection::StaleSequence;
        if(packet.Sequence==seat.LastAcceptedSequence)return Rejection::DuplicateSequence;
        if(packet.ClientTimeS>serverTimeS+Config.MaximumClientFutureS)return Rejection::TooFarInFuture;
        if(packet.ClientTimeS<serverTimeS-Config.MaximumClientPastS)return Rejection::TooFarInPast;
        seat.LastAcceptedSequence=packet.Sequence;
        seat.LastClientTimeS=packet.ClientTimeS;
        seat.LastInputServerTimeS=serverTimeS;
        seat.HasAcceptedInput=true;
        seat.State=SeatControlState::ReceivingInput;
        seat.AppliedCommand=packet.Command;
        ApplyArmEdges(seat,packet);
        return Rejection::None;
    }

    Rejection Tick(double serverTimeS)
    {
        return AdvanceServerTime(serverTimeS);
    }

    Rejection PublishServerEvent(const ServerEventRequest& request,SessionEvent& eventOut)
    {
        eventOut=SessionEvent{};
        if(!IsValidEventRequest(request))return Rejection::EventDataInvalid;
        const Rejection timeResult=AdvanceServerTime(request.ServerTimeS);
        if(timeResult!=Rejection::None)return timeResult;
        return AppendEventUnchecked(request,&eventOut)?Rejection::None:Rejection::EventIdExhausted;
    }

    Rejection AcknowledgeEvents(ClientId clientId,EventId acknowledgedId)
    {
        ClientRecord* const client=FindConnectedClient(clientId);
        if(client==nullptr)return ClientConnectionRejection(clientId);
        if(acknowledgedId>LastEventId)return Rejection::AcknowledgementBeyondLatest;
        if(acknowledgedId<client->LastAcknowledgedEventId)return Rejection::AcknowledgementWentBackward;
        client->LastAcknowledgedEventId=acknowledgedId;
        return Rejection::None;
    }

    Rejection GetEventCatchup(ClientId clientId,EventId afterId,EventCatchup& catchupOut) const
    {
        catchupOut=EventCatchup{};
        const ClientRecord* const client=FindClient(clientId);
        if(client==nullptr)return Rejection::UnknownClient;
        if(!client->Connected)return Rejection::ClientNotConnected;
        if(afterId>LastEventId)return Rejection::AcknowledgementBeyondLatest;
        catchupOut.LatestEventId=LastEventId;
        if(EventCount==0U)return Rejection::None;
        const EventId oldestId=EventLog[0U].Id;
        if(oldestId>1U&&afterId<oldestId-1U)return Rejection::EventHistoryExpired;
        for(std::size_t index=0U;index<EventCount;++index)
        {
            if(EventLog[index].Id>afterId)
            {
                catchupOut.Events[catchupOut.Count]=EventLog[index];
                ++catchupOut.Count;
            }
        }
        return Rejection::None;
    }

    Rejection GetSeatSnapshot(int seatIndex,SeatSnapshot& snapshotOut) const
    {
        snapshotOut=SeatSnapshot{};
        if(!IsValidSeat(seatIndex))return Rejection::InvalidSeat;
        const SeatRecord& seat=Seats[static_cast<std::size_t>(seatIndex)];
        snapshotOut.Owner=seat.Owner;
        snapshotOut.OwnerConnected=seat.OwnerConnected;
        snapshotOut.ReclaimRequired=seat.ReclaimRequired;
        snapshotOut.Armed=seat.Armed;
        snapshotOut.HasAcceptedInput=seat.HasAcceptedInput;
        snapshotOut.State=seat.State;
        snapshotOut.AppliedCommand=seat.AppliedCommand;
        snapshotOut.LastAcceptedSequence=seat.LastAcceptedSequence;
        snapshotOut.LastClientTimeS=seat.LastClientTimeS;
        snapshotOut.LastInputServerTimeS=seat.LastInputServerTimeS;
        return Rejection::None;
    }

    EventId GetLastEventId() const { return LastEventId; }
    EventId GetClientAcknowledgement(ClientId clientId) const
    {
        const ClientRecord* const client=FindClient(clientId);
        return client==nullptr?0U:client->LastAcknowledgedEventId;
    }
    double GetServerTimeS() const { return ServerTimeS; }
    const SessionConfig& GetConfig() const { return Config; }

private:
    struct ClientRecord
    {
        bool Known=false;
        bool Connected=false;
        ClientId Id=0U;
        ClientRole Role=ClientRole::Player;
        EventId LastAcknowledgedEventId=0U;
    };

    struct SeatRecord
    {
        ClientId Owner=0U;
        bool OwnerConnected=false;
        bool ReclaimRequired=false;
        bool Armed=false;
        bool HasAcceptedInput=false;
        bool ArmReleaseObserved=false;
        bool LastArmButtonDown=true;
        bool LastDisarmButtonDown=false;
        SeatControlState State=SeatControlState::Unowned;
        ControlCommand AppliedCommand{};
        std::uint64_t LastAcceptedSequence=0U;
        double LastClientTimeS=0.0;
        double LastInputServerTimeS=0.0;
    };

    SessionConfig Config{};
    std::array<ClientRecord,kMaxClientCount> Clients{};
    std::array<SeatRecord,kAircraftSeatCount> Seats{};
    std::array<SessionEvent,kMaxEventHistory> EventLog{};
    std::size_t EventCount=0U;
    EventId LastEventId=0U;
    double ServerTimeS=0.0;

    static bool IsFiniteInRange(double value,double minimum,double maximum)
    {
        return std::isfinite(value)&&value>=minimum&&value<=maximum;
    }

    static bool IsValidRole(ClientRole role)
    {
        return role==ClientRole::Player||role==ClientRole::Referee||role==ClientRole::Administrator;
    }

    static bool IsValidFailsafe(FailsafePolicy policy)
    {
        return policy==FailsafePolicy::NeutralAndDisarm||policy==FailsafePolicy::HoldLastAndDisarm;
    }

    static bool IsValidSeat(int seatIndex)
    {
        return seatIndex>=0&&seatIndex<static_cast<int>(kAircraftSeatCount);
    }

    static bool IsValidConfiguration(const SessionConfig& configuration)
    {
        return IsFiniteInRange(configuration.InputTimeoutS,0.001,60.0)&&
            IsFiniteInRange(configuration.MaximumClientFutureS,0.0,10.0)&&
            IsFiniteInRange(configuration.MaximumClientPastS,0.0,60.0)&&
            IsValidFailsafe(configuration.TimeoutFailsafe);
    }

    static bool IsValidCommand(const ControlCommand& command)
    {
        return IsFiniteInRange(command.RollNormalized,-1.0,1.0)&&
            IsFiniteInRange(command.PitchNormalized,-1.0,1.0)&&
            IsFiniteInRange(command.YawNormalized,-1.0,1.0)&&
            IsFiniteInRange(command.ThrottleNormalized,0.0,1.0);
    }

    static bool IsValidEventType(EventType type)
    {
        return type==EventType::Goal||type==EventType::SetEnded||type==EventType::Forfeit||type==EventType::Disconnect;
    }

    static bool IsValidEventRequest(const ServerEventRequest& request)
    {
        if(!IsValidEventType(request.Type)||!IsFiniteInRange(request.ServerTimeS,0.0,kMaximumSessionTimeS))return false;
        if(request.TeamIndex < -1||request.TeamIndex>1||request.SeatIndex < -1||request.SeatIndex>=static_cast<int>(kAircraftSeatCount))return false;
        switch(request.Type)
        {
        case EventType::Goal:
            return request.TeamIndex>=0&&request.SeatIndex>=0;
        case EventType::SetEnded:
            return request.SeatIndex==-1;
        case EventType::Forfeit:
            return request.TeamIndex>=0||request.SubjectClient!=0U;
        case EventType::Disconnect:
            return request.SubjectClient!=0U&&request.TeamIndex==-1&&request.SeatIndex==-1;
        }
        return false;
    }

    static Rejection ValidateInputPacket(const InputPacket& packet)
    {
        if(!IsValidSeat(packet.SeatIndex))return Rejection::InvalidSeat;
        if(packet.Sequence==0U)return Rejection::InvalidSequence;
        if(!std::isfinite(packet.ClientTimeS)||!std::isfinite(packet.Command.RollNormalized)||
            !std::isfinite(packet.Command.PitchNormalized)||!std::isfinite(packet.Command.YawNormalized)||
            !std::isfinite(packet.Command.ThrottleNormalized))return Rejection::NonFiniteValue;
        if(!IsFiniteInRange(packet.ClientTimeS,0.0,kMaximumSessionTimeS)||!IsValidCommand(packet.Command))return Rejection::ValueOutOfRange;
        return Rejection::None;
    }

    void ResetRuntimeState()
    {
        Clients={};
        Seats={};
        EventLog={};
        EventCount=0U;
        LastEventId=0U;
        ServerTimeS=0.0;
    }

    ClientRecord* FindClient(ClientId clientId)
    {
        for(ClientRecord& client:Clients)if(client.Known&&client.Id==clientId)return &client;
        return nullptr;
    }

    const ClientRecord* FindClient(ClientId clientId) const
    {
        for(const ClientRecord& client:Clients)if(client.Known&&client.Id==clientId)return &client;
        return nullptr;
    }

    ClientRecord* FindConnectedClient(ClientId clientId)
    {
        ClientRecord* const client=FindClient(clientId);
        return client!=nullptr&&client->Connected?client:nullptr;
    }

    Rejection ClientConnectionRejection(ClientId clientId) const
    {
        if(clientId==0U)return Rejection::InvalidClientId;
        return FindClient(clientId)==nullptr?Rejection::UnknownClient:Rejection::ClientNotConnected;
    }

    void AssignSeatToOwner(SeatRecord& seat,ClientId clientId)
    {
        seat=SeatRecord{};
        seat.Owner=clientId;
        seat.OwnerConnected=true;
        seat.State=SeatControlState::AwaitingInput;
        RequireFreshArmPress(seat);
    }

    static void RequireFreshArmPress(SeatRecord& seat)
    {
        seat.Armed=false;
        seat.ArmReleaseObserved=false;
        seat.LastArmButtonDown=true;
        seat.LastDisarmButtonDown=false;
    }

    void ApplyFailsafe(SeatRecord& seat) const
    {
        seat.Armed=false;
        RequireFreshArmPress(seat);
        if(Config.TimeoutFailsafe==FailsafePolicy::NeutralAndDisarm)seat.AppliedCommand=ControlCommand{};
        seat.State=SeatControlState::InputLost;
    }

    static void ApplyArmEdges(SeatRecord& seat,const InputPacket& packet)
    {
        const bool disarmPressed=packet.DisarmButtonDown&&!seat.LastDisarmButtonDown;
        if(disarmPressed)
        {
            seat.Armed=false;
            RequireFreshArmPress(seat);
        }
        if(!packet.ArmButtonDown)seat.ArmReleaseObserved=true;
        else if(seat.ArmReleaseObserved&&!seat.LastArmButtonDown&&!packet.DisarmButtonDown)seat.Armed=true;
        seat.LastArmButtonDown=packet.ArmButtonDown;
        seat.LastDisarmButtonDown=packet.DisarmButtonDown;
    }

    Rejection AdvanceServerTime(double serverTimeS)
    {
        if(!std::isfinite(serverTimeS))return Rejection::NonFiniteValue;
        if(serverTimeS<0.0||serverTimeS>kMaximumSessionTimeS)return Rejection::ValueOutOfRange;
        if(serverTimeS<ServerTimeS)return Rejection::ServerTimeWentBackward;
        ServerTimeS=serverTimeS;
        for(SeatRecord& seat:Seats)
        {
            if(seat.Owner!=0U&&seat.OwnerConnected&&seat.State==SeatControlState::ReceivingInput&&
                serverTimeS-seat.LastInputServerTimeS>Config.InputTimeoutS)ApplyFailsafe(seat);
        }
        return Rejection::None;
    }

    bool AppendEventUnchecked(const ServerEventRequest& request,SessionEvent* eventOut)
    {
        if(LastEventId==std::numeric_limits<EventId>::max())return false;
        const SessionEvent event{LastEventId+1U,request.Type,request.ServerTimeS,request.TeamIndex,request.SeatIndex,request.SubjectClient};
        LastEventId=event.Id;
        if(EventCount<kMaxEventHistory)
        {
            EventLog[EventCount]=event;
            ++EventCount;
        }
        else
        {
            for(std::size_t index=1U;index<kMaxEventHistory;++index)EventLog[index-1U]=EventLog[index];
            EventLog[kMaxEventHistory-1U]=event;
        }
        if(eventOut!=nullptr)*eventOut=event;
        return true;
    }
};
}
