#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

// Engine-independent local Topgun S-course judge. Positions are metres and
// all times are seconds supplied by the caller's monotonic simulation clock.
namespace ImperialTopgun
{
struct Vec3M
{
    double XM=0.0;
    double YM=0.0;
    double ZM=0.0;
};

inline bool IsFinite(const Vec3M& Value)
{
    return std::isfinite(Value.XM) && std::isfinite(Value.YM) && std::isfinite(Value.ZM);
}

struct VerticalPillar
{
    Vec3M CenterM{};
    double RadiusM=0.0;
    double BaseZM=0.0;
    double TopZM=0.0;
};

struct PassZone
{
    double PlaneXM=0.0;
    double MinimumYM=0.0;
    double MaximumYM=0.0;
    double MinimumZM=0.0;
    double MaximumZM=0.0;
};

// The pillar positions and dimensions below are simulator choices, not FIDA
// course dimensions. GuardRadiusM is the FIDA Class40 0.20 m exterior radius
// (rules-sources-2026.md, clause 2-1).
struct CourseDefinition
{
    const char* Version="topgun-s2-draft-1";
    VerticalPillar PillarOne{};
    VerticalPillar PillarTwo{};
    PassZone StartLine{};
    PassZone PillarOneSide{};
    PassZone BetweenPillars{};
    PassZone PillarTwoOppositeSide{};
    PassZone FinishLine{};
    double GuardRadiusM=0.20;
    double MaximumFrameStepS=0.25;
    double MaximumClockPhysicsGapS=0.005;
    double NormalTimeScaleTolerance=0.000001;
    // Simulator choice: reject telemetry outside the local-course envelope.
    double MaximumCoordinateMagnitudeM=100.0;

    static CourseDefinition TopgunS2Draft1()
    {
        CourseDefinition Definition{};
        // Simulator choices: 4.00 m high, 0.35 m radius pillars on the course
        // centreline. Positive-Y around pillar one then negative-Y around
        // pillar two gives the required forward S shape.
        Definition.PillarOne={{-2.0,0.0,2.25},0.35,0.25,4.25};
        Definition.PillarTwo={{2.0,0.0,2.25},0.35,0.25,4.25};
        Definition.StartLine={-5.50,-2.75,2.75,0.25,4.25};
        Definition.PillarOneSide={-2.0,0.75,2.75,0.25,4.25};
        Definition.BetweenPillars={0.0,-0.45,0.45,0.25,4.25};
        Definition.PillarTwoOppositeSide={2.0,-2.75,-0.75,0.25,4.25};
        Definition.FinishLine={5.50,-2.75,2.75,0.25,4.25};
        return Definition;
    }
};

struct AttemptConditions
{
    std::uint64_t AircraftId=0U;
    std::uint64_t PhysicsRevision=0U;
    std::uint64_t GravityRevision=0U;
    std::uint64_t AssistRevision=0U;
};

struct StartObservation
{
    Vec3M GuardCenterM{};
    double SimulationClockS=0.0;
    double TimeScale=1.0;
    bool Paused=false;
    bool ExplicitReset=false;
    AttemptConditions Conditions{};
};

struct FrameObservation
{
    Vec3M GuardCenterM{};
    // This is the caller's actual physics-time advance, not a display delta.
    double PhysicsDeltaS=0.0;
    double SimulationClockS=0.0;
    double TimeScale=1.0;
    bool Paused=false;
    bool ExplicitReset=false;
    std::uint32_t CollisionCount=0U;
    AttemptConditions Conditions{};
};

enum class CourseStage
{
    Idle,
    AwaitStart,
    PassPillarOneSide,
    PassBetweenPillars,
    PassPillarTwoOppositeSide,
    PassFinish,
    Finished,
    Invalid
};

enum class CourseRejection
{
    None,
    NotArmed,
    CourseDefinitionInvalid,
    NonFiniteInput,
    ExplicitReset,
    AircraftSwap,
    PhysicsSettingChanged,
    GravitySettingChanged,
    AssistSettingChanged,
    Paused,
    AbnormalTimeScale,
    NonMonotonicClock,
    PhysicsTimeGap,
    ReverseDirection,
    SkippedZone,
    WrongSide
};

struct JudgeResult
{
    CourseStage Stage=CourseStage::Idle;
    CourseRejection Rejection=CourseRejection::None;
    bool AcceptedFrame=false;
    bool Started=false;
    bool Advanced=false;
    bool Finished=false;
    bool OfficialRecordEligible=false;
    std::uint32_t CollisionCount=0U;
    // EventClockS is an interpolated crossing time when a zone is crossed.
    double EventClockS=0.0;
    double ElapsedTimeS=0.0;
};

struct CollisionPolicy
{
    // Draft default: collisions continue a practice run but remove its local
    // official-record eligibility. Set false only for an explicitly labelled
    // alternative draft; the completed record remains local and unverified.
    bool RequireNoCollisionForOfficial=true;
};

class CourseJudge
{
public:
    explicit CourseJudge(const CourseDefinition& InDefinition=CourseDefinition::TopgunS2Draft1(),
                         const CollisionPolicy& InCollisionPolicy=CollisionPolicy())
        : Definition(InDefinition), Policy(InCollisionPolicy)
    {
    }

    JudgeResult Begin(const StartObservation& Observation)
    {
        ResetState();
        if (!IsDefinitionValid()) return Reject(CourseRejection::CourseDefinitionInvalid);
        if (!IsStartObservationValid(Observation)) return Reject(CourseRejection::NonFiniteInput);
        if (Observation.ExplicitReset) return Reject(CourseRejection::ExplicitReset);
        if (Observation.Paused) return Reject(CourseRejection::Paused);
        if (!IsNormalTimeScale(Observation.TimeScale)) return Reject(CourseRejection::AbnormalTimeScale);

        PreviousCenterM=Observation.GuardCenterM;
        PreviousClockS=Observation.SimulationClockS;
        BaselineConditions=Observation.Conditions;
        HavePrevious=true;
        StageValue=CourseStage::AwaitStart;
        LastRejection=CourseRejection::None;
        return MakeResult(true);
    }

    JudgeResult Update(const FrameObservation& Observation)
    {
        if (!HavePrevious || StageValue==CourseStage::Idle) return Reject(CourseRejection::NotArmed);
        if (StageValue==CourseStage::Invalid || StageValue==CourseStage::Finished) return MakeResult(false);
        if (!IsFrameObservationFinite(Observation)) return Reject(CourseRejection::NonFiniteInput);
        if (Observation.ExplicitReset) return Reject(CourseRejection::ExplicitReset);
        if (Observation.Paused) return Reject(CourseRejection::Paused);
        if (!IsNormalTimeScale(Observation.TimeScale)) return Reject(CourseRejection::AbnormalTimeScale);
        if (Observation.Conditions.AircraftId!=BaselineConditions.AircraftId) return Reject(CourseRejection::AircraftSwap);
        if (Observation.Conditions.PhysicsRevision!=BaselineConditions.PhysicsRevision) return Reject(CourseRejection::PhysicsSettingChanged);
        if (Observation.Conditions.GravityRevision!=BaselineConditions.GravityRevision) return Reject(CourseRejection::GravitySettingChanged);
        if (Observation.Conditions.AssistRevision!=BaselineConditions.AssistRevision) return Reject(CourseRejection::AssistSettingChanged);

        const double ClockDeltaS=Observation.SimulationClockS-PreviousClockS;
        if (ClockDeltaS<=0.0) return Reject(CourseRejection::NonMonotonicClock);
        if (Observation.PhysicsDeltaS<=0.0 || Observation.PhysicsDeltaS>Definition.MaximumFrameStepS ||
            ClockDeltaS>Definition.MaximumFrameStepS ||
            std::abs(ClockDeltaS-Observation.PhysicsDeltaS)>Definition.MaximumClockPhysicsGapS)
            return Reject(CourseRejection::PhysicsTimeGap);

        AddCollisions(Observation.CollisionCount);
        if (Observation.GuardCenterM.XM+DirectionEpsilonM<PreviousCenterM.XM)
            return Reject(CourseRejection::ReverseDirection);

        JudgeResult Result=MakeResult(true);
        Result.EventClockS=PreviousClockS;
        ProcessSweptSegment(Observation.GuardCenterM,Observation.SimulationClockS,Result);
        if (StageValue!=CourseStage::Invalid)
        {
            PreviousCenterM=Observation.GuardCenterM;
            PreviousClockS=Observation.SimulationClockS;
        }
        Result.Stage=StageValue;
        Result.Rejection=LastRejection;
        Result.AcceptedFrame=StageValue!=CourseStage::Invalid;
        Result.CollisionCount=CollisionTotal;
        Result.OfficialRecordEligible=IsOfficialRecordEligible();
        Result.Finished=StageValue==CourseStage::Finished;
        Result.ElapsedTimeS=ElapsedTimeS(Result.EventClockS);
        return Result;
    }

    void ResetState()
    {
        StageValue=CourseStage::Idle;
        LastRejection=CourseRejection::None;
        PreviousCenterM={};
        PreviousClockS=0.0;
        StartClockS=0.0;
        BaselineConditions={};
        CollisionTotal=0U;
        HavePrevious=false;
        HasStarted=false;
    }

    CourseStage GetStage() const { return StageValue; }
    CourseRejection GetRejection() const { return LastRejection; }
    std::uint32_t GetCollisionCount() const { return CollisionTotal; }
    bool IsOfficialRecordEligible() const { return !Policy.RequireNoCollisionForOfficial || CollisionTotal==0U; }
    const CourseDefinition& GetDefinition() const { return Definition; }

private:
    static constexpr double DirectionEpsilonM=0.000001;

    CourseDefinition Definition{};
    CollisionPolicy Policy{};
    CourseStage StageValue=CourseStage::Idle;
    CourseRejection LastRejection=CourseRejection::None;
    Vec3M PreviousCenterM{};
    double PreviousClockS=0.0;
    double StartClockS=0.0;
    AttemptConditions BaselineConditions{};
    std::uint32_t CollisionTotal=0U;
    bool HavePrevious=false;
    bool HasStarted=false;

    bool IsDefinitionValid() const
    {
        if (Definition.Version==nullptr || Definition.GuardRadiusM<=0.0 || Definition.MaximumFrameStepS<=0.0 ||
            Definition.MaximumClockPhysicsGapS<0.0 || Definition.NormalTimeScaleTolerance<0.0 ||
            Definition.MaximumCoordinateMagnitudeM<=0.0 ||
            !std::isfinite(Definition.GuardRadiusM) || !std::isfinite(Definition.MaximumFrameStepS) ||
            !std::isfinite(Definition.MaximumClockPhysicsGapS) || !std::isfinite(Definition.NormalTimeScaleTolerance) ||
            !std::isfinite(Definition.MaximumCoordinateMagnitudeM))
            return false;
        if (!IsPillarValid(Definition.PillarOne) || !IsPillarValid(Definition.PillarTwo) ||
            !IsZoneValid(Definition.StartLine) || !IsZoneValid(Definition.PillarOneSide) ||
            !IsZoneValid(Definition.BetweenPillars) || !IsZoneValid(Definition.PillarTwoOppositeSide) ||
            !IsZoneValid(Definition.FinishLine))
            return false;
        return Definition.StartLine.PlaneXM<Definition.PillarOneSide.PlaneXM &&
            Definition.PillarOneSide.PlaneXM<Definition.BetweenPillars.PlaneXM &&
            Definition.BetweenPillars.PlaneXM<Definition.PillarTwoOppositeSide.PlaneXM &&
            Definition.PillarTwoOppositeSide.PlaneXM<Definition.FinishLine.PlaneXM;
    }

    static bool IsPillarValid(const VerticalPillar& Pillar)
    {
        return IsFinite(Pillar.CenterM) && std::isfinite(Pillar.RadiusM) && std::isfinite(Pillar.BaseZM) &&
            std::isfinite(Pillar.TopZM) && Pillar.RadiusM>0.0 && Pillar.TopZM>Pillar.BaseZM;
    }

    static bool IsZoneValid(const PassZone& Zone)
    {
        return std::isfinite(Zone.PlaneXM) && std::isfinite(Zone.MinimumYM) && std::isfinite(Zone.MaximumYM) &&
            std::isfinite(Zone.MinimumZM) && std::isfinite(Zone.MaximumZM) &&
            Zone.MaximumYM>Zone.MinimumYM && Zone.MaximumZM>Zone.MinimumZM;
    }

    bool IsStartObservationValid(const StartObservation& Observation) const
    {
        return IsWithinSupportedCoordinateRange(Observation.GuardCenterM) && std::isfinite(Observation.SimulationClockS) &&
            std::isfinite(Observation.TimeScale);
    }

    bool IsFrameObservationFinite(const FrameObservation& Observation) const
    {
        return IsWithinSupportedCoordinateRange(Observation.GuardCenterM) && std::isfinite(Observation.PhysicsDeltaS) &&
            std::isfinite(Observation.SimulationClockS) && std::isfinite(Observation.TimeScale);
    }

    bool IsWithinSupportedCoordinateRange(const Vec3M& Point) const
    {
        return IsFinite(Point) && std::abs(Point.XM)<=Definition.MaximumCoordinateMagnitudeM &&
            std::abs(Point.YM)<=Definition.MaximumCoordinateMagnitudeM && std::abs(Point.ZM)<=Definition.MaximumCoordinateMagnitudeM;
    }

    bool IsNormalTimeScale(const double TimeScale) const
    {
        return std::abs(TimeScale-1.0)<=Definition.NormalTimeScaleTolerance;
    }

    JudgeResult Reject(const CourseRejection Reason)
    {
        StageValue=CourseStage::Invalid;
        LastRejection=Reason;
        JudgeResult Result=MakeResult(false);
        Result.Rejection=Reason;
        return Result;
    }

    JudgeResult MakeResult(const bool Accepted) const
    {
        JudgeResult Result{};
        Result.Stage=StageValue;
        Result.Rejection=LastRejection;
        Result.AcceptedFrame=Accepted;
        Result.Finished=StageValue==CourseStage::Finished;
        Result.OfficialRecordEligible=IsOfficialRecordEligible();
        Result.CollisionCount=CollisionTotal;
        Result.ElapsedTimeS=HasStarted ? ElapsedTimeS(PreviousClockS) : 0.0;
        return Result;
    }

    double ElapsedTimeS(const double ClockS) const
    {
        if (!HasStarted || ClockS<StartClockS) return 0.0;
        return ClockS-StartClockS;
    }

    void AddCollisions(const std::uint32_t NewCollisions)
    {
        const std::uint32_t Remaining=std::numeric_limits<std::uint32_t>::max()-CollisionTotal;
        CollisionTotal=NewCollisions>Remaining ? std::numeric_limits<std::uint32_t>::max() : CollisionTotal+NewCollisions;
    }

    static bool CrossesForwardPlane(const double PreviousXM,const double CurrentXM,const double PlaneXM)
    {
        return CurrentXM>PreviousXM+DirectionEpsilonM && PreviousXM<=PlaneXM && CurrentXM>=PlaneXM;
    }

    static double CrossingFraction(const double PreviousXM,const double CurrentXM,const double PlaneXM)
    {
        const double DistanceXM=CurrentXM-PreviousXM;
        if (DistanceXM<=0.0) return 0.0;
        return (PlaneXM-PreviousXM)/DistanceXM;
    }

    Vec3M PointAtPlane(const Vec3M& CurrentCenterM,const PassZone& Zone) const
    {
        const double Fraction=CrossingFraction(PreviousCenterM.XM,CurrentCenterM.XM,Zone.PlaneXM);
        return {Zone.PlaneXM,
            PreviousCenterM.YM+(CurrentCenterM.YM-PreviousCenterM.YM)*Fraction,
            PreviousCenterM.ZM+(CurrentCenterM.ZM-PreviousCenterM.ZM)*Fraction};
    }

    double ClockAtPlane(const double CurrentClockS,const PassZone& Zone) const
    {
        const double Fraction=CrossingFraction(PreviousCenterM.XM,LastCurrentXM,Zone.PlaneXM);
        return PreviousClockS+(CurrentClockS-PreviousClockS)*Fraction;
    }

    bool IsInsideZoneWithGuardClearance(const Vec3M& Point,const PassZone& Zone) const
    {
        const double RadiusM=Definition.GuardRadiusM;
        if (Point.YM<Zone.MinimumYM+RadiusM || Point.YM>Zone.MaximumYM-RadiusM ||
            Point.ZM<Zone.MinimumZM+RadiusM || Point.ZM>Zone.MaximumZM-RadiusM)
            return false;
        return IsClearOfPillar(Point,Definition.PillarOne) && IsClearOfPillar(Point,Definition.PillarTwo);
    }

    bool IsClearOfPillar(const Vec3M& Point,const VerticalPillar& Pillar) const
    {
        if (Point.ZM<=(Pillar.BaseZM-Definition.GuardRadiusM) || Point.ZM>=(Pillar.TopZM+Definition.GuardRadiusM))
            return true;
        const double DXM=Point.XM-Pillar.CenterM.XM;
        const double DYM=Point.YM-Pillar.CenterM.YM;
        const double RequiredM=Pillar.RadiusM+Definition.GuardRadiusM;
        return DXM*DXM+DYM*DYM>RequiredM*RequiredM;
    }

    void ProcessSweptSegment(const Vec3M& CurrentCenterM,const double CurrentClockS,JudgeResult& Result)
    {
        LastCurrentXM=CurrentCenterM.XM;
        while (StageValue!=CourseStage::Invalid && StageValue!=CourseStage::Finished)
        {
            const PassZone* RequiredZone=RequiredZoneForStage();
            if (RequiredZone==nullptr) break;
            if (!CrossesForwardPlane(PreviousCenterM.XM,CurrentCenterM.XM,RequiredZone->PlaneXM))
            {
                if (CurrentCenterM.XM>RequiredZone->PlaneXM+DirectionEpsilonM) Reject(RejectedForMissedStage());
                break;
            }

            const Vec3M CrossingPointM=PointAtPlane(CurrentCenterM,*RequiredZone);
            const double CrossingClockS=ClockAtPlane(CurrentClockS,*RequiredZone);
            if (!IsInsideZoneWithGuardClearance(CrossingPointM,*RequiredZone))
            {
                Reject(RejectedForBadZone());
                break;
            }

            Result.EventClockS=CrossingClockS;
            Result.Advanced=true;
            AdvanceStage(CrossingClockS,Result);
        }
        LastCurrentXM=0.0;
    }

    const PassZone* RequiredZoneForStage() const
    {
        switch (StageValue)
        {
        case CourseStage::AwaitStart:return &Definition.StartLine;
        case CourseStage::PassPillarOneSide:return &Definition.PillarOneSide;
        case CourseStage::PassBetweenPillars:return &Definition.BetweenPillars;
        case CourseStage::PassPillarTwoOppositeSide:return &Definition.PillarTwoOppositeSide;
        case CourseStage::PassFinish:return &Definition.FinishLine;
        case CourseStage::Finished:case CourseStage::Invalid:case CourseStage::Idle:return nullptr;
        }
        return nullptr;
    }

    CourseRejection RejectedForMissedStage() const
    {
        return StageValue==CourseStage::PassPillarOneSide || StageValue==CourseStage::PassPillarTwoOppositeSide
            ? CourseRejection::WrongSide : CourseRejection::SkippedZone;
    }

    CourseRejection RejectedForBadZone() const
    {
        return StageValue==CourseStage::PassPillarOneSide || StageValue==CourseStage::PassPillarTwoOppositeSide
            ? CourseRejection::WrongSide : CourseRejection::SkippedZone;
    }

    void AdvanceStage(const double CrossingClockS,JudgeResult& Result)
    {
        switch (StageValue)
        {
        case CourseStage::AwaitStart:
            StartClockS=CrossingClockS;
            HasStarted=true;
            StageValue=CourseStage::PassPillarOneSide;
            Result.Started=true;
            break;
        case CourseStage::PassPillarOneSide:StageValue=CourseStage::PassBetweenPillars;break;
        case CourseStage::PassBetweenPillars:StageValue=CourseStage::PassPillarTwoOppositeSide;break;
        case CourseStage::PassPillarTwoOppositeSide:StageValue=CourseStage::PassFinish;break;
        case CourseStage::PassFinish:
            StageValue=CourseStage::Finished;
            Result.Finished=true;
            break;
        case CourseStage::Finished:case CourseStage::Invalid:case CourseStage::Idle:break;
        }
    }

    // Stored only while a swept segment is processed so the interpolation uses
    // the caller's current endpoint without duplicating it in persistent state.
    double LastCurrentXM=0.0;
};
}
