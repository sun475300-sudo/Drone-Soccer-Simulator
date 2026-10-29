#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Observation-only tutorial state. This class never issues aircraft commands.
namespace ImperialFirstFlight
{
enum class Step { Takeoff, Hover, Move, Land, Goal, FinalLand, Complete };
enum class Rejection { None, TrainingOnly, AIActive, SelectBlueStriker, KeyboardOnly,
    Autonomy, DisarmAll, LandFirst, StateInvalid, UserStopped, ThrottleNotLow, InputNotNeutral, LandingRequired };
struct Observation
{
    bool Practice=true, AIActive=false, Keyboard=true, Autonomous=false, Armed=false, AllDisarmed=true, NeutralInput=true;
    int SelectedIndex=0, TeamScore=0;
    double AltitudeM=.2, SpeedMps=0, HorizontalSpeedMps=0, VerticalSpeedMps=0, TiltDeg=0, X=0, Y=0, Throttle=0;
};
// Restart is an explicit reset of the selected aircraft only. Every other
// aircraft must be present, stationary, disarmed and manually controlled.
struct RestartOtherAircraft
{
    bool Present=false, Armed=false, Autonomous=false;
    double AltitudeM=0, SpeedMps=0;
};
inline bool CanResetSelectedAircraft(int AircraftCount,const std::array<RestartOtherAircraft,9>& Others)
{
    if(AircraftCount!=10)return false;
    for(const auto& Other:Others)
        if(!Other.Present||Other.Armed||Other.Autonomous||
            !std::isfinite(Other.AltitudeM)||!std::isfinite(Other.SpeedMps)||
            Other.AltitudeM<.05||Other.AltitudeM>.35||Other.SpeedMps<0||Other.SpeedMps>.25)
            return false;
    return true;
}
class Guide
{
public:
    bool Begin(const Observation& O)
    {
        *this=Guide();
        const Rejection Reason=Context(O);
        if(Reason!=Rejection::None){Stop(Reason);return false;}
        if(!O.AllDisarmed||O.Armed){Stop(Rejection::DisarmAll);return false;}
        if(O.AltitudeM>.35||O.SpeedMps>.25){Stop(Rejection::LandFirst);return false;}
        if(O.Throttle>.05){Stop(Rejection::ThrottleNotLow);return false;}
        if(!O.NeutralInput){Stop(Rejection::InputNotNeutral);return false;}
        Active=true;Capture(O);return true;
    }
    bool Advance(const Observation& O)
    {
        if(!Active||!Completed)return false;
        if(!Accept(O))return false;
        // A latched landing is evidence of an earlier observation, not permission
        // to continue after rearming, moving, or raising the throttle again.
        if(Current==Step::Land||Current==Step::FinalLand)
        {
            if(O.Armed||O.AltitudeM>.35||O.SpeedMps>.25)
            {LastRejection=Rejection::LandingRequired;return false;}
            if(O.Throttle>.05){LastRejection=Rejection::ThrottleNotLow;return false;}
            if(!O.NeutralInput){LastRejection=Rejection::InputNotNeutral;return false;}
        }
        switch(Current)
        {
        case Step::Takeoff:Current=Step::Hover;break;
        case Step::Hover:Current=Step::Move;break;
        case Step::Move:Current=Step::Land;break;
        case Step::Land:Current=Step::Goal;break;
        case Step::Goal:Current=Step::FinalLand;break;
        case Step::FinalLand:Current=Step::Complete;Active=false;Progress=1;LastRejection=Rejection::None;return true;
        case Step::Complete:return false;
        }
        Capture(O);return true;
    }
    void Retry(const Observation& O)
    {
        if(Active&&Accept(O))Capture(O);
    }
    void Stop(Rejection Reason=Rejection::UserStopped)
    {
        Active=false;Completed=false;Stable=Progress=0;LastRejection=Reason;
    }
    void Observe(double Dt,const Observation& O)
    {
        if(!Active)return;
        if(!std::isfinite(Dt)||Dt<=0||Dt>.25){Stop(Rejection::StateInvalid);return;}
        if(!Accept(O))return;
        if(Completed)
        {
            // Clear a recoverable next-button warning when the current inputs
            // are safe again, while retaining the latch and requiring a new click.
            if((Current==Step::Land||Current==Step::FinalLand)&&!O.Armed&&O.AltitudeM<=.35&&O.SpeedMps<=.25&&
                O.Throttle<=.05&&O.NeutralInput&&(LastRejection==Rejection::LandingRequired||
                LastRejection==Rejection::ThrottleNotLow||LastRejection==Rejection::InputNotNeutral))
                LastRejection=Rejection::None;
            return;
        }
        switch(Current)
        {
        case Step::Takeoff:Hold(O.AltitudeM>=.7,Dt,.35);break;
        case Step::Hover:
            Hold(O.AltitudeM>=.8&&O.AltitudeM<=2.3&&O.HorizontalSpeedMps<=.6&&
                std::abs(O.VerticalSpeedMps)<=.45&&O.TiltDeg<12,Dt,1.5);break;
        case Step::Move:
            Progress=O.AltitudeM>=.5?std::clamp(std::hypot(O.X-AnchorX,O.Y-AnchorY),0.,1.):0;
            Completed=Progress>=1;break;
        case Step::Land:case Step::FinalLand:Hold(!O.Armed&&O.AltitudeM<=.35&&O.SpeedMps<=.25,Dt,.5);break;
        case Step::Goal:Completed=O.TeamScore>ScoreBaseline;Progress=Completed?1.:0.;break;
        case Step::Complete:break;
        }
    }
    bool IsActive() const { return Active; }
    bool IsStepComplete() const { return Completed; }
    Step GetStep() const { return Current; }
    double GetProgress() const { return Progress; }
    Rejection GetRejection() const { return LastRejection; }
private:
    bool Active=false,Completed=false;
    Step Current=Step::Takeoff;
    Rejection LastRejection=Rejection::None;
    double Stable=0,Progress=0,AnchorX=0,AnchorY=0;
    int ScoreBaseline=0,LastScore=0;
    static Rejection Context(const Observation& O)
    {
        if(!std::isfinite(O.AltitudeM)||!std::isfinite(O.SpeedMps)||!std::isfinite(O.HorizontalSpeedMps)||
            !std::isfinite(O.VerticalSpeedMps)||!std::isfinite(O.TiltDeg)||!std::isfinite(O.X)||!std::isfinite(O.Y)||!std::isfinite(O.Throttle)||
            O.AltitudeM<0||O.SpeedMps<0||O.HorizontalSpeedMps<0||O.TiltDeg<0||O.TeamScore<0||O.Throttle<0||O.Throttle>1)return Rejection::StateInvalid;
        if(!O.Practice)return Rejection::TrainingOnly;
        if(O.AIActive)return Rejection::AIActive;
        if(O.SelectedIndex!=0)return Rejection::SelectBlueStriker;
        if(!O.Keyboard)return Rejection::KeyboardOnly;
        if(O.Autonomous)return Rejection::Autonomy;
        return Rejection::None;
    }
    bool Accept(const Observation& O)
    {
        Rejection Reason=Context(O);
        if(Reason==Rejection::None&&O.TeamScore<LastScore)Reason=Rejection::StateInvalid;
        if(Reason!=Rejection::None){Stop(Reason);return false;}
        LastScore=O.TeamScore;return true;
    }
    void Capture(const Observation& O)
    {
        Completed=false;Stable=Progress=0;AnchorX=O.X;AnchorY=O.Y;
        ScoreBaseline=LastScore=O.TeamScore;LastRejection=Rejection::None;
    }
    void Hold(bool Valid,double Dt,double Required)
    {
        Stable=Valid?Stable+Dt:0.;Progress=std::clamp(Stable/Required,0.,1.);
        if(Stable+1e-9>=Required){Completed=true;Progress=1;}
    }
};
}
