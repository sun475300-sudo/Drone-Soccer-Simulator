#include "PilotResponse.h"
#include <iostream>
#include <limits>
int main()
{
    using namespace ImperialInput;int Checks=0,Failures=0;
    auto Check=[&](bool Good,const char* Name){++Checks;if(!Good){++Failures;std::cerr<<"FAIL "<<Name<<'\n';}};
    PilotResponse P;
    Check(P.Valid(),"defaults");
    for(int Axis=0;Axis<3;++Axis)for(double E:{0.,.2,.5,.8})
    {
        P.Expo[Axis]=E;double Prior=-2;
        for(int I=0;I<=200;++I)
        {
            const double X=I/100.-1,Y=P.Shape(Axis,X);
            Check(Y>=Prior&&Y>=-1&&Y<=1,"monotone bounded response");Prior=Y;
            Check(std::abs(Y+P.Shape(Axis,-X))<1e-12,"odd symmetry");
        }
        Check(P.Shape(Axis,-1)==-1&&P.Shape(Axis,1)==1&&P.Shape(Axis,0)==0,"endpoints neutral");
        Check(std::abs(P.Shape(Axis,.5)-(.5-.375*E))<1e-12,"independent half stick oracle");
    }
    P={};const auto Pair=P.LimitTiltRates(4,3);
    Check(std::abs(std::hypot(Pair[0],Pair[1])-3.141592653589793)<1e-12,"legacy combined tilt cap");
    P.RateDegrees={30,90,360};const auto Mixed=P.LimitTiltRates(2,2);
    Check(std::abs(Mixed[0]-Mixed[1])<1e-12&&Mixed[0]<=3.141592653589793/6,"axis limits preserve direction");
    P.Expo[0]=std::numeric_limits<double>::quiet_NaN();Check(!P.Valid()&&P.Shape(0,.5)==0,"NaN config fail closed");
    P={};Check(P.Shape(0,std::numeric_limits<double>::infinity())==0,"nonfinite sample");
    P.RateDegrees[1]=0;Check(!P.Valid(),"rate bounds");
    ButtonLearner Learner;
    Learner.Begin(0x80);Check(Learner.Sample(0x80)==-1,"held unrelated switch ignored");
    Check(Learner.Sample(0x88)==3,"new press learned with unrelated switch held");
    Learner.Begin(8);Check(Learner.Sample(8)==-1,"held target at learning entry ignored");
    Check(Learner.Sample(0)==-1&&Learner.Sample(8)==3,"release and fresh press learned");
    Learner.Begin(0);Check(Learner.Sample(3)==-1&&Learner.Sample(1)==-1,"ambiguous chord and partial release ignored");
    Check(Learner.Sample(0)==-1&&Learner.Sample(2)==1,"fresh single press after ambiguous chord");
    Learner.Begin(1);Check(Learner.Sample(2)==-1,"simultaneous release and press ignored");
    Learner.Begin(0);Check(Learner.Sample(0x80000000u)==31,"highest supported button");
    for(bool Toggle:{false,true})
    {
        ArmingEdgeGate Gate;
        Check(Gate.Step(true,true,Toggle,false,0)==ArmRequest::None,"held at startup cannot arm");
        Check(Gate.Step(true,false,Toggle,false,0)==ArmRequest::None,"release primes");
        Check(Gate.Step(true,true,Toggle,false,.9)==ArmRequest::None,"high throttle blocked");
        Check(Gate.Step(true,true,Toggle,false,0)==ArmRequest::None,"lowering while held cannot arm");
        Gate.Step(true,false,Toggle,false,0);
        Check(Gate.Step(true,true,Toggle,false,0)==ArmRequest::Arm,"fresh low edge arms");
        Check(Gate.Step(true,true,Toggle,true,1)==ArmRequest::None,"held does not repeat");
        Check(Gate.Step(true,false,Toggle,true,1)==(Toggle?ArmRequest::None:ArmRequest::Disarm),"switch off disarms regardless throttle");
        if(Toggle)Check(Gate.Step(true,true,Toggle,true,1)==ArmRequest::Disarm,"toggle disarms at high throttle");
        Gate.Step(false,true,Toggle,false,0);
        Check(Gate.Step(true,true,Toggle,false,0)==ArmRequest::None,"reconnect needs release");
        Gate.Step(true,false,Toggle,false,0);
        Check(Gate.Step(true,true,Toggle,false,std::numeric_limits<double>::quiet_NaN())==ArmRequest::None,"nonfinite throttle blocks arm");
    }
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";return Failures?1:0;
}
