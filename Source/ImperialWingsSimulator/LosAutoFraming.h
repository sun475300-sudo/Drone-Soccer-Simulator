#pragma once

#include <algorithm>
#include <cmath>

// Header-only LOS camera assistance. It returns head yaw/pitch offsets and FOV
// only; the caller retains ownership of the fixed pilot camera position.
namespace ImperialLos
{
inline constexpr double Pi = 3.14159265358979323846;

struct Vec3M
{
    double X=0.0;
    double Y=0.0;
    double Z=0.0;
};

enum class Rejection
{
    None,
    Disabled,
    InvalidConfiguration,
    NonFiniteInput,
    DeltaSecondsOutOfRange,
    CoordinateOutOfRange,
    VelocityOutOfRange,
    AngleOutOfRange,
    FovOutOfRange,
    FloorRelationInvalid,
    AircraftTooClose
};

enum class AssistStatus
{
    Disabled,
    ManualOverride,
    Active,
    Rejected
};

enum class DirectionAid
{
    Disabled,
    NoseTowardPilot,
    NoseAwayFromPilot,
    SideOn,
    Indeterminate
};

// All values below are simulator selections except the 40..85 degree FOV and
// +/-60, -25..40 degree head limits, which mirror the current LOS controller.
struct Settings
{
    bool Enabled=false;
    bool DirectionAssistEnabled=false;
    double MinimumFovDeg=40.0;
    double MaximumFovDeg=85.0;
    double NearZoomDistanceM=3.0;
    double FarZoomDistanceM=16.0;
    double FovHysteresisDeg=1.0;
    double SmoothingTimeS=0.22;
    double MaximumYawRateDegPerS=120.0;
    double MaximumPitchRateDegPerS=90.0;
    double MaximumFovRateDegPerS=40.0;
    double ManualOverrideDurationS=0.75;
    double LowAircraftAltitudeM=0.75;
    double RollingAltitudeM=0.25;
    double RollingSpeedMps=0.15;
    double FloorVisibleMarginM=0.50;
    double ScreenEdgeMarginDeg=3.0;
    double ViewportAspectRatio=16.0/9.0;
    double MinimumHorizontalDistanceM=0.25;
    double MaximumCoordinateMagnitudeM=1000.0;
    double MaximumVelocityMps=100.0;
    double MaximumDeltaSeconds=0.25;
    double MinimumHeadYawDeg=-60.0;
    double MaximumHeadYawDeg=60.0;
    double MinimumHeadPitchDeg=-25.0;
    double MaximumHeadPitchDeg=40.0;
};

// PilotBase angles are the fixed station-facing rotation. Current and desired
// head angles are offsets from that rotation, matching the existing LOS path.
struct Input
{
    Vec3M PilotEyePositionM{};
    Vec3M AircraftPositionM{};
    Vec3M AircraftVelocityMps{};
    double PilotBaseYawDeg=0.0;
    double PilotBasePitchDeg=0.0;
    double AircraftYawDeg=0.0;
    double AircraftUpDotWorldUp=1.0;
    double FloorHeightM=0.0;
    double CurrentHeadYawDeg=0.0;
    double CurrentHeadPitchDeg=0.0;
    double CurrentFovDeg=85.0;
    double DeltaSeconds=1.0/60.0;
    bool RightDragActive=false;
    bool WheelInputActive=false;
    bool AircraftRollingOnFloor=false;
};

struct State
{
    bool HasFovTarget=false;
    double HystereticFovTargetDeg=85.0;
    double ManualOverrideRemainingS=0.0;
};

struct Result
{
    Rejection Reason=Rejection::None;
    AssistStatus Status=AssistStatus::Rejected;
    DirectionAid Orientation=DirectionAid::Disabled;
    double DesiredHeadYawDeg=0.0;
    double DesiredHeadPitchDeg=0.0;
    double DesiredFovDeg=0.0;
    double RawTargetFovDeg=0.0;
    double HystereticTargetFovDeg=0.0;
    double AircraftScreenElevationDeg=0.0;
    double FloorMarginScreenElevationDeg=0.0;
    double ManualOverrideRemainingS=0.0;
    bool GroundTrackingActive=false;
    bool AircraftInverted=false;
};

inline bool IsFinite(const double Value)
{
    return std::isfinite(Value);
}

inline bool IsFinite(const Vec3M& Value)
{
    return IsFinite(Value.X)&&IsFinite(Value.Y)&&IsFinite(Value.Z);
}

inline double Clamp(const double Value,const double Minimum,const double Maximum)
{
    return std::max(Minimum,std::min(Value,Maximum));
}

inline double ToDegrees(const double Radians)
{
    return Radians*(180.0/Pi);
}

inline double ToRadians(const double Degrees)
{
    return Degrees*(Pi/180.0);
}

inline double NormalizeDegrees(double Degrees)
{
    Degrees=std::fmod(Degrees+180.0,360.0);
    if(Degrees<0.0)
        Degrees+=360.0;
    return Degrees-180.0;
}

inline double ShortestAngleDeltaDeg(const double FromDeg,const double ToDeg)
{
    return NormalizeDegrees(ToDeg-FromDeg);
}

inline double VerticalFovDeg(const double HorizontalFovDeg,const double AspectRatio)
{
    const double HalfHorizontalRadians=ToRadians(HorizontalFovDeg)*0.5;
    return ToDegrees(2.0*std::atan(std::tan(HalfHorizontalRadians)/AspectRatio));
}

inline bool IsConfigurationValid(const Settings& Value)
{
    const double Values[]={
        Value.MinimumFovDeg,Value.MaximumFovDeg,Value.NearZoomDistanceM,Value.FarZoomDistanceM,
        Value.FovHysteresisDeg,Value.SmoothingTimeS,Value.MaximumYawRateDegPerS,
        Value.MaximumPitchRateDegPerS,Value.MaximumFovRateDegPerS,Value.ManualOverrideDurationS,
        Value.LowAircraftAltitudeM,Value.RollingAltitudeM,Value.RollingSpeedMps,Value.FloorVisibleMarginM,
        Value.ScreenEdgeMarginDeg,Value.ViewportAspectRatio,Value.MinimumHorizontalDistanceM,
        Value.MaximumCoordinateMagnitudeM,Value.MaximumVelocityMps,Value.MaximumDeltaSeconds,
        Value.MinimumHeadYawDeg,Value.MaximumHeadYawDeg,Value.MinimumHeadPitchDeg,Value.MaximumHeadPitchDeg
    };
    for(const double Number:Values)
        if(!IsFinite(Number))
            return false;
    return Value.MinimumFovDeg>=1.0&&Value.MaximumFovDeg<=179.0&&Value.MinimumFovDeg<Value.MaximumFovDeg&&
        Value.NearZoomDistanceM>0.0&&Value.FarZoomDistanceM>Value.NearZoomDistanceM&&
        Value.FovHysteresisDeg>=0.0&&Value.FovHysteresisDeg<=30.0&&
        Value.SmoothingTimeS>0.0&&Value.SmoothingTimeS<=10.0&&
        Value.MaximumYawRateDegPerS>0.0&&Value.MaximumYawRateDegPerS<=10000.0&&
        Value.MaximumPitchRateDegPerS>0.0&&Value.MaximumPitchRateDegPerS<=10000.0&&
        Value.MaximumFovRateDegPerS>0.0&&Value.MaximumFovRateDegPerS<=10000.0&&
        Value.ManualOverrideDurationS>=0.0&&Value.ManualOverrideDurationS<=30.0&&
        Value.LowAircraftAltitudeM>=0.0&&Value.LowAircraftAltitudeM<=20.0&&
        Value.RollingAltitudeM>=0.0&&Value.RollingAltitudeM<=20.0&&
        Value.RollingSpeedMps>=0.0&&Value.RollingSpeedMps<=100.0&&
        Value.FloorVisibleMarginM>=0.0&&Value.FloorVisibleMarginM<=20.0&&
        Value.ScreenEdgeMarginDeg>=0.0&&Value.ScreenEdgeMarginDeg<45.0&&
        Value.ViewportAspectRatio>=0.25&&Value.ViewportAspectRatio<=5.0&&
        Value.MinimumHorizontalDistanceM>0.0&&Value.MinimumHorizontalDistanceM<=10.0&&
        Value.MaximumCoordinateMagnitudeM>=20.0&&Value.MaximumCoordinateMagnitudeM<=1000000.0&&
        Value.MaximumVelocityMps>0.0&&Value.MaximumVelocityMps<=10000.0&&
        Value.MaximumDeltaSeconds>0.0&&Value.MaximumDeltaSeconds<=1.0&&
        Value.MinimumHeadYawDeg>=-180.0&&Value.MaximumHeadYawDeg<=180.0&&
        Value.MinimumHeadYawDeg<Value.MaximumHeadYawDeg&&
        Value.MinimumHeadPitchDeg>=-89.0&&Value.MaximumHeadPitchDeg<=89.0&&
        Value.MinimumHeadPitchDeg<Value.MaximumHeadPitchDeg;
}

inline bool IsCoordinateInRange(const Vec3M& Value,const double MaximumMagnitudeM)
{
    return std::abs(Value.X)<=MaximumMagnitudeM&&std::abs(Value.Y)<=MaximumMagnitudeM&&
        std::abs(Value.Z)<=MaximumMagnitudeM;
}

inline bool IsVelocityInRange(const Vec3M& Value,const double MaximumVelocityMps)
{
    return std::abs(Value.X)<=MaximumVelocityMps&&std::abs(Value.Y)<=MaximumVelocityMps&&
        std::abs(Value.Z)<=MaximumVelocityMps;
}

inline Result Rejected(const Rejection Reason)
{
    Result Value{};
    Value.Reason=Reason;
    Value.Status=AssistStatus::Rejected;
    return Value;
}

inline Result CurrentView(const Input& Value,const AssistStatus Status,const Rejection Reason,const State& AssistState)
{
    Result ResultValue{};
    ResultValue.Reason=Reason;
    ResultValue.Status=Status;
    ResultValue.DesiredHeadYawDeg=Value.CurrentHeadYawDeg;
    ResultValue.DesiredHeadPitchDeg=Value.CurrentHeadPitchDeg;
    ResultValue.DesiredFovDeg=Value.CurrentFovDeg;
    ResultValue.RawTargetFovDeg=Value.CurrentFovDeg;
    ResultValue.HystereticTargetFovDeg=Value.CurrentFovDeg;
    ResultValue.ManualOverrideRemainingS=AssistState.ManualOverrideRemainingS;
    return ResultValue;
}

inline double SmoothToward(const double Current,const double Target,const double DeltaSeconds,
    const double SmoothingTimeS,const double MaximumRatePerS)
{
    const double Interpolation=1.0-std::exp(-DeltaSeconds/SmoothingTimeS);
    const double SmoothedDelta=(Target-Current)*Interpolation;
    const double MaximumDelta=MaximumRatePerS*DeltaSeconds;
    return Current+Clamp(SmoothedDelta,-MaximumDelta,MaximumDelta);
}

inline DirectionAid DetermineDirectionAid(const Input& Value,const Settings& Configuration,
    const double HorizontalDistanceM)
{
    if(!Configuration.DirectionAssistEnabled)
        return DirectionAid::Disabled;
    if(HorizontalDistanceM<Configuration.MinimumHorizontalDistanceM)
        return DirectionAid::Indeterminate;
    const double ToPilotX=(Value.PilotEyePositionM.X-Value.AircraftPositionM.X)/HorizontalDistanceM;
    const double ToPilotY=(Value.PilotEyePositionM.Y-Value.AircraftPositionM.Y)/HorizontalDistanceM;
    const double NoseX=std::cos(ToRadians(Value.AircraftYawDeg));
    const double NoseY=std::sin(ToRadians(Value.AircraftYawDeg));
    const double Dot=NoseX*ToPilotX+NoseY*ToPilotY;
    const double Threshold=std::sqrt(0.5);
    if(Dot>=Threshold)
        return DirectionAid::NoseTowardPilot;
    if(Dot<=-Threshold)
        return DirectionAid::NoseAwayFromPilot;
    return DirectionAid::SideOn;
}

inline Result Step(const Input& Value,const Settings& Configuration,State& AssistState)
{
    if(!IsConfigurationValid(Configuration))
        return Rejected(Rejection::InvalidConfiguration);
    if(!IsFinite(Value.PilotEyePositionM)||!IsFinite(Value.AircraftPositionM)||!IsFinite(Value.AircraftVelocityMps)||
        !IsFinite(Value.PilotBaseYawDeg)||!IsFinite(Value.PilotBasePitchDeg)||!IsFinite(Value.AircraftYawDeg)||
        !IsFinite(Value.AircraftUpDotWorldUp)||!IsFinite(Value.FloorHeightM)||!IsFinite(Value.CurrentHeadYawDeg)||
        !IsFinite(Value.CurrentHeadPitchDeg)||!IsFinite(Value.CurrentFovDeg)||!IsFinite(Value.DeltaSeconds))
        return Rejected(Rejection::NonFiniteInput);
    if(Value.DeltaSeconds<=0.0||Value.DeltaSeconds>Configuration.MaximumDeltaSeconds)
        return Rejected(Rejection::DeltaSecondsOutOfRange);
    if(!IsCoordinateInRange(Value.PilotEyePositionM,Configuration.MaximumCoordinateMagnitudeM)||
        !IsCoordinateInRange(Value.AircraftPositionM,Configuration.MaximumCoordinateMagnitudeM)||
        std::abs(Value.FloorHeightM)>Configuration.MaximumCoordinateMagnitudeM)
        return Rejected(Rejection::CoordinateOutOfRange);
    if(!IsVelocityInRange(Value.AircraftVelocityMps,Configuration.MaximumVelocityMps))
        return Rejected(Rejection::VelocityOutOfRange);
    if(std::abs(Value.PilotBaseYawDeg)>720.0||std::abs(Value.PilotBasePitchDeg)>180.0||
        std::abs(Value.AircraftYawDeg)>720.0||Value.AircraftUpDotWorldUp<-1.0||Value.AircraftUpDotWorldUp>1.0||
        Value.CurrentHeadYawDeg<Configuration.MinimumHeadYawDeg||Value.CurrentHeadYawDeg>Configuration.MaximumHeadYawDeg||
        Value.CurrentHeadPitchDeg<Configuration.MinimumHeadPitchDeg||Value.CurrentHeadPitchDeg>Configuration.MaximumHeadPitchDeg)
        return Rejected(Rejection::AngleOutOfRange);
    if(Value.CurrentFovDeg<Configuration.MinimumFovDeg||Value.CurrentFovDeg>Configuration.MaximumFovDeg)
        return Rejected(Rejection::FovOutOfRange);
    if(Value.PilotEyePositionM.Z<=Value.FloorHeightM||Value.AircraftPositionM.Z<Value.FloorHeightM-1.0)
        return Rejected(Rejection::FloorRelationInvalid);

    const double DeltaX=Value.AircraftPositionM.X-Value.PilotEyePositionM.X;
    const double DeltaY=Value.AircraftPositionM.Y-Value.PilotEyePositionM.Y;
    const double HorizontalDistanceM=std::hypot(DeltaX,DeltaY);
    if(HorizontalDistanceM<Configuration.MinimumHorizontalDistanceM)
        return Rejected(Rejection::AircraftTooClose);

    if(!Configuration.Enabled)
    {
        AssistState=State{};
        return CurrentView(Value,AssistStatus::Disabled,Rejection::Disabled,AssistState);
    }

    if(Value.RightDragActive||Value.WheelInputActive)
    {
        AssistState.HasFovTarget=true;
        AssistState.HystereticFovTargetDeg=Value.CurrentFovDeg;
        AssistState.ManualOverrideRemainingS=Configuration.ManualOverrideDurationS;
        return CurrentView(Value,AssistStatus::ManualOverride,Rejection::None,AssistState);
    }

    if(AssistState.ManualOverrideRemainingS>0.0)
    {
        AssistState.ManualOverrideRemainingS=std::max(0.0,AssistState.ManualOverrideRemainingS-Value.DeltaSeconds);
        if(AssistState.ManualOverrideRemainingS>0.0)
            return CurrentView(Value,AssistStatus::ManualOverride,Rejection::None,AssistState);
    }

    const double AircraftAltitudeM=Value.AircraftPositionM.Z-Value.FloorHeightM;
    const double HorizontalSpeedMps=std::hypot(Value.AircraftVelocityMps.X,Value.AircraftVelocityMps.Y);
    const bool GroundTracking=AircraftAltitudeM<=Configuration.LowAircraftAltitudeM||
        (AircraftAltitudeM<=Configuration.RollingAltitudeM&&HorizontalSpeedMps>=Configuration.RollingSpeedMps)||
        Value.AircraftRollingOnFloor;
    const double DistanceFraction=Clamp((HorizontalDistanceM-Configuration.NearZoomDistanceM)/
        (Configuration.FarZoomDistanceM-Configuration.NearZoomDistanceM),0.0,1.0);
    double RawTargetFovDeg=Configuration.MinimumFovDeg+
        (Configuration.MaximumFovDeg-Configuration.MinimumFovDeg)*DistanceFraction;

    const double AircraftWorldPitchDeg=ToDegrees(std::atan2(Value.AircraftPositionM.Z-Value.PilotEyePositionM.Z,
        HorizontalDistanceM));
    const double FloorMarginWorldPitchDeg=ToDegrees(std::atan2(
        Value.FloorHeightM+Configuration.FloorVisibleMarginM-Value.PilotEyePositionM.Z,HorizontalDistanceM));
    if(GroundTracking)
    {
        const double RequiredVerticalFovDeg=std::abs(AircraftWorldPitchDeg-FloorMarginWorldPitchDeg)+
            2.0*Configuration.ScreenEdgeMarginDeg;
        const double RequiredHorizontalFovDeg=ToDegrees(2.0*std::atan(
            std::tan(ToRadians(RequiredVerticalFovDeg)*0.5)*Configuration.ViewportAspectRatio));
        RawTargetFovDeg=std::max(RawTargetFovDeg,RequiredHorizontalFovDeg);
        RawTargetFovDeg=Clamp(RawTargetFovDeg,Configuration.MinimumFovDeg,Configuration.MaximumFovDeg);
    }

    if(!AssistState.HasFovTarget||std::abs(RawTargetFovDeg-AssistState.HystereticFovTargetDeg)>=
        Configuration.FovHysteresisDeg)
    {
        AssistState.HasFovTarget=true;
        AssistState.HystereticFovTargetDeg=RawTargetFovDeg;
    }

    const double TargetWorldYawDeg=ToDegrees(std::atan2(DeltaY,DeltaX));
    const double TargetHeadYawDeg=Clamp(ShortestAngleDeltaDeg(Value.PilotBaseYawDeg,TargetWorldYawDeg),
        Configuration.MinimumHeadYawDeg,Configuration.MaximumHeadYawDeg);
    double TargetWorldPitchDeg=AircraftWorldPitchDeg;
    if(GroundTracking)
    {
        const double VerticalFovHalfDeg=VerticalFovDeg(AssistState.HystereticFovTargetDeg,
            Configuration.ViewportAspectRatio)*0.5;
        const double FloorLimitedWorldPitchDeg=FloorMarginWorldPitchDeg+VerticalFovHalfDeg-
            Configuration.ScreenEdgeMarginDeg;
        const double DownBiasedWorldPitchDeg=AircraftWorldPitchDeg-Configuration.ScreenEdgeMarginDeg;
        TargetWorldPitchDeg=std::min(DownBiasedWorldPitchDeg,FloorLimitedWorldPitchDeg);
    }
    const double TargetHeadPitchDeg=Clamp(TargetWorldPitchDeg-Value.PilotBasePitchDeg,
        Configuration.MinimumHeadPitchDeg,Configuration.MaximumHeadPitchDeg);

    Result ResultValue{};
    ResultValue.Reason=Rejection::None;
    ResultValue.Status=AssistStatus::Active;
    ResultValue.Orientation=DetermineDirectionAid(Value,Configuration,HorizontalDistanceM);
    ResultValue.DesiredHeadYawDeg=SmoothToward(Value.CurrentHeadYawDeg,TargetHeadYawDeg,Value.DeltaSeconds,
        Configuration.SmoothingTimeS,Configuration.MaximumYawRateDegPerS);
    ResultValue.DesiredHeadPitchDeg=SmoothToward(Value.CurrentHeadPitchDeg,TargetHeadPitchDeg,Value.DeltaSeconds,
        Configuration.SmoothingTimeS,Configuration.MaximumPitchRateDegPerS);
    ResultValue.DesiredFovDeg=SmoothToward(Value.CurrentFovDeg,AssistState.HystereticFovTargetDeg,
        Value.DeltaSeconds,Configuration.SmoothingTimeS,Configuration.MaximumFovRateDegPerS);
    ResultValue.RawTargetFovDeg=RawTargetFovDeg;
    ResultValue.HystereticTargetFovDeg=AssistState.HystereticFovTargetDeg;
    ResultValue.AircraftScreenElevationDeg=AircraftWorldPitchDeg-
        (Value.PilotBasePitchDeg+ResultValue.DesiredHeadPitchDeg);
    ResultValue.FloorMarginScreenElevationDeg=FloorMarginWorldPitchDeg-
        (Value.PilotBasePitchDeg+ResultValue.DesiredHeadPitchDeg);
    ResultValue.ManualOverrideRemainingS=AssistState.ManualOverrideRemainingS;
    ResultValue.GroundTrackingActive=GroundTracking;
    ResultValue.AircraftInverted=Value.AircraftUpDotWorldUp<0.0;
    return ResultValue;
}
}
