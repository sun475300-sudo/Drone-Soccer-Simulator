#include "LosAutoFraming.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
using namespace ImperialLos;

int Checks=0;
int Failures=0;

void Expect(const bool Good,const char* const Name)
{
    ++Checks;
    if(!Good)
    {
        ++Failures;
        std::cerr<<"FAIL "<<Name<<'\n';
    }
}

bool Near(const double Left,const double Right,const double Tolerance=0.001)
{
    return std::abs(Left-Right)<=Tolerance;
}

Settings EnabledSettings()
{
    Settings Value{};
    Value.Enabled=true;
    return Value;
}

Input StandardInput()
{
    Input Value{};
    Value.PilotEyePositionM={-9.5,0.0,1.7};
    Value.AircraftPositionM={0.0,0.0,1.5};
    Value.AircraftVelocityMps={0.0,0.0,0.0};
    Value.PilotBaseYawDeg=0.0;
    Value.PilotBasePitchDeg=0.0;
    Value.AircraftYawDeg=0.0;
    Value.AircraftUpDotWorldUp=1.0;
    Value.FloorHeightM=0.0;
    Value.CurrentHeadYawDeg=0.0;
    Value.CurrentHeadPitchDeg=0.0;
    Value.CurrentFovDeg=85.0;
    Value.DeltaSeconds=1.0/60.0;
    return Value;
}

Result StepFor(State& StateValue,Input Value,const Settings& Configuration,const int Frames)
{
    Result ResultValue{};
    for(int Frame=0;Frame<Frames;++Frame)
    {
        ResultValue=Step(Value,Configuration,StateValue);
        if(ResultValue.Status==AssistStatus::Active)
        {
            Value.CurrentHeadYawDeg=ResultValue.DesiredHeadYawDeg;
            Value.CurrentHeadPitchDeg=ResultValue.DesiredHeadPitchDeg;
            Value.CurrentFovDeg=ResultValue.DesiredFovDeg;
        }
    }
    return ResultValue;
}

Result Settle(State& StateValue,Input Value,const Settings& Configuration,const int FramesPerSecond=60)
{
    Value.DeltaSeconds=1.0/static_cast<double>(FramesPerSecond);
    return StepFor(StateValue,Value,Configuration,FramesPerSecond*4);
}
}

int main()
{
    std::cout<<"Pure C++ LOS framing math only; no Unreal build, packaged runtime, physical controller, other-PC, network or human-playtest validation.\n";

    {
        Input Value=StandardInput();
        State StateValue{};
        const Result Output=Step(Value,Settings{},StateValue);
        Expect(Output.Status==AssistStatus::Disabled&&Output.Reason==Rejection::Disabled,"default opt-in setting leaves assistance disabled");
        Expect(Near(Output.DesiredHeadYawDeg,Value.CurrentHeadYawDeg)&&Near(Output.DesiredHeadPitchDeg,Value.CurrentHeadPitchDeg)&&Near(Output.DesiredFovDeg,Value.CurrentFovDeg),"disabled assistance preserves manual LOS yaw pitch and FOV");
        Expect(!StateValue.HasFovTarget&&Near(StateValue.ManualOverrideRemainingS,0.0),"disabled assistance clears private automatic state");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        State StateValue{};
        const Result Output=Settle(StateValue,Value,Configuration);
        Expect(Output.Status==AssistStatus::Active&&Output.Reason==Rejection::None,"normal aircraft framing becomes active");
        Expect(Near(Output.DesiredHeadYawDeg,0.0,0.02),"forward aircraft keeps a forward head yaw");
        Expect(Output.DesiredFovDeg>Configuration.MinimumFovDeg&&Output.DesiredFovDeg<Configuration.MaximumFovDeg,"middle distance selects an interior FOV");
        Expect(Output.DesiredFovDeg>=Configuration.MinimumFovDeg&&Output.DesiredFovDeg<=Configuration.MaximumFovDeg,"normal result observes FOV bounds");
    }

    {
        Settings Configuration=EnabledSettings();
        Input NearInput=StandardInput();
        NearInput.AircraftPositionM={-6.5,0.0,1.7};
        NearInput.CurrentFovDeg=85.0;
        State NearState{};
        const Result NearOutput=Settle(NearState,NearInput,Configuration);
        Input FarInput=StandardInput();
        FarInput.AircraftPositionM={6.5,0.0,1.7};
        FarInput.CurrentFovDeg=40.0;
        State FarState{};
        const Result FarOutput=Settle(FarState,FarInput,Configuration);
        Expect(Near(NearOutput.HystereticTargetFovDeg,40.0),"near distance reaches existing LOS minimum FOV");
        Expect(Near(FarOutput.HystereticTargetFovDeg,85.0),"far 16 metre arena end reaches existing LOS maximum FOV");
        Expect(NearOutput.DesiredFovDeg<FarOutput.DesiredFovDeg,"distance zoom widens the view for the far end");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={6.5,0.0,1.7};
        Value.CurrentHeadYawDeg=-60.0;
        Value.CurrentFovDeg=40.0;
        Value.DeltaSeconds=0.05;
        State StateValue{};
        const Result Output=Step(Value,Configuration,StateValue);
        Expect(Output.DesiredHeadYawDeg-Value.CurrentHeadYawDeg<=Configuration.MaximumYawRateDegPerS*Value.DeltaSeconds+1e-9,"yaw changes no faster than configured rate");
        Expect(Output.DesiredFovDeg-Value.CurrentFovDeg<=Configuration.MaximumFovRateDegPerS*Value.DeltaSeconds+1e-9,"FOV changes no faster than configured rate");
        Expect(Output.DesiredHeadYawDeg>Value.CurrentHeadYawDeg&&Output.DesiredFovDeg>Value.CurrentFovDeg,"rate-limited assist still advances toward the target");
    }

    {
        Settings Configuration=EnabledSettings();
        Configuration.MaximumYawRateDegPerS=1000.0;
        Configuration.MaximumPitchRateDegPerS=1000.0;
        Configuration.MaximumFovRateDegPerS=1000.0;
        Input Value=StandardInput();
        Value.AircraftPositionM={6.5,2.0,3.0};
        Value.CurrentHeadYawDeg=-40.0;
        Value.CurrentHeadPitchDeg=-10.0;
        Value.CurrentFovDeg=40.0;
        std::array<Result,3> Outputs{};
        const std::array<int,3> FramesPerSecond{{20,60,240}};
        for(std::size_t Index=0U;Index<FramesPerSecond.size();++Index)
        {
            State StateValue{};
            Input RateInput=Value;
            RateInput.DeltaSeconds=1.0/static_cast<double>(FramesPerSecond[Index]);
            Outputs[Index]=StepFor(StateValue,RateInput,Configuration,FramesPerSecond[Index]*2);
        }
        Expect(Near(Outputs[0].DesiredHeadYawDeg,Outputs[1].DesiredHeadYawDeg,0.02)&&Near(Outputs[1].DesiredHeadYawDeg,Outputs[2].DesiredHeadYawDeg,0.02),"20 60 and 240 FPS converge to similar yaw");
        Expect(Near(Outputs[0].DesiredHeadPitchDeg,Outputs[1].DesiredHeadPitchDeg,0.02)&&Near(Outputs[1].DesiredHeadPitchDeg,Outputs[2].DesiredHeadPitchDeg,0.02),"20 60 and 240 FPS converge to similar pitch");
        Expect(Near(Outputs[0].DesiredFovDeg,Outputs[1].DesiredFovDeg,0.02)&&Near(Outputs[1].DesiredFovDeg,Outputs[2].DesiredFovDeg,0.02),"20 60 and 240 FPS converge to similar FOV");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={0.0,0.0,1.7};
        State StateValue{};
        const Result First=Step(Value,Configuration,StateValue);
        const double HeldTarget=First.HystereticTargetFovDeg;
        Value.AircraftPositionM.X+=0.10;
        const Result Nearer=Step(Value,Configuration,StateValue);
        Value.AircraftPositionM.X-=0.20;
        const Result Farther=Step(Value,Configuration,StateValue);
        Expect(Near(Nearer.HystereticTargetFovDeg,HeldTarget)&&Near(Farther.HystereticTargetFovDeg,HeldTarget),"one degree FOV hysteresis rejects small distance oscillation");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={0.0,0.0,0.10};
        Value.AircraftVelocityMps={0.0,0.0,0.0};
        Value.CurrentHeadPitchDeg=0.0;
        State StateValue{};
        const Result Output=Settle(StateValue,Value,Configuration);
        const double BottomSafeElevation=-VerticalFovDeg(Output.HystereticTargetFovDeg,Configuration.ViewportAspectRatio)*0.5+Configuration.ScreenEdgeMarginDeg;
        Expect(Output.GroundTrackingActive,"low aircraft activates ground tracking");
        Expect(Output.DesiredHeadPitchDeg<0.0,"ground tracking biases the LOS head pitch downward");
        Expect(Output.AircraftScreenElevationDeg>=BottomSafeElevation-0.05,"low aircraft remains above the bottom screen safety margin");
        Expect(Output.FloorMarginScreenElevationDeg>=BottomSafeElevation-0.05,"floor margin remains above the bottom screen safety margin");
    }

    {
        Settings Configuration=EnabledSettings();
        Configuration.LowAircraftAltitudeM=0.05;
        Input Value=StandardInput();
        Value.AircraftPositionM={0.0,0.0,0.10};
        Value.AircraftVelocityMps={1.0,0.0,0.0};
        State StateValue{};
        const Result Output=Settle(StateValue,Value,Configuration);
        Expect(Output.GroundTrackingActive,"reported fast floor roll activates tracking above the low-altitude threshold");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={1.0,0.0,1.7};
        Value.AircraftUpDotWorldUp=-1.0;
        State StateValue{};
        const Result Output=Settle(StateValue,Value,Configuration);
        Expect(Output.Status==AssistStatus::Active&&Output.AircraftInverted,"inverted aircraft remains a valid framing target");
        Expect(std::isfinite(Output.DesiredHeadYawDeg)&&std::isfinite(Output.DesiredHeadPitchDeg)&&std::isfinite(Output.DesiredFovDeg),"inverted aircraft yields finite camera controls");
    }

    {
        Settings Configuration=EnabledSettings();
        bool AllStationsAccepted=true;
        const std::array<double,5> LateralM{{0.0,-1.25,-2.50,1.25,2.50}};
        for(const int Team:{0,1})
        {
            for(const double Lateral: LateralM)
            {
                Input Value=StandardInput();
                const double Sign=Team==0?1.0:-1.0;
                Value.PilotEyePositionM={-9.5*Sign,Team==0?Lateral:-Lateral,1.7};
                Value.AircraftPositionM={-6.5*Sign,Team==0?Lateral:-Lateral,0.25};
                Value.PilotBaseYawDeg=Team==0?0.0:180.0;
                State StateValue{};
                const Result Output=Step(Value,Configuration,StateValue);
                AllStationsAccepted=AllStationsAccepted&&Output.Status==AssistStatus::Active&&std::isfinite(Output.DesiredHeadYawDeg);
            }
        }
        Expect(AllStationsAccepted,"both teams and all five fixed pilot stations frame their own near end");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={6.5,3.5,4.5};
        State StateValue{};
        const Result Output=Settle(StateValue,Value,Configuration);
        Expect(Output.Status==AssistStatus::Active&&Near(Output.HystereticTargetFovDeg,85.0),"16 by 7 by 4.5 metre arena far corner uses a valid wide FOV");
        Expect(Output.DesiredHeadYawDeg>0.0&&Output.DesiredHeadPitchDeg>0.0,"far upper right arena corner tracks right and upward");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={-9.249,0.0,1.0};
        State StateValue{};
        const Result CloseOutput=Step(Value,Configuration,StateValue);
        Expect(CloseOutput.Status==AssistStatus::Active,"aircraft just outside the minimum horizontal distance remains frameable");
        Value.AircraftPositionM.X=-9.251;
        const Result TooClose=Step(Value,Configuration,StateValue);
        Expect(TooClose.Status==AssistStatus::Rejected&&TooClose.Reason==Rejection::AircraftTooClose,"aircraft inside the minimum horizontal distance is explicitly rejected");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.PilotEyePositionM.X=std::numeric_limits<double>::quiet_NaN();
        State StateValue{};
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::NonFiniteInput,"non-finite pilot position is rejected");
        Value=StandardInput();
        Value.DeltaSeconds=0.0;
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::DeltaSecondsOutOfRange,"zero time step is rejected");
        Value=StandardInput();
        Value.CurrentFovDeg=85.1;
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::FovOutOfRange,"FOV outside existing LOS limits is rejected");
        Value=StandardInput();
        Value.CurrentHeadYawDeg=60.1;
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::AngleOutOfRange,"head yaw outside existing LOS limits is rejected");
        Value=StandardInput();
        Value.AircraftVelocityMps.X=100.1;
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::VelocityOutOfRange,"excessive velocity is rejected");
        Value=StandardInput();
        Value.AircraftPositionM.X=1000.1;
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::CoordinateOutOfRange,"out-of-range coordinate is rejected");
        Value=StandardInput();
        Value.PilotEyePositionM.Z=0.0;
        Expect(Step(Value,Configuration,StateValue).Reason==Rejection::FloorRelationInvalid,"pilot eye at floor height is rejected");
        Settings Invalid=Configuration;
        Invalid.MinimumFovDeg=85.0;
        Invalid.MaximumFovDeg=40.0;
        Expect(Step(StandardInput(),Invalid,StateValue).Reason==Rejection::InvalidConfiguration,"invalid configuration is rejected before use");
    }

    {
        Settings Configuration=EnabledSettings();
        Configuration.DirectionAssistEnabled=true;
        Input Value=StandardInput();
        Value.PilotEyePositionM={0.0,0.0,1.7};
        Value.AircraftPositionM={5.0,0.0,1.7};
        Value.AircraftYawDeg=180.0;
        State StateValue{};
        Expect(Step(Value,Configuration,StateValue).Orientation==DirectionAid::NoseTowardPilot,"direction aid identifies nose toward the pilot");
        Value.AircraftYawDeg=0.0;
        Expect(Step(Value,Configuration,StateValue).Orientation==DirectionAid::NoseAwayFromPilot,"direction aid identifies nose away from the pilot");
        Value.AircraftYawDeg=90.0;
        Expect(Step(Value,Configuration,StateValue).Orientation==DirectionAid::SideOn,"direction aid identifies left or right side-on aircraft");
        Configuration.DirectionAssistEnabled=false;
        Expect(Step(Value,Configuration,StateValue).Orientation==DirectionAid::Disabled,"direction aid remains optional when disabled");
    }

    {
        Settings Configuration=EnabledSettings();
        Input Value=StandardInput();
        Value.AircraftPositionM={6.5,0.0,1.7};
        Value.CurrentHeadYawDeg=-60.0;
        Value.CurrentFovDeg=40.0;
        Value.DeltaSeconds=0.10;
        Value.RightDragActive=true;
        State StateValue{};
        const Result Dragging=Step(Value,Configuration,StateValue);
        Expect(Dragging.Status==AssistStatus::ManualOverride&&Near(Dragging.DesiredHeadYawDeg,-60.0)&&Near(Dragging.DesiredFovDeg,40.0),"right drag suspends automatic yaw and FOV changes");
        Value.RightDragActive=false;
        Value.DeltaSeconds=0.25;
        const Result Waiting=Step(Value,Configuration,StateValue);
        Expect(Waiting.Status==AssistStatus::ManualOverride&&Waiting.ManualOverrideRemainingS>0.0,"manual override remains active before its configured timeout");
        const Result StillWaiting=Step(Value,Configuration,StateValue);
        Expect(StillWaiting.Status==AssistStatus::ManualOverride&&StillWaiting.ManualOverrideRemainingS>0.0,"manual override remains active until the complete configured timeout");
        Value.DeltaSeconds=0.25;
        const Result Returning=Step(Value,Configuration,StateValue);
        Expect(Returning.Status==AssistStatus::Active&&Returning.DesiredFovDeg>40.0&&Returning.DesiredFovDeg<=50.0+1e-9,"automatic FOV returns smoothly after manual timeout");
        Value.WheelInputActive=true;
        const Result Wheeling=Step(Value,Configuration,StateValue);
        Expect(Wheeling.Status==AssistStatus::ManualOverride&&Near(Wheeling.ManualOverrideRemainingS,Configuration.ManualOverrideDurationS),"wheel input resets the configurable manual override timeout");
    }

    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0?0:1;
}
