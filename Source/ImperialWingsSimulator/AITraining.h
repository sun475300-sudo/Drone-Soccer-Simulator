#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Local defensive training tactics, independent of the SDACS route planner.
// Coordinates are metres. Seed reproduces targets, not chaotic collision outcomes.
namespace ImperialAI
{
struct Point { double X=0,Y=0,Z=0; };
struct Settings
{
    int Count=3, Difficulty=0;
    std::uint32_t Seed=1;
    bool Duel=false;
    int FirstId() const { return Duel?5:6; }
    int DefenderCount() const { return Count-(Duel?1:0); }
    bool Valid() const { return (Count==3||Count==4)&&Difficulty>=0&&Difficulty<=2&&Seed!=0; }
};
inline double Distance(Point A,Point B){return std::sqrt((A.X-B.X)*(A.X-B.X)+(A.Y-B.Y)*(A.Y-B.Y)+(A.Z-B.Z)*(A.Z-B.Z));}
inline bool Finite(Point P){return std::isfinite(P.X)&&std::isfinite(P.Y)&&std::isfinite(P.Z);}
inline double Speed(int D){return D==0?.50:(D==1?.85:1.20);}
inline double Reaction(int D){return D==0?1.65:(D==1?.80:.35);}
class Planner
{
public:
    bool Reset(Settings S)
    {
        if(!S.Valid())return false;
        Config=S;Random=S.Seed;Elapsed=0;NextDecision=0;
        static constexpr double HomeY[]={1.25,2.5,-1.25,-2.5};
        for(int I=0;I<4;++I)Targets[I]={6.5,HomeY[I],1.2};
        Desired=Targets;Decisions=0;Pattern=0;return true;
    }
    void Step(double Dt,Point Player,Point Velocity)
    {
        if(!std::isfinite(Dt)||Dt<=0||Dt>.25||!Finite(Player)||!Finite(Velocity))return;
        Elapsed+=Dt;
        if(Elapsed<4)return; // Lift vertically before approaching the defensive lanes.
        if(Elapsed>=NextDecision)
        {
            NextDecision=Elapsed+Reaction(Config.Difficulty);++Decisions;
            Pattern=static_cast<int>(Next()%3); // staggered screen, lateral patrol, tracking screen
            for(int I=0;I<Config.DefenderCount();++I)
            {
                const double Lane=Config.DefenderCount()==2?(I==0?-1.2:1.2):Config.DefenderCount()==3?(I==0?0:(I==1?1.5:-1.5)):(I<2?.6+I*1.2:-.6-(I-2)*1.2);
                const double PredictedY=Player.Y+(Config.Difficulty==2?std::clamp(Velocity.Y,-3.,3.)*.3:0);
                const double Tracking=Pattern==2?std::clamp(PredictedY,-2.,2.)*.06*(Config.Difficulty+1):0;
                const double Patrol=Pattern==1?std::sin(Elapsed*.7+I)*.18:0;
                const double Height=Pattern==0?2.25+(I%2)*.65:2.2+Unit()*.8;
                Desired[I]={4.5+Unit()*.9+(I%2)*.15,
                    Lane+std::clamp((Unit()-.5)*.12+Tracking+Patrol,-.22,.22),
                    std::clamp(Height+(Pattern==2?std::clamp(Player.Z-2.5,-1.,1.)*.25:0),1.8,3.3)};
            }
        }
        for(int I=0;I<Config.DefenderCount();++I)
        {
            const double D=Distance(Targets[I],Desired[I]);
            const double F=D>0?std::min(1.,Speed(Config.Difficulty)*Dt/D):0;
            Targets[I].X+=(Desired[I].X-Targets[I].X)*F;
            Targets[I].Y+=(Desired[I].Y-Targets[I].Y)*F;
            Targets[I].Z+=(Desired[I].Z-Targets[I].Z)*F;
        }
    }
    const std::array<Point,4>& GetTargets() const { return Targets; }
    int DecisionCount() const { return Decisions; }
    int GetPattern() const { return Pattern; }
private:
    std::uint32_t Next(){Random^=Random<<13;Random^=Random>>17;Random^=Random<<5;return Random;}
    double Unit(){return static_cast<double>(Next())/4294967295.;}
    Settings Config;
    std::uint32_t Random=1;
    double Elapsed=0,NextDecision=0;
    int Decisions=0,Pattern=0;
    std::array<Point,4> Targets{},Desired{};
};
}
