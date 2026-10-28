#include "TopgunCourse.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace ImperialTopgun;
int Checks=0;
void Check(bool Value){++Checks;if(!Value){std::printf("FAIL %d\n",Checks);std::exit(1);}}
Point Path(double X)
{
    double Y=0;
    if(X>-5&&X<-2)Y=-(X+5)*.5;else if(X>=-2&&X<=2)Y=X*.75;else if(X>2&&X<5)Y=(5-X)*.5;
    return {X,Y,1.8};
}
int main()
{
    for(int Hz:{20,30,60,120,240})
    {
        Run R;R.Reset(Path(-6.5),0);
        for(int I=1;I<=Hz*12;++I){const double Now=double(I)/Hz;R.Update(Path(Now-6.5),Now);Check(R.State!=Phase::Invalid);}
        Check(R.State==Phase::Finished&&R.Next==5&&std::abs(R.Elapsed-10)<1e-8);
        for(int I=1;I<5;++I)Check(R.Split[I]>R.Split[I-1]);
        R.Contact();Check(R.State==Phase::Finished);R.Update({99,99,99},100);Check(std::abs(R.Elapsed-10)<1e-8);
        R.Reset({-6.5,0,1.8},0);for(int I=1;I<=Hz*12;++I)R.Update({double(I)/Hz-6.5,0,1.8},double(I)/Hz);Check(R.State==Phase::Invalid&&R.Failure==Reason::MissedGate);
    }
    Run R;R.Reset({-5.4,0,1.8},0);R.Update({-4.6,0,1.8},.05);Check(R.State==Phase::Running&&R.Next==1&&std::abs(R.Elapsed-.009375)<1e-9);
    R.Update({-5.4,0,1.8},.1);Check(R.State==Phase::Invalid&&R.Failure==Reason::Reverse);
    R.Reset({-5.4,0,1.8},0);R.Contact();Check(R.State==Phase::Ready);R.Update({-4.6,0,1.8},.05);R.Contact();Check(R.State==Phase::Invalid&&R.Failure==Reason::Contact);
    for(Point P:std::array<Point,3>{{{-4.6,.6,1.8},{-4.6,0,.8},{-4.6,0,2.8}}})
    {R.Reset({-5.4,P.Y,P.Z},0);R.Update(P,.05);Check(R.State==Phase::Invalid);}
    R.Reset({-5.4,0,1.8},0);R.Update({-5,0,1.8},.05);Check(R.Next==0);R.Update({-4.6,1,1.8},.1);Check(R.State==Phase::Invalid);
    R.Reset({-5.4,1,1.8},0);R.Update({-5,1,1.8},.05);R.Update({-5.4,1,1.8},.1);Check(R.State==Phase::Ready&&R.Next==0);
    R.Update({-5.4,0,1.8},.15);R.Update({-4.6,0,1.8},.2);Check(R.State==Phase::Running&&R.Next==1);
    R.Reset({-.4,0,1.8},0);R.Update({.4,0,1.8},.05);Check(R.Failure==Reason::MissedGate);
    R.Reset({-5.4,0,1.8},0);R.Update({-4.6,0,1.8},.05);R.Update({-4.5,0,1.8},.5);Check(R.Failure==Reason::Clock);
    R.Reset({-5.4,0,1.8},0);R.Update({-4.6,0,1.8},.05,false);Check(R.Failure==Reason::Clock);
    R.Reset({-5.4,0,1.8},0);R.Update({-4.6,0,1.8},0);Check(R.Failure==Reason::Clock);
    R.Reset({-5.4,0,1.8},0);R.Update({5.5,0,1.8},.01);Check(R.Failure==Reason::Motion);
    R.Reset({-5.4,0,1.8},0);R.Update({std::numeric_limits<double>::quiet_NaN(),0,0},.1);Check(R.Failure==Reason::Clock);
    std::printf("PASS %d S-01 course and timing checks\n",Checks);return 0;
}
