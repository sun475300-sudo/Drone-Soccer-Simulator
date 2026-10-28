#include "AITraining.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
int Checks=0;
void Check(bool Value){++Checks;if(!Value){std::printf("FAIL check %d\n",Checks);std::exit(1);}}
int main()
{
    using namespace ImperialAI;
    Check(!Settings{2,0,1}.Valid());Check(!Settings{5,0,1}.Valid());Check(!Settings{3,-1,1}.Valid());Check(!Settings{3,3,1}.Valid());Check(!Settings{3,0,0}.Valid());
    Check(Speed(0)<Speed(1)&&Speed(1)<Speed(2));Check(Reaction(0)>Reaction(1)&&Reaction(1)>Reaction(2));
    const double Speeds[]={.50,.85,1.20},Reactions[]={1.65,.80,.35};
    int Decisions[3]={};
    for(int Level=0;Level<3;++Level)
    {
        Check(std::abs(Speed(Level)-Speeds[Level])<1e-12);
        Check(std::abs(Reaction(Level)-Reactions[Level])<1e-12);
        Planner P;Check(P.Reset({3,Level,1}));
        for(int I=0;I<15;++I)P.Step(.25,{0,0,2.5},{});
        const auto Before=P.GetTargets();P.Step(.25,{0,0,2.5},{});
        for(int I=0;I<3;++I)Check(std::abs(Distance(Before[I],P.GetTargets()[I])-Speed(Level)*.25)<1e-10);
        for(int I=0;I<12;++I)P.Step(.25,{0,0,2.5},{});
        Decisions[Level]=P.DecisionCount();
    }
    Check(Decisions[0]<Decisions[1]&&Decisions[1]<Decisions[2]);
    for(int Count=3;Count<=4;++Count)for(int Level=0;Level<3;++Level)for(std::uint32_t Seed=1;Seed<=32;++Seed)
    {
        Planner A,B;Check(A.Reset({Count,Level,Seed})&&B.Reset({Count,Level,Seed}));
        for(int N=0;N<1200;++N)
        {
            const auto Before=A.GetTargets();Point Player{std::sin(N*.01)*8,std::cos(N*.02)*4,2+std::sin(N*.03)*2};
            A.Step(.05,Player,{1,-2,.3});B.Step(.05,Player,{1,-2,.3});
            for(int I=0;I<Count;++I)
            {
                const auto P=A.GetTargets()[I];Check(Finite(P)&&P.X>=4.4&&P.X<=6.5&&std::abs(P.Y)<=2.5&&P.Z>=1.2&&P.Z<=3.3);
                Check(Distance(P,B.GetTargets()[I])==0);Check(Distance(P,Before[I])<=Speed(Level)*.05+1e-10);
                // Lane spacing applies after the approach, not to physical bodies.
                if(N>800)for(int J=0;J<I;++J)Check(Distance(P,A.GetTargets()[J])>.70);
            }
        }
        Check(A.DecisionCount()>25);const auto Old=A.GetTargets();
        Check(!A.Reset({4,9,Seed}));A.Step(-1,{},{});A.Step(.5,{},{});A.Step(.05,{std::numeric_limits<double>::quiet_NaN(),0,0},{});
        for(int I=0;I<4;++I)Check(Distance(Old[I],A.GetTargets()[I])==0);
    }
    Planner A,B;A.Reset({4,2,1});B.Reset({4,2,2});for(int I=0;I<1000;++I){A.Step(.05,{5,1,3},{});B.Step(.05,{5,1,3},{});}Check(Distance(A.GetTargets()[0],B.GetTargets()[0])>.01);
    std::printf("PASS %d AI planner checks (no physical flight claims)\n",Checks);return 0;
}
