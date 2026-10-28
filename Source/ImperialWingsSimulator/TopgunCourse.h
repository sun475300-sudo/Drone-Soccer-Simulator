#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// S-01 v1 local practice course. Metres/seconds; not a FIDA competition rule.
namespace ImperialTopgun
{
struct Point {double X=0,Y=0,Z=0;};
struct Gate {double X,Y;};
inline constexpr std::array<Gate,5> Gates={{{-5,0},{-2,-1.5},{0,0},{2,1.5},{5,0}}};
inline constexpr double Radius=.2,Extent=.25,HalfWidth=.8,Bottom=.6,Top=3.0;
enum class Phase {Ready,Running,Finished,Invalid};
enum class Reason {None,MissedGate,Reverse,Contact,Clock,Motion,Disarmed,Reset,Assistance};
inline bool Finite(Point P){return std::isfinite(P.X)&&std::isfinite(P.Y)&&std::isfinite(P.Z);}
inline Point Mix(Point A,Point B,double T){return {A.X+(B.X-A.X)*T,A.Y+(B.Y-A.Y)*T,A.Z+(B.Z-A.Z)*T};}
inline bool Fits(Point P,Gate G){return std::abs(P.Y-G.Y)<HalfWidth-Radius-1e-9&&P.Z>Bottom+Radius+1e-9&&P.Z<Top-Radius-1e-9;}
class Run
{
public:
    void Reset(Point P,double Now){*this=Run();Previous=P;PreviousTime=Now;Initialized=Finite(P)&&std::isfinite(Now);}
    void Invalidate(Reason R){if(State==Phase::Ready||State==Phase::Running){State=Phase::Invalid;Failure=R;}}
    void Contact(){if(State==Phase::Running)Invalidate(Reason::Contact);}
    void Update(Point P,double Now,bool TimingOk=true)
    {
        if(State==Phase::Finished||State==Phase::Invalid)return;
        if(!Initialized||!Finite(P)||!std::isfinite(Now)||Now<=PreviousTime){Invalidate(Reason::Clock);return;}
        const double Dt=Now-PreviousTime,DX=P.X-Previous.X;
        const double Distance=std::sqrt(DX*DX+(P.Y-Previous.Y)*(P.Y-Previous.Y)+(P.Z-Previous.Z)*(P.Z-Previous.Z));
        if(!TimingOk||(State==Phase::Running&&Dt>.25)){Invalidate(Reason::Clock);return;}
        if(Distance>40.*Dt+.05){Invalidate(Reason::Motion);return;}
        // Evaluate completion crossings in motion order, including multiple gates per frame.
        for(int Order=0;Order<5;++Order)
        {
            const int I=DX>=0?Order:4-Order;const Gate G=Gates[I];auto& T=Transits[I];
            const double A=Previous.X-G.X,B=P.X-G.X;
            if(T.Direction==0)
            {
                if(A<=-Extent&&B>-Extent){T.Direction=1;T.Clear=true;}
                else if(A>=Extent&&B<Extent){T.Direction=-1;T.Clear=true;}
            }
            if(T.Direction==0)continue;
            // A partial entry followed by backing out is not a completed crossing.
            // Forget its clearance so a new approach can be judged independently.
            if((T.Direction==1&&B<=-Extent)||(T.Direction==-1&&B>=Extent)){T={};continue;}
            double U=0,V=1;
            if(std::abs(DX)>1e-12){U=(-Extent-A)/DX;V=(Extent-A)/DX;if(U>V)std::swap(U,V);U=std::clamp(U,0.,1.);V=std::clamp(V,0.,1.);}
            T.Clear&=Fits(Mix(Previous,P,U),G)&&Fits(Mix(Previous,P,V),G);
            if((T.Direction==1&&B>=Extent)||(T.Direction==-1&&B<=-Extent))
            {
                const int Direction=T.Direction;const bool Clear=T.Clear;T={};
                if(Direction<0){if(State==Phase::Running){Invalidate(Reason::Reverse);break;}continue;}
                if(I<Next)continue;
                if(I!=Next||!Clear){Invalidate(Reason::MissedGate);break;}
                const double At=PreviousTime+std::clamp((Extent-A)/DX,0.,1.)*Dt;
                if(Next==0){Start=At;State=Phase::Running;}
                Split[Next]=At-Start;++Next;
                if(Next==5){Elapsed=At-Start;State=Phase::Finished;break;}
            }
        }
        if(State==Phase::Running)Elapsed=Now-Start;
        Previous=P;PreviousTime=Now;
    }
    Phase State=Phase::Ready;
    Reason Failure=Reason::None;
    int Next=0;
    double Elapsed=0;
    std::array<double,5> Split{};
private:
    struct Transit {int Direction=0;bool Clear=true;};
    std::array<Transit,5> Transits{};
    Point Previous;
    double PreviousTime=0,Start=0;
    bool Initialized=false;
};
}
