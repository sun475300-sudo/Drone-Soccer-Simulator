#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

// Deterministic horizontal waypoint planning only. Callers fly every waypoint
// through the existing force/torque control path; this file never teleports a body.
namespace ImperialAIPath
{
inline constexpr double ArenaLengthM=16.0;
inline constexpr double ArenaWidthM=7.0;
inline constexpr double ArenaHeightM=4.5;
inline constexpr double AircraftGuardRadiusM=0.20;
inline constexpr std::size_t MaxObstacles=8U;
inline constexpr std::size_t MaxWaypoints=512U;

struct PointM
{
    double X=0.0;
    double Y=0.0;
};

enum class ObstacleKind { Aircraft, GoalRing, Pillar };
enum class Rejection
{
    None,
    NonFiniteInput,
    InvalidAltitudeBand,
    AltitudeOutsideBand,
    InvalidSafetyMargin,
    InvalidWallMargin,
    InvalidReplanSettings,
    TooManyObstacles,
    InvalidObstacle,
    OutsideNavigableArena,
    StartInsideKeepOut,
    TargetInsideKeepOut,
    InvalidGoalAxisApproach,
    NoPath,
    InternalCapacity,
    WaypointCapacity,
    InvalidUpdateInterval,
    InvalidWaypointIndex
};

enum class UpdateCode
{
    NotActive,
    Retained,
    Replanned,
    BlockedRateLimited,
    ReplanFailed,
    LandingRequested,
    RejectedInput
};

struct CircularObstacle
{
    PointM CenterM{};
    // Aircraft always use the rule-derived 0.20 m guard radius. Goal-ring and
    // pillar radii are caller supplied physical radii before SafetyMarginM.
    double RadiusM=0.0;
    ObstacleKind Kind=ObstacleKind::Pillar;
};

struct ObstacleSet
{
    std::array<CircularObstacle,MaxObstacles> Items{};
    std::size_t Count=0U;
};

struct GoalAxisApproach
{
    // This is deliberately opt-in. When requested, TargetM must equal the
    // selected goal centre and OutwardAxisM points from that centre to entry.
    bool Requested=false;
    std::size_t GoalObstacleIndex=MaxObstacles;
    PointM OutwardAxisM{};
};

struct PlanRequest
{
    PointM StartM{};
    PointM TargetM{};
    // All heights are centres of the aircraft's selected altitude band.
    double AltitudeM=2.0;
    double AltitudeBandMinM=0.20;
    double AltitudeBandMaxM=4.30;
    // Margin, wall offset, retry rate, and retry count are simulator choices.
    double SafetyMarginM=0.05;
    double WallMarginM=0.25;
    double ReplanCooldownS=0.25;
    int MaxFailedReplans=2;
    ObstacleSet Obstacles{};
    GoalAxisApproach GoalAxis{};
};

struct PlanResult
{
    Rejection Reason=Rejection::None;
    bool UsesGoalAxisApproach=false;
    std::array<PointM,MaxWaypoints> Waypoints{};
    std::size_t WaypointCount=0U;
    double LengthM=0.0;

    bool Succeeded() const { return Reason==Rejection::None; }
};

class Replanner
{
public:
    PlanResult Begin(const PlanRequest& Request)
    {
        Reset();
        StoredRequest=Request;
        CurrentPlan=BuildPlan(StoredRequest);
        LastReason=CurrentPlan.Reason;
        SessionActive=CurrentPlan.Succeeded();
        return CurrentPlan;
    }

    void Reset()
    {
        StoredRequest=PlanRequest{};
        CurrentPlan=PlanResult{};
        LastReason=Rejection::None;
        SessionActive=false;
        LandingSignalled=false;
        FailedReplans=0;
        SinceLastReplanS=0.0;
    }

    UpdateCode Update(double DtS,PointM CurrentPositionM,std::size_t NextWaypointIndex,
        const ObstacleSet& CurrentObstacles)
    {
        if(!SessionActive)return LandingSignalled?UpdateCode::LandingRequested:UpdateCode::NotActive;
        if(LandingSignalled)return UpdateCode::LandingRequested;
        if(!std::isfinite(DtS)||DtS<=0.0||DtS>1.0)
        {
            LastReason=Rejection::InvalidUpdateInterval;
            return UpdateCode::RejectedInput;
        }
        if(!Finite(CurrentPositionM)||!InsideNavigableArena(CurrentPositionM,StoredRequest.WallMarginM))
        {
            LastReason=Rejection::OutsideNavigableArena;
            return UpdateCode::RejectedInput;
        }
        if(NextWaypointIndex>CurrentPlan.WaypointCount)
        {
            LastReason=Rejection::InvalidWaypointIndex;
            return UpdateCode::RejectedInput;
        }
        const Rejection ObstacleReason=ValidateObstacleSet(CurrentObstacles);
        if(ObstacleReason!=Rejection::None)
        {
            LastReason=ObstacleReason;
            return UpdateCode::RejectedInput;
        }

        SinceLastReplanS+=DtS;
        if(!PathBlocked(CurrentPositionM,NextWaypointIndex,CurrentObstacles))
        {
            LastReason=Rejection::None;
            return UpdateCode::Retained;
        }
        if(SinceLastReplanS<StoredRequest.ReplanCooldownS)return UpdateCode::BlockedRateLimited;

        PlanRequest CandidateRequest=StoredRequest;
        CandidateRequest.StartM=CurrentPositionM;
        CandidateRequest.Obstacles=CurrentObstacles;
        const PlanResult Candidate=BuildPlan(CandidateRequest);
        SinceLastReplanS=0.0;
        if(Candidate.Succeeded())
        {
            StoredRequest=CandidateRequest;
            CurrentPlan=Candidate;
            FailedReplans=0;
            LastReason=Rejection::None;
            return UpdateCode::Replanned;
        }

        LastReason=Candidate.Reason;
        ++FailedReplans;
        if(FailedReplans>=StoredRequest.MaxFailedReplans)
        {
            LandingSignalled=true;
            SessionActive=false;
            return UpdateCode::LandingRequested;
        }
        return UpdateCode::ReplanFailed;
    }

    const PlanResult& GetPlan() const { return CurrentPlan; }
    Rejection GetLastRejection() const { return LastReason; }
    int GetFailedReplans() const { return FailedReplans; }
    bool IsLandingRequested() const { return LandingSignalled; }
    bool IsActive() const { return SessionActive; }

private:
    inline static constexpr double Pi=3.141592653589793238462643383279502884;
    inline static constexpr double TwoPi=2.0*Pi;
    inline static constexpr double EpsilonM=0.000000001;
    inline static constexpr double MinimumSafetyMarginM=0.01;
    inline static constexpr double GoalTargetToleranceM=0.000001;
    inline static constexpr std::size_t NoIndex=static_cast<std::size_t>(-1);
    inline static constexpr std::size_t MaxGraphNodes=384U;

    struct Rect
    {
        double MinX=0.0;
        double MaxX=0.0;
        double MinY=0.0;
        double MaxY=0.0;
    };

    struct ExpandedObstacle
    {
        PointM CenterM{};
        double RadiusM=0.0;
    };

    struct GraphNode
    {
        PointM PositionM{};
        std::size_t CircleIndex=NoIndex;
    };

    struct ArcInfo
    {
        bool Exists=false;
        bool CounterClockwise=true;
        double SweepRad=0.0;
        double LengthM=0.0;
    };

    struct EdgeInfo
    {
        bool Exists=false;
        bool IsArc=false;
        double LengthM=0.0;
        ArcInfo Arc{};
    };

    PlanRequest StoredRequest{};
    PlanResult CurrentPlan{};
    Rejection LastReason=Rejection::None;
    bool SessionActive=false;
    bool LandingSignalled=false;
    int FailedReplans=0;
    double SinceLastReplanS=0.0;

    static bool Finite(PointM Value)
    {
        return std::isfinite(Value.X)&&std::isfinite(Value.Y);
    }

    static double DistanceSquared(PointM A,PointM B)
    {
        const double Dx=A.X-B.X;
        const double Dy=A.Y-B.Y;
        return Dx*Dx+Dy*Dy;
    }

    static double Distance(PointM A,PointM B)
    {
        return std::sqrt(DistanceSquared(A,B));
    }

    static Rect MakeRect(double WallMarginM)
    {
        const double HalfLengthM=ArenaLengthM*0.5;
        const double HalfWidthM=ArenaWidthM*0.5;
        return {-HalfLengthM+WallMarginM,HalfLengthM-WallMarginM,
            -HalfWidthM+WallMarginM,HalfWidthM-WallMarginM};
    }

    static bool InsideRect(PointM Point,const Rect& Bounds)
    {
        return Point.X>=Bounds.MinX-EpsilonM&&Point.X<=Bounds.MaxX+EpsilonM&&
            Point.Y>=Bounds.MinY-EpsilonM&&Point.Y<=Bounds.MaxY+EpsilonM;
    }

    static bool InsideNavigableArena(PointM Point,double WallMarginM)
    {
        if(!Finite(Point)||!std::isfinite(WallMarginM)||WallMarginM<0.0)return false;
        const double HalfLengthM=ArenaLengthM*0.5;
        const double HalfWidthM=ArenaWidthM*0.5;
        if(WallMarginM>=HalfLengthM||WallMarginM>=HalfWidthM)return false;
        return InsideRect(Point,MakeRect(WallMarginM));
    }

    static double BaseRadiusM(const CircularObstacle& Obstacle)
    {
        return Obstacle.Kind==ObstacleKind::Aircraft?AircraftGuardRadiusM:Obstacle.RadiusM;
    }

    static Rejection ValidateObstacleSet(const ObstacleSet& Obstacles)
    {
        if(Obstacles.Count>MaxObstacles)return Rejection::TooManyObstacles;
        const double HalfLengthM=ArenaLengthM*0.5;
        const double HalfWidthM=ArenaWidthM*0.5;
        for(std::size_t Index=0U;Index<Obstacles.Count;++Index)
        {
            const CircularObstacle& Obstacle=Obstacles.Items[Index];
            if(!Finite(Obstacle.CenterM)||!std::isfinite(Obstacle.RadiusM))return Rejection::NonFiniteInput;
            if(Obstacle.CenterM.X<-HalfLengthM||Obstacle.CenterM.X>HalfLengthM||
                Obstacle.CenterM.Y<-HalfWidthM||Obstacle.CenterM.Y>HalfWidthM)return Rejection::InvalidObstacle;
            const bool KnownKind=Obstacle.Kind==ObstacleKind::Aircraft||Obstacle.Kind==ObstacleKind::GoalRing||
                Obstacle.Kind==ObstacleKind::Pillar;
            if(!KnownKind)return Rejection::InvalidObstacle;
            if(Obstacle.Kind!=ObstacleKind::Aircraft&&Obstacle.RadiusM<=0.0)return Rejection::InvalidObstacle;
            if(Obstacle.Kind==ObstacleKind::Aircraft&&Obstacle.RadiusM<0.0)return Rejection::InvalidObstacle;
        }
        return Rejection::None;
    }

    static double ArcOutsetM(const PlanRequest& Request)
    {
        return Request.SafetyMarginM*0.5<0.02?Request.SafetyMarginM*0.5:0.02;
    }

    static Rejection ValidateRequest(const PlanRequest& Request,PointM& EffectiveTargetM,
        std::size_t& GoalAxisObstacleIndex)
    {
        GoalAxisObstacleIndex=NoIndex;
        EffectiveTargetM=Request.TargetM;
        if(!Finite(Request.StartM)||!Finite(Request.TargetM)||!std::isfinite(Request.AltitudeM)||
            !std::isfinite(Request.AltitudeBandMinM)||!std::isfinite(Request.AltitudeBandMaxM)||
            !std::isfinite(Request.SafetyMarginM)||!std::isfinite(Request.WallMarginM)||
            !std::isfinite(Request.ReplanCooldownS)||!Finite(Request.GoalAxis.OutwardAxisM))
            return Rejection::NonFiniteInput;
        if(Request.AltitudeBandMinM<0.0||Request.AltitudeBandMaxM>ArenaHeightM||
            Request.AltitudeBandMinM>=Request.AltitudeBandMaxM)return Rejection::InvalidAltitudeBand;
        if(Request.AltitudeM<Request.AltitudeBandMinM||Request.AltitudeM>Request.AltitudeBandMaxM)
            return Rejection::AltitudeOutsideBand;
        if(Request.SafetyMarginM<MinimumSafetyMarginM||Request.SafetyMarginM>0.50)
            return Rejection::InvalidSafetyMargin;
        const double HalfLengthM=ArenaLengthM*0.5;
        const double HalfWidthM=ArenaWidthM*0.5;
        if(Request.WallMarginM<0.0||Request.WallMarginM>=HalfLengthM||Request.WallMarginM>=HalfWidthM)
            return Rejection::InvalidWallMargin;
        if(Request.ReplanCooldownS<=0.0||Request.ReplanCooldownS>10.0||
            Request.MaxFailedReplans<1||Request.MaxFailedReplans>10)return Rejection::InvalidReplanSettings;
        const Rejection ObstacleReason=ValidateObstacleSet(Request.Obstacles);
        if(ObstacleReason!=Rejection::None)return ObstacleReason;

        if(Request.GoalAxis.Requested)
        {
            if(Request.GoalAxis.GoalObstacleIndex>=Request.Obstacles.Count)return Rejection::InvalidGoalAxisApproach;
            const CircularObstacle& Goal=Request.Obstacles.Items[Request.GoalAxis.GoalObstacleIndex];
            if(Goal.Kind!=ObstacleKind::GoalRing)return Rejection::InvalidGoalAxisApproach;
            if(Distance(Request.TargetM,Goal.CenterM)>GoalTargetToleranceM)return Rejection::InvalidGoalAxisApproach;
            const double AxisLengthM=Distance(Request.GoalAxis.OutwardAxisM,PointM{});
            if(AxisLengthM<=EpsilonM)return Rejection::InvalidGoalAxisApproach;
            const double EntryDistanceM=BaseRadiusM(Goal)+Request.SafetyMarginM+ArcOutsetM(Request);
            EffectiveTargetM={Goal.CenterM.X+Request.GoalAxis.OutwardAxisM.X*EntryDistanceM/AxisLengthM,
                Goal.CenterM.Y+Request.GoalAxis.OutwardAxisM.Y*EntryDistanceM/AxisLengthM};
            GoalAxisObstacleIndex=Request.GoalAxis.GoalObstacleIndex;
        }

        if(!InsideNavigableArena(Request.StartM,Request.WallMarginM)||
            !InsideNavigableArena(Request.TargetM,Request.WallMarginM)||
            !InsideNavigableArena(EffectiveTargetM,Request.WallMarginM))return Rejection::OutsideNavigableArena;

        for(std::size_t Index=0U;Index<Request.Obstacles.Count;++Index)
        {
            const CircularObstacle& Obstacle=Request.Obstacles.Items[Index];
            const double RadiusM=BaseRadiusM(Obstacle)+Request.SafetyMarginM;
            const double RadiusSquaredM=RadiusM*RadiusM;
            if(DistanceSquared(Request.StartM,Obstacle.CenterM)<RadiusSquaredM-EpsilonM)return Rejection::StartInsideKeepOut;
            if(DistanceSquared(EffectiveTargetM,Obstacle.CenterM)<RadiusSquaredM-EpsilonM)
                return Rejection::TargetInsideKeepOut;
        }
        return Rejection::None;
    }

    static void ExpandObstacles(const PlanRequest& Request,std::array<ExpandedObstacle,MaxObstacles>& Expanded)
    {
        for(std::size_t Index=0U;Index<Request.Obstacles.Count;++Index)
        {
            const CircularObstacle& Source=Request.Obstacles.Items[Index];
            Expanded[Index]={Source.CenterM,BaseRadiusM(Source)+Request.SafetyMarginM};
        }
    }

    static double PositiveAngle(double AngleRad)
    {
        double Result=std::fmod(AngleRad,TwoPi);
        if(Result<0.0)Result+=TwoPi;
        return Result;
    }

    static bool AngleOnArc(double AngleRad,double StartRad,double SweepRad,bool CounterClockwise)
    {
        const double TravelledRad=CounterClockwise?PositiveAngle(AngleRad-StartRad):
            PositiveAngle(StartRad-AngleRad);
        return TravelledRad<=SweepRad+EpsilonM;
    }

    static PointM PointOnCircle(const ExpandedObstacle& Circle,double AngleRad,double RadiusM)
    {
        return {Circle.CenterM.X+RadiusM*std::cos(AngleRad),Circle.CenterM.Y+RadiusM*std::sin(AngleRad)};
    }

    static double SegmentDistanceSquared(PointM Point,PointM A,PointM B)
    {
        const double Dx=B.X-A.X;
        const double Dy=B.Y-A.Y;
        const double SegmentLengthSquaredM=Dx*Dx+Dy*Dy;
        if(SegmentLengthSquaredM<=EpsilonM)return DistanceSquared(Point,A);
        double Projection=((Point.X-A.X)*Dx+(Point.Y-A.Y)*Dy)/SegmentLengthSquaredM;
        if(Projection<0.0)Projection=0.0;
        if(Projection>1.0)Projection=1.0;
        const PointM Closest{A.X+Dx*Projection,A.Y+Dy*Projection};
        return DistanceSquared(Point,Closest);
    }

    static bool SegmentClear(PointM A,PointM B,const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,
        std::size_t ObstacleCount,std::size_t IgnoreObstacleIndex=NoIndex)
    {
        for(std::size_t Index=0U;Index<ObstacleCount;++Index)
        {
            if(Index==IgnoreObstacleIndex)continue;
            const double RadiusM=Obstacles[Index].RadiusM;
            if(SegmentDistanceSquared(Obstacles[Index].CenterM,A,B)<RadiusM*RadiusM-EpsilonM)return false;
        }
        return true;
    }

    static bool ArcPointClear(std::size_t CircleIndex,double AngleRad,const Rect& Bounds,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,std::size_t ObstacleCount)
    {
        const PointM Point=PointOnCircle(Obstacles[CircleIndex],AngleRad,Obstacles[CircleIndex].RadiusM);
        if(!InsideRect(Point,Bounds))return false;
        for(std::size_t Index=0U;Index<ObstacleCount;++Index)
        {
            if(Index==CircleIndex)continue;
            const double RadiusM=Obstacles[Index].RadiusM;
            if(DistanceSquared(Point,Obstacles[Index].CenterM)<RadiusM*RadiusM-EpsilonM)return false;
        }
        return true;
    }

    static bool ArcClear(std::size_t CircleIndex,double StartRad,double SweepRad,bool CounterClockwise,
        const Rect& Bounds,const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,std::size_t ObstacleCount)
    {
        const double EndRad=CounterClockwise?StartRad+SweepRad:StartRad-SweepRad;
        if(!ArcPointClear(CircleIndex,StartRad,Bounds,Obstacles,ObstacleCount)||
            !ArcPointClear(CircleIndex,EndRad,Bounds,Obstacles,ObstacleCount))return false;
        const std::array<double,4U> CardinalAngles{{0.0,Pi*0.5,Pi,Pi*1.5}};
        for(const double AngleRad:CardinalAngles)
            if(AngleOnArc(AngleRad,StartRad,SweepRad,CounterClockwise)&&
                !ArcPointClear(CircleIndex,AngleRad,Bounds,Obstacles,ObstacleCount))return false;
        for(std::size_t Index=0U;Index<ObstacleCount;++Index)
        {
            if(Index==CircleIndex)continue;
            const PointM Delta{Obstacles[Index].CenterM.X-Obstacles[CircleIndex].CenterM.X,
                Obstacles[Index].CenterM.Y-Obstacles[CircleIndex].CenterM.Y};
            if(DistanceSquared(Delta,PointM{})<=EpsilonM)continue;
            const double NearestAngleRad=std::atan2(Delta.Y,Delta.X);
            if(AngleOnArc(NearestAngleRad,StartRad,SweepRad,CounterClockwise)&&
                !ArcPointClear(CircleIndex,NearestAngleRad,Bounds,Obstacles,ObstacleCount))return false;
        }
        return true;
    }

    static ArcInfo FindBestArc(const GraphNode& A,const GraphNode& B,const Rect& Bounds,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,std::size_t ObstacleCount)
    {
        ArcInfo Best{};
        if(A.CircleIndex==NoIndex||A.CircleIndex!=B.CircleIndex)return Best;
        const ExpandedObstacle& Circle=Obstacles[A.CircleIndex];
        const double StartRad=std::atan2(A.PositionM.Y-Circle.CenterM.Y,A.PositionM.X-Circle.CenterM.X);
        const double EndRad=std::atan2(B.PositionM.Y-Circle.CenterM.Y,B.PositionM.X-Circle.CenterM.X);
        const double CounterClockwiseSweepRad=PositiveAngle(EndRad-StartRad);
        const double ClockwiseSweepRad=PositiveAngle(StartRad-EndRad);
        const bool CounterClockwiseClear=CounterClockwiseSweepRad>EpsilonM&&
            ArcClear(A.CircleIndex,StartRad,CounterClockwiseSweepRad,true,Bounds,Obstacles,ObstacleCount);
        const bool ClockwiseClear=ClockwiseSweepRad>EpsilonM&&
            ArcClear(A.CircleIndex,StartRad,ClockwiseSweepRad,false,Bounds,Obstacles,ObstacleCount);
        if(CounterClockwiseClear)
        {
            Best={true,true,CounterClockwiseSweepRad,CounterClockwiseSweepRad*Circle.RadiusM};
        }
        if(ClockwiseClear)
        {
            const double ClockwiseLengthM=ClockwiseSweepRad*Circle.RadiusM;
            if(!Best.Exists||ClockwiseLengthM+EpsilonM<Best.LengthM)
                Best={true,false,ClockwiseSweepRad,ClockwiseLengthM};
        }
        return Best;
    }

    static EdgeInfo MakeEdge(const GraphNode& A,const GraphNode& B,const Rect& Bounds,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,std::size_t ObstacleCount)
    {
        if(A.CircleIndex!=NoIndex&&A.CircleIndex==B.CircleIndex)
        {
            const ArcInfo Arc=FindBestArc(A,B,Bounds,Obstacles,ObstacleCount);
            return {Arc.Exists,true,Arc.LengthM,Arc};
        }
        if(!SegmentClear(A.PositionM,B.PositionM,Obstacles,ObstacleCount))return {};
        return {true,false,Distance(A.PositionM,B.PositionM),{}};
    }

    static bool AddNode(std::array<GraphNode,MaxGraphNodes>& Nodes,std::size_t& NodeCount,PointM PositionM,
        std::size_t CircleIndex,const Rect& Bounds)
    {
        if(!InsideRect(PositionM,Bounds))return true;
        for(std::size_t Index=0U;Index<NodeCount;++Index)
        {
            if(Nodes[Index].CircleIndex==CircleIndex&&DistanceSquared(Nodes[Index].PositionM,PositionM)<=EpsilonM*EpsilonM)
                return true;
        }
        if(NodeCount>=MaxGraphNodes)return false;
        Nodes[NodeCount]={PositionM,CircleIndex};
        ++NodeCount;
        return true;
    }

    static bool AddPointTangents(PointM ExternalPointM,std::size_t CircleIndex,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,const Rect& Bounds,
        std::array<GraphNode,MaxGraphNodes>& Nodes,std::size_t& NodeCount)
    {
        const ExpandedObstacle& Circle=Obstacles[CircleIndex];
        const double Dx=ExternalPointM.X-Circle.CenterM.X;
        const double Dy=ExternalPointM.Y-Circle.CenterM.Y;
        const double DistanceM=std::sqrt(Dx*Dx+Dy*Dy);
        if(DistanceM<=Circle.RadiusM+EpsilonM)return true;
        const double BaseAngleRad=std::atan2(Dy,Dx);
        double Ratio=Circle.RadiusM/DistanceM;
        if(Ratio>1.0)Ratio=1.0;
        const double OffsetRad=std::acos(Ratio);
        const PointM First=PointOnCircle(Circle,BaseAngleRad+OffsetRad,Circle.RadiusM);
        const PointM Second=PointOnCircle(Circle,BaseAngleRad-OffsetRad,Circle.RadiusM);
        return AddNode(Nodes,NodeCount,First,CircleIndex,Bounds)&&AddNode(Nodes,NodeCount,Second,CircleIndex,Bounds);
    }

    static bool AddCircleTangents(std::size_t FirstIndex,std::size_t SecondIndex,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,const Rect& Bounds,
        std::array<GraphNode,MaxGraphNodes>& Nodes,std::size_t& NodeCount)
    {
        const ExpandedObstacle& First=Obstacles[FirstIndex];
        const ExpandedObstacle& Second=Obstacles[SecondIndex];
        const double Dx=Second.CenterM.X-First.CenterM.X;
        const double Dy=Second.CenterM.Y-First.CenterM.Y;
        const double DistanceSquaredM=Dx*Dx+Dy*Dy;
        if(DistanceSquaredM<=EpsilonM)return true;
        for(const int RelativeSign:{-1,1})
        {
            const double DifferenceM=First.RadiusM-static_cast<double>(RelativeSign)*Second.RadiusM;
            const double HeightSquaredM=DistanceSquaredM-DifferenceM*DifferenceM;
            if(HeightSquaredM<-EpsilonM)continue;
            const double HeightM=std::sqrt(HeightSquaredM>0.0?HeightSquaredM:0.0);
            for(const int Orientation:{-1,1})
            {
                const double Nx=(Dx*DifferenceM-Dy*HeightM*static_cast<double>(Orientation))/DistanceSquaredM;
                const double Ny=(Dy*DifferenceM+Dx*HeightM*static_cast<double>(Orientation))/DistanceSquaredM;
                const PointM FirstPoint{First.CenterM.X+First.RadiusM*Nx,First.CenterM.Y+First.RadiusM*Ny};
                const PointM SecondPoint{Second.CenterM.X+static_cast<double>(RelativeSign)*Second.RadiusM*Nx,
                    Second.CenterM.Y+static_cast<double>(RelativeSign)*Second.RadiusM*Ny};
                if(!AddNode(Nodes,NodeCount,FirstPoint,FirstIndex,Bounds)||
                    !AddNode(Nodes,NodeCount,SecondPoint,SecondIndex,Bounds))return false;
            }
        }
        return true;
    }

    static bool AppendWaypoint(PlanResult& Result,PointM PointMValue)
    {
        if(Result.WaypointCount>=MaxWaypoints)return false;
        Result.Waypoints[Result.WaypointCount]=PointMValue;
        ++Result.WaypointCount;
        return true;
    }

    static bool AppendArcWaypoints(PlanResult& Result,const GraphNode& A,const GraphNode& B,const ArcInfo& Arc,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,const Rect& Bounds,const PlanRequest& Request)
    {
        const ExpandedObstacle& Circle=Obstacles[A.CircleIndex];
        const double OutsetM=ArcOutsetM(Request);
        const double OuterRadiusM=Circle.RadiusM+OutsetM;
        double Ratio=Circle.RadiusM/OuterRadiusM;
        if(Ratio>1.0)Ratio=1.0;
        const double MaximumStepRad=2.0*std::acos(Ratio);
        if(MaximumStepRad<=EpsilonM)return false;
        const std::size_t SegmentCount=static_cast<std::size_t>(std::ceil(Arc.SweepRad/MaximumStepRad));
        if(SegmentCount==0U)return false;
        const double StartRad=std::atan2(A.PositionM.Y-Circle.CenterM.Y,A.PositionM.X-Circle.CenterM.X);
        for(std::size_t Segment=0U;Segment<=SegmentCount;++Segment)
        {
            const double Fraction=static_cast<double>(Segment)/static_cast<double>(SegmentCount);
            const double Direction=Arc.CounterClockwise?1.0:-1.0;
            const PointM OuterPoint=PointOnCircle(Circle,StartRad+Direction*Arc.SweepRad*Fraction,OuterRadiusM);
            if(!InsideRect(OuterPoint,Bounds)||!AppendWaypoint(Result,OuterPoint))return false;
        }
        return AppendWaypoint(Result,B.PositionM);
    }

    static bool VerifyOutput(const PlanResult& Result,const PlanRequest& Request,
        const std::array<ExpandedObstacle,MaxObstacles>& Obstacles,std::size_t GoalAxisObstacleIndex)
    {
        PointM PreviousM=Request.StartM;
        for(std::size_t Index=0U;Index<Result.WaypointCount;++Index)
        {
            const bool FinalGoalAxisSegment=Result.UsesGoalAxisApproach&&Index+1U==Result.WaypointCount;
            const std::size_t IgnoreIndex=FinalGoalAxisSegment?GoalAxisObstacleIndex:NoIndex;
            if(!SegmentClear(PreviousM,Result.Waypoints[Index],Obstacles,Request.Obstacles.Count,IgnoreIndex))return false;
            PreviousM=Result.Waypoints[Index];
        }
        return true;
    }

    static void ComputeOutputLength(PlanResult& Result,PointM StartM)
    {
        Result.LengthM=0.0;
        PointM PreviousM=StartM;
        for(std::size_t Index=0U;Index<Result.WaypointCount;++Index)
        {
            Result.LengthM+=Distance(PreviousM,Result.Waypoints[Index]);
            PreviousM=Result.Waypoints[Index];
        }
    }

    static PlanResult BuildPlan(const PlanRequest& Request)
    {
        PlanResult Result{};
        PointM EffectiveTargetM{};
        std::size_t GoalAxisObstacleIndex=NoIndex;
        const Rejection InputReason=ValidateRequest(Request,EffectiveTargetM,GoalAxisObstacleIndex);
        if(InputReason!=Rejection::None)
        {
            Result.Reason=InputReason;
            return Result;
        }

        const Rect Bounds=MakeRect(Request.WallMarginM);
        std::array<ExpandedObstacle,MaxObstacles> Obstacles{};
        ExpandObstacles(Request,Obstacles);
        std::array<GraphNode,MaxGraphNodes> Nodes{};
        std::size_t NodeCount=0U;
        if(!AddNode(Nodes,NodeCount,Request.StartM,NoIndex,Bounds)||
            !AddNode(Nodes,NodeCount,EffectiveTargetM,NoIndex,Bounds))
        {
            Result.Reason=Rejection::InternalCapacity;
            return Result;
        }
        const std::array<PointM,4U> Corners{{{Bounds.MinX,Bounds.MinY},{Bounds.MinX,Bounds.MaxY},
            {Bounds.MaxX,Bounds.MinY},{Bounds.MaxX,Bounds.MaxY}}};
        for(const PointM Corner:Corners)
        {
            if(!AddNode(Nodes,NodeCount,Corner,NoIndex,Bounds))
            {
                Result.Reason=Rejection::InternalCapacity;
                return Result;
            }
        }
        const std::array<PointM,6U> ExternalPoints{{Request.StartM,EffectiveTargetM,Corners[0U],Corners[1U],Corners[2U],Corners[3U]}};
        for(std::size_t CircleIndex=0U;CircleIndex<Request.Obstacles.Count;++CircleIndex)
        {
            for(const PointM ExternalPoint:ExternalPoints)
            {
                if(!AddPointTangents(ExternalPoint,CircleIndex,Obstacles,Bounds,Nodes,NodeCount))
                {
                    Result.Reason=Rejection::InternalCapacity;
                    return Result;
                }
            }
        }
        for(std::size_t FirstIndex=0U;FirstIndex<Request.Obstacles.Count;++FirstIndex)
        {
            for(std::size_t SecondIndex=FirstIndex+1U;SecondIndex<Request.Obstacles.Count;++SecondIndex)
            {
                if(!AddCircleTangents(FirstIndex,SecondIndex,Obstacles,Bounds,Nodes,NodeCount))
                {
                    Result.Reason=Rejection::InternalCapacity;
                    return Result;
                }
            }
        }

        std::array<double,MaxGraphNodes> DistanceTo{};
        std::array<std::size_t,MaxGraphNodes> Previous{};
        std::array<bool,MaxGraphNodes> Visited{};
        const double Infinity=std::numeric_limits<double>::infinity();
        for(std::size_t Index=0U;Index<NodeCount;++Index)
        {
            DistanceTo[Index]=Infinity;
            Previous[Index]=NoIndex;
        }
        DistanceTo[0U]=0.0;
        for(std::size_t Step=0U;Step<NodeCount;++Step)
        {
            std::size_t CurrentIndex=NoIndex;
            double BestDistanceM=Infinity;
            for(std::size_t Index=0U;Index<NodeCount;++Index)
            {
                if(!Visited[Index]&&DistanceTo[Index]<BestDistanceM)
                {
                    BestDistanceM=DistanceTo[Index];
                    CurrentIndex=Index;
                }
            }
            if(CurrentIndex==NoIndex||CurrentIndex==1U)break;
            Visited[CurrentIndex]=true;
            for(std::size_t NextIndex=0U;NextIndex<NodeCount;++NextIndex)
            {
                if(Visited[NextIndex]||NextIndex==CurrentIndex)continue;
                const EdgeInfo Edge=MakeEdge(Nodes[CurrentIndex],Nodes[NextIndex],Bounds,Obstacles,Request.Obstacles.Count);
                if(!Edge.Exists)continue;
                const double CandidateDistanceM=DistanceTo[CurrentIndex]+Edge.LengthM;
                const bool Better=CandidateDistanceM+EpsilonM<DistanceTo[NextIndex];
                const bool EqualAndEarlier=std::abs(CandidateDistanceM-DistanceTo[NextIndex])<=EpsilonM&&
                    (Previous[NextIndex]==NoIndex||CurrentIndex<Previous[NextIndex]);
                if(Better||EqualAndEarlier)
                {
                    DistanceTo[NextIndex]=CandidateDistanceM;
                    Previous[NextIndex]=CurrentIndex;
                }
            }
        }
        if(!std::isfinite(DistanceTo[1U]))
        {
            Result.Reason=Rejection::NoPath;
            return Result;
        }

        std::array<std::size_t,MaxGraphNodes> ReverseRoute{};
        std::size_t RouteCount=0U;
        std::size_t RouteNode=1U;
        while(true)
        {
            if(RouteCount>=MaxGraphNodes)
            {
                Result.Reason=Rejection::InternalCapacity;
                return Result;
            }
            ReverseRoute[RouteCount]=RouteNode;
            ++RouteCount;
            if(RouteNode==0U)break;
            RouteNode=Previous[RouteNode];
            if(RouteNode==NoIndex)
            {
                Result.Reason=Rejection::NoPath;
                return Result;
            }
        }

        Result.UsesGoalAxisApproach=Request.GoalAxis.Requested;
        for(std::size_t RouteIndex=RouteCount-1U;RouteIndex>0U;--RouteIndex)
        {
            const GraphNode& A=Nodes[ReverseRoute[RouteIndex]];
            const GraphNode& B=Nodes[ReverseRoute[RouteIndex-1U]];
            const EdgeInfo Edge=MakeEdge(A,B,Bounds,Obstacles,Request.Obstacles.Count);
            const bool Appended=Edge.IsArc?AppendArcWaypoints(Result,A,B,Edge.Arc,Obstacles,Bounds,Request):
                AppendWaypoint(Result,B.PositionM);
            if(!Edge.Exists||!Appended)
            {
                Result.Reason=Rejection::WaypointCapacity;
                Result.WaypointCount=0U;
                return Result;
            }
        }
        if(Request.GoalAxis.Requested&&!AppendWaypoint(Result,Request.TargetM))
        {
            Result.Reason=Rejection::WaypointCapacity;
            Result.WaypointCount=0U;
            return Result;
        }
        if(!VerifyOutput(Result,Request,Obstacles,GoalAxisObstacleIndex))
        {
            Result.Reason=Rejection::NoPath;
            Result.WaypointCount=0U;
            return Result;
        }
        ComputeOutputLength(Result,Request.StartM);
        Result.Reason=Rejection::None;
        return Result;
    }

    bool PathBlocked(PointM CurrentPositionM,std::size_t NextWaypointIndex,const ObstacleSet& ObstaclesNow) const
    {
        std::array<ExpandedObstacle,MaxObstacles> Expanded{};
        PlanRequest CurrentRequest=StoredRequest;
        CurrentRequest.Obstacles=ObstaclesNow;
        ExpandObstacles(CurrentRequest,Expanded);
        PointM PreviousM=CurrentPositionM;
        for(std::size_t Index=NextWaypointIndex;Index<CurrentPlan.WaypointCount;++Index)
        {
            const bool FinalGoalAxisSegment=CurrentPlan.UsesGoalAxisApproach&&Index+1U==CurrentPlan.WaypointCount;
            const std::size_t IgnoreIndex=FinalGoalAxisSegment?StoredRequest.GoalAxis.GoalObstacleIndex:NoIndex;
            if(!SegmentClear(PreviousM,CurrentPlan.Waypoints[Index],Expanded,ObstaclesNow.Count,IgnoreIndex))return true;
            PreviousM=CurrentPlan.Waypoints[Index];
        }
        return false;
    }
};

inline PlanResult PlanWaypoints(const PlanRequest& Request)
{
    Replanner Planner;
    return Planner.Begin(Request);
}
}
