#include "AIAttacker.h"
#include "AIRecovery.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace ImperialAI;
int Checks=0;
void Check(bool V){++Checks;if(!V){std::printf("FAIL %d\n",Checks);std::exit(1);}}
int main()
{
    for(int Count:{3,4})for(int Level:{0,1,2})
    {
        Settings S{Count,Level,42,true};Check(S.Valid()&&S.FirstId()==5&&S.DefenderCount()==Count-1);
        Planner P;Check(P.Reset(S));for(int I=0;I<3600;++I){P.Step(1./60,{0,0,2.5},{});for(int J=0;J<S.DefenderCount();++J){const auto T=P.GetTargets()[J];Check(Finite(T)&&T.X>=4&&T.X<=6.5&&std::abs(T.Y)<3&&T.Z>=1.2&&T.Z<=3.3);}}
    }
    for(int Hz:{20,60,240})
    {
        Attacker A;A.Reset(1);Point P{6.5,0,1.2},G{-6.5,0,3},T;const double Dt=1./Hz;
        for(int I=0;I<Hz*5;++I)T=A.Step(Dt,P,0,1,G,false);Check(A.GetPhase()==AttackPhase::Approach);
        P={-5.3,0,3};for(int I=0;I<Hz;++I)T=A.Step(Dt,P,0,1,G,false);Check(A.GetPhase()==AttackPhase::Cross);
        const double Before=T.X;T=A.Step(Dt,P,0,1,G,false);Check(T.X<Before&&Before-T.X<=.551*Dt);
        T=A.Step(Dt,{-6.9,0,3},.4,1,G,true);Check(A.GetPhase()==AttackPhase::Clear&&A.GetCycles()==0);
        P={-7.3,-1.3,2.5};for(int I=0;I<Hz;++I)T=A.Step(Dt,P,0,1,G,true);Check(A.GetPhase()==AttackPhase::Return);
        for(int I=0;I<Hz;++I)T=A.Step(Dt,{1,0,2.5},0,1,G,true);Check(A.GetPhase()==AttackPhase::Return&&A.GetCycles()==0);
        T=A.Step(Dt,{1,0,2.5},0,1,G,false);Check(A.GetPhase()==AttackPhase::Approach&&A.GetCycles()==1);
        const auto Stable=T;T=A.Step(-1,{},{},1,G,false);Check(Distance(T,Stable)==0);
        T=A.Step(Dt,{std::numeric_limits<double>::quiet_NaN(),0,0},0,1,G,false);Check(Distance(T,Stable)==0);
        A.Reset(0);T=A.Step(.1,P,0,1,G,true,true);Check(A.GetPhase()==AttackPhase::Lift);
        Recovery R;for(int I=0;I<Hz*10;++I){if(R.Step(Dt,{-6,0,2.5},{-7.2,0,3},1,true,true,0,true)==RecoveryAction::Escape)break;}
        const auto Escape=R.GetTarget();Check(R.IsEscaping()&&Escape.X<0&&Escape.X>=-7.3&&Escape.X<=7.3);
    }
    // Profiles affect the target planner and transit ceilings, never the scoring gates.
    const double CrossSpeeds[]={.40,.55,.70},TransitSpeeds[]={2.35,2.55,2.75},TransitAcceleration[]={3.35,3.60,3.90};
    for(int Level:{0,1,2})for(int Hz:{20,60,240})
    {
        Attacker A;A.Reset(Level);const double Dt=1./Hz;Point P{6.5,0,1.2},G{-6.5,0,3},T;
        Check(std::abs(A.GetCrossTargetSpeedMps()-CrossSpeeds[Level])<1e-12);
        Check(!A.UsesTransitLimits()&&A.GetHorizontalSpeedLimitMps()==2.2&&A.GetHorizontalAccelerationLimitMps2()==3.2);
        for(int I=0;I<Hz*5;++I)T=A.Step(Dt,P,0,1,G,false);
        Check(A.GetPhase()==AttackPhase::Approach&&A.UsesTransitLimits());
        Check(std::abs(A.GetHorizontalSpeedLimitMps()-TransitSpeeds[Level])<1e-12);
        Check(std::abs(A.GetHorizontalAccelerationLimitMps2()-TransitAcceleration[Level])<1e-12);
        P={-5.3,0,3};T=A.Step(Dt,P,.2,1,G,false);Check(A.GetPhase()==AttackPhase::Align&&!A.UsesTransitLimits());
        for(int I=0;I<Hz;++I)T=A.Step(Dt,P,.13,1,G,false);Check(A.GetPhase()==AttackPhase::Align);
        for(int I=0;I<Hz;++I)T=A.Step(Dt,P,0,.979,G,false);Check(A.GetPhase()==AttackPhase::Align);
        for(int I=0;I<Hz;++I)T=A.Step(Dt,{P.X+.061,P.Y,P.Z},0,1,G,false);Check(A.GetPhase()==AttackPhase::Align);
        for(int I=0;I<Hz/2;++I)T=A.Step(Dt,P,0,1,G,false);Check(A.GetPhase()==AttackPhase::Align);
        for(int I=0;I<Hz/4;++I)T=A.Step(Dt,P,0,1,G,false);Check(A.GetPhase()==AttackPhase::Cross&&!A.UsesTransitLimits());
        Check(A.GetHorizontalSpeedLimitMps()==2.2&&A.GetHorizontalAccelerationLimitMps2()==3.2);
        const double Before=T.X;T=A.Step(Dt,P,0,1,G,false);
        Check(std::abs((Before-T.X)/Dt-CrossSpeeds[Level])<1e-10);
        T=A.Step(Dt,{-6.9,0,3},.4,1,G,true);Check(A.GetPhase()==AttackPhase::Clear&&A.UsesTransitLimits());
        T=A.Step(Dt,{-7.3,-1.3,2.5},0,1,G,true);Check(A.GetPhase()==AttackPhase::Return&&A.UsesTransitLimits());
        T=A.Step(Dt,{1,0,2.5},0,1,G,true);Check(A.GetCycles()==0);
        T=A.Step(Dt,{1,0,2.5},0,1,G,false);Check(A.GetCycles()==1&&A.GetPhase()==AttackPhase::Approach);
        A.Reset(Level);Check(A.GetHorizontalSpeedLimitMps()==2.2&&A.GetHorizontalAccelerationLimitMps2()==3.2);
    }
    std::printf("PASS %d attacker and duel formation checks\n",Checks);return 0;
}
