#pragma once
#include "AITraining.h"

namespace ImperialAI
{
enum class AttackPhase { Lift, Approach, Align, Cross, Clear, Return };
// Targets only. The rules engine, not this planner, awards goals/offside clearance.
class Attacker
{
public:
    void Reset(int Difficulty){*this=Attacker();Level=std::clamp(Difficulty,0,2);}
    Point Step(double Dt,Point Position,double SpeedMps,double UpZ,Point Goal,bool Offside,bool Recovering=false)
    {
        if(!std::isfinite(Dt)||Dt<=0||Dt>.25||!Finite(Position)||!Finite(Goal))return Target;
        Age+=Dt;PhaseAge+=Dt;
        if(Recovering)return Target;
        if(Offside&&Phase!=AttackPhase::Clear&&Phase!=AttackPhase::Return)Enter(AttackPhase::Clear);
        const Point Front{Goal.X+1.2,Goal.Y,Goal.Z};
        switch(Phase)
        {
        case AttackPhase::Lift:
            Target={6.5,0,1.2};if(Age>4&&Distance(Position,Target)<.4)Enter(AttackPhase::Approach);break;
        case AttackPhase::Approach:
            Target=Front;if(Distance(Position,Target)<.22&&SpeedMps<.35&&UpZ>.96)Enter(AttackPhase::Align);break;
        case AttackPhase::Align:
            Target=Front;Stable=Distance(Position,Front)<.06&&SpeedMps<.12&&UpZ>.98?Stable+Dt:0;
            if(Stable>.6){CrossX=Front.X;Enter(AttackPhase::Cross);}break;
        case AttackPhase::Cross:
            CrossX=std::max(Goal.X-.8,CrossX-GetCrossTargetSpeedMps()*Dt);Target={CrossX,Goal.Y,Goal.Z};
            if(Position.X<Goal.X-.55||PhaseAge>12)Enter(AttackPhase::Clear);break;
        case AttackPhase::Clear:
            Target={Goal.X-.8,Goal.Y-1.3,Goal.Z-.5};if(Distance(Position,Target)<.3)Enter(AttackPhase::Return);break;
        case AttackPhase::Return:
            Target={1.2,-1.3,2.5};if(Position.X>.55&&!Offside){++Cycles;Enter(AttackPhase::Approach);}break;
        }
        return Target;
    }
    AttackPhase GetPhase() const { return Phase; }
    int GetCycles() const { return Cycles; }
    // Planned crossing speed, not a promise about the physical body's measured speed.
    double GetCrossTargetSpeedMps() const { return .40+.15*Level; }
    bool UsesTransitLimits() const
    { return Phase==AttackPhase::Approach||Phase==AttackPhase::Clear||Phase==AttackPhase::Return; }
    double GetHorizontalSpeedLimitMps() const
    { return UsesTransitLimits()?2.35+.20*Level:2.2; }
    double GetHorizontalAccelerationLimitMps2() const
    {
        static constexpr double Acceleration[]={3.35,3.60,3.90};
        return UsesTransitLimits()?Acceleration[Level]:3.2;
    }
private:
    void Enter(AttackPhase P){Phase=P;PhaseAge=Stable=0;}
    AttackPhase Phase=AttackPhase::Lift;
    Point Target{6.5,0,1.2};
    int Level=0,Cycles=0;
    double Age=0,PhaseAge=0,Stable=0,CrossX=0;
};
}
