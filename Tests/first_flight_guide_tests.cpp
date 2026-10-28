#include "FirstFlightGuide.h"
#include <array>
#include <iostream>
#include <limits>

namespace
{
using namespace ImperialFirstFlight;
int Checks=0,Failures=0;
void Check(bool Good,const char* Name)
{++Checks;if(!Good){++Failures;std::cerr<<"FAIL "<<Name<<'\n';}}
void ObserveFor(Guide& G,Observation& O,double Seconds,double Dt=.05)
{for(int I=0;I<static_cast<int>(std::ceil(Seconds/Dt-1e-9));++I)G.Observe(Dt,O);}
void ToMove(Guide& G,Observation& O)
{
    Check(G.Begin(O),"start helper");O.Armed=true;O.AllDisarmed=false;O.AltitudeM=1;
    ObserveFor(G,O,.35);Check(G.Advance(O),"advance takeoff helper");
    ObserveFor(G,O,1.5);Check(G.Advance(O),"advance hover helper");
}
void ToGoal(Guide& G,Observation& O)
{
    ToMove(G,O);O.X+=1.1;G.Observe(.05,O);Check(G.Advance(O),"advance move helper");
    O.Armed=false;O.AllDisarmed=true;O.AltitudeM=.2;O.SpeedMps=0;
    ObserveFor(G,O,.5);Check(G.Advance(O),"advance landing helper");
}
}
int main()
{
    Guide Initial;Observation Ground;
    Check(!Initial.IsActive()&&!Initial.IsStepComplete()&&Initial.GetStep()==Step::Takeoff&&Initial.GetProgress()==0,"initial state inactive");
    Check(!Initial.Advance(Ground),"cannot advance before starting");
    struct GuardCase {Observation Value;Rejection Reason;};
    std::array<GuardCase,11> Guards{};
    for(auto& Case:Guards)Case.Value=Ground;
    Guards[0].Value.Practice=false;Guards[0].Reason=Rejection::TrainingOnly;
    Guards[1].Value.AIActive=true;Guards[1].Reason=Rejection::AIActive;
    Guards[2].Value.SelectedIndex=5;Guards[2].Reason=Rejection::SelectBlueStriker;
    Guards[3].Value.Keyboard=false;Guards[3].Reason=Rejection::KeyboardOnly;
    Guards[4].Value.Autonomous=true;Guards[4].Reason=Rejection::Autonomy;
    Guards[5].Value.Armed=true;Guards[5].Reason=Rejection::DisarmAll;
    Guards[6].Value.AllDisarmed=false;Guards[6].Reason=Rejection::DisarmAll;
    Guards[7].Value.AltitudeM=.35001;Guards[7].Reason=Rejection::LandFirst;
    Guards[8].Value.SpeedMps=.25001;Guards[8].Reason=Rejection::LandFirst;
    Guards[9].Value.Throttle=.05001;Guards[9].Reason=Rejection::ThrottleNotLow;
    Guards[10].Value.NeutralInput=false;Guards[10].Reason=Rejection::InputNotNeutral;
    for(const auto& Case:Guards)
    {
        Guide G;Check(!G.Begin(Case.Value)&&!G.IsActive()&&G.GetRejection()==Case.Reason,"begin rejects unsafe context");
        Check(G.GetProgress()==0&&!G.IsStepComplete(),"rejected start earns no progress");
    }
    {
        Guide G;Observation O;O.AltitudeM=.35;O.SpeedMps=.25;O.Throttle=.05;
        Check(G.Begin(O),"inclusive floor and rest speed boundary");
        O.AltitudeM=.69999;ObserveFor(G,O,1);Check(!G.IsStepComplete()&&G.GetProgress()==0,"low altitude never passes takeoff");
        O.AltitudeM=.7;ObserveFor(G,O,.3);Check(!G.IsStepComplete()&&G.GetProgress()>.8,"takeoff needs full dwell");
        O.AltitudeM=.6;G.Observe(.05,O);Check(G.GetProgress()==0,"takeoff dwell is continuous");
        O.AltitudeM=.7;ObserveFor(G,O,.35);Check(G.IsStepComplete()&&G.GetStep()==Step::Takeoff,"takeoff latches without automatic advance");
        O.AltitudeM=.2;ObserveFor(G,O,1);Check(G.IsStepComplete()&&G.GetProgress()==1,"completed step remains latched");
        G.Retry(O);Check(G.IsActive()&&!G.IsStepComplete()&&G.GetStep()==Step::Takeoff&&G.GetProgress()==0,"retry clears only current completion");
        Check(!G.Advance(O),"incomplete retried step cannot advance");
    }
    {
        Guide G;Observation O;Check(G.Begin(O),"hover setup start");O.AltitudeM=1;ObserveFor(G,O,.35);Check(G.Advance(O),"enter hover");
        ObserveFor(G,O,1.4);Check(!G.IsStepComplete(),"hover requires 1.5 continuous seconds");
        O.TiltDeg=12;G.Observe(.05,O);Check(G.GetProgress()==0,"12 degree tilt resets hover");O.TiltDeg=0;
        O.HorizontalSpeedMps=.60001;G.Observe(.05,O);Check(G.GetProgress()==0,"horizontal drift resets hover");O.HorizontalSpeedMps=.6;
        O.VerticalSpeedMps=-.45001;G.Observe(.05,O);Check(G.GetProgress()==0,"descending too fast resets hover");O.VerticalSpeedMps=-.45;
        O.AltitudeM=.79999;G.Observe(.05,O);Check(G.GetProgress()==0,"low hover altitude rejected");O.AltitudeM=2.30001;G.Observe(.05,O);Check(G.GetProgress()==0,"high hover altitude rejected");
        O.AltitudeM=2.3;ObserveFor(G,O,1.5);Check(G.IsStepComplete()&&G.GetStep()==Step::Hover,"hover boundary conditions pass");
    }
    {
        Guide G;Observation O;ToMove(G,O);O.X=.7;G.Observe(.05,O);Check(!G.IsStepComplete()&&G.GetProgress()>.69,"move measures displacement from entry");
        G.Retry(O);O.X=1.2;G.Observe(.05,O);Check(!G.IsStepComplete()&&G.GetProgress()<.51,"retry captures new move anchor");
        O.X=1.71;O.AltitudeM=.49;G.Observe(.05,O);Check(!G.IsStepComplete()&&G.GetProgress()==0,"ground movement does not count");
        O.AltitudeM=.5;G.Observe(.05,O);Check(G.IsStepComplete(),"one metre airborne displacement completes movement");
        Check(G.Advance(O)&&G.GetStep()==Step::Land,"explicit advance reaches landing");
        O.AltitudeM=.2;O.SpeedMps=0;O.Armed=true;ObserveFor(G,O,1);Check(!G.IsStepComplete(),"landing requires motors disarmed");
        O.Armed=false;O.SpeedMps=.26;ObserveFor(G,O,1);Check(!G.IsStepComplete(),"moving disarmed aircraft is not landed");
        O.SpeedMps=.25;O.AltitudeM=.35;ObserveFor(G,O,.45);Check(!G.IsStepComplete(),"landing needs full half second dwell");
        O.Armed=true;G.Observe(.05,O);O.Armed=false;ObserveFor(G,O,.45);Check(!G.IsStepComplete(),"landing dwell resets after rearming");
        G.Observe(.05,O);Check(G.IsStepComplete()&&G.GetStep()==Step::Land,"stable disarmed landing passes");
    }
    {
        Guide G;Observation O;O.TeamScore=4;ToGoal(G,O);G.Observe(.1,O);
        Check(!G.IsStepComplete()&&G.GetStep()==Step::Goal,"old score at entry cannot complete goal");
        O.TeamScore=5;G.Observe(.1,O);Check(G.IsStepComplete()&&G.IsActive(),"new approved team goal latches while awaiting explicit advance");
        G.Retry(O);G.Observe(.1,O);Check(!G.IsStepComplete(),"goal retry captures current score baseline");
        O.TeamScore=6;G.Observe(.1,O);Check(G.Advance(O),"goal advance enters final landing");
        Check(G.IsActive()&&!G.IsStepComplete()&&G.GetStep()==Step::FinalLand,"scoring is separate from safe final landing");
        Check(!G.Advance(O)&&G.GetStep()==Step::FinalLand,"double next cannot skip final landing dwell");
        ObserveFor(G,O,.45);Check(!G.IsStepComplete(),"final landing needs half second dwell");
        G.Observe(.05,O);Check(G.IsStepComplete(),"final landing dwell latches");
        O.Armed=true;
        Check(!G.Advance(O)&&G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::FinalLand&&G.GetRejection()==Rejection::LandingRequired&&G.GetProgress()==1,"rearming after final landing cannot finish tutorial");
        O.Armed=false;O.AltitudeM=.36;
        Check(!G.Advance(O)&&G.GetRejection()==Rejection::LandingRequired,"airborne final next is rejected");
        O.AltitudeM=.35;O.SpeedMps=.26;
        Check(!G.Advance(O)&&G.GetRejection()==Rejection::LandingRequired,"moving final next is rejected");
        O.SpeedMps=.25;O.Throttle=.05001;
        Check(!G.Advance(O)&&G.GetRejection()==Rejection::ThrottleNotLow,"raised throttle after final landing cannot finish");
        O.Throttle=.05;O.NeutralInput=false;
        Check(!G.Advance(O)&&G.IsActive()&&G.IsStepComplete()&&G.GetRejection()==Rejection::InputNotNeutral,"held flight input after final landing cannot finish");
        O.NeutralInput=true;G.Observe(.05,O);
        Check(G.GetRejection()==Rejection::None&&G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::FinalLand,"safe final landing observation clears warning without completing tutorial");
        Check(G.Advance(O),"safe final landing boundary allows completion");
        Check(!G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::Complete&&G.GetProgress()==1&&G.GetRejection()==Rejection::None,"complete state inactive but visibly complete");
        Check(!G.Advance(O),"completed guide cannot advance again");
    }
    {
        Guide G;Observation O;ToMove(G,O);O.X+=1.1;G.Observe(.05,O);Check(G.Advance(O),"landing gate setup");
        O.Armed=false;O.AllDisarmed=true;O.AltitudeM=.2;ObserveFor(G,O,.5);
        Check(G.IsStepComplete()&&G.GetStep()==Step::Land,"first landing latches before explicit next");
        O.Armed=true;
        Check(!G.Advance(O)&&G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::Land&&G.GetProgress()==1&&G.GetRejection()==Rejection::LandingRequired,"rearmed first landing keeps stage and evidence without stopping");
        O.Armed=false;O.AltitudeM=.35001;
        Check(!G.Advance(O)&&G.GetRejection()==Rejection::LandingRequired,"airborne first landing next rejected");
        O.AltitudeM=.35;O.SpeedMps=.25001;
        Check(!G.Advance(O)&&G.GetRejection()==Rejection::LandingRequired,"moving first landing next rejected");
        O.SpeedMps=.25;O.Throttle=.05001;
        Check(!G.Advance(O)&&G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::Land&&G.GetRejection()==Rejection::ThrottleNotLow,"high throttle cannot bypass first landing latch");
        O.Throttle=.05;G.Observe(.05,O);
        Check(G.GetRejection()==Rejection::None&&G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::Land&&G.GetProgress()==1,"safe observation clears first landing warning without advancing");
        O.NeutralInput=false;
        Check(!G.Advance(O)&&G.GetRejection()==Rejection::InputNotNeutral,"nonneutral first landing next rejected");
        O.NeutralInput=true;G.Observe(.05,O);
        Check(G.GetRejection()==Rejection::None&&G.IsStepComplete()&&G.GetStep()==Step::Land,"released inputs clear warning but preserve first landing latch");
        Check(G.Advance(O)&&G.GetStep()==Step::Goal&&G.IsActive()&&!G.IsStepComplete()&&G.GetRejection()==Rejection::None,"safe low neutral next starts fresh goal stage");
        Check(!G.Advance(O)&&G.GetStep()==Step::Goal,"double next cannot bypass a new goal");
    }
    {
        Guide G;Observation O;O.Throttle=1.00001;
        Check(!G.Begin(O)&&G.GetRejection()==Rejection::StateInvalid,"out of range throttle is invalid data");
    }
    // Sample-rate sweep checks elapsed-time requirements instead of frame counts.
    for(double Dt:{1./20,1./60,1./120,1./240})
    {
        Guide G;Observation O;Check(G.Begin(O),"sample rate start");O.AltitudeM=1;
        for(int I=0;I<static_cast<int>(std::ceil(.35/Dt));++I)G.Observe(Dt,O);
        Check(G.IsStepComplete()&&G.GetStep()==Step::Takeoff,"takeoff dwell independent of observation rate");
        Check(G.Advance(O),"rate sweep enter hover");
        for(int I=0;I<static_cast<int>(std::ceil(1.5/Dt));++I)G.Observe(Dt,O);
        Check(G.IsStepComplete()&&G.GetStep()==Step::Hover,"hover dwell independent of observation rate");
    }
    for(int Index=0;Index<5;++Index)
    {
        Guide G;Observation O;Check(G.Begin(O),"context change setup");
        if(Index==0)O.Practice=false;if(Index==1)O.AIActive=true;if(Index==2)O.SelectedIndex=1;
        if(Index==3)O.Keyboard=false;if(Index==4)O.Autonomous=true;
        G.Observe(.1,O);Check(!G.IsActive()&&G.GetRejection()==Guards[Index].Reason,"context source change stops the guide");
    }
    for(double Dt:{0.,-.1,.25001,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {
        Guide G;Observation O;Check(G.Begin(O),"bad dt setup");G.Observe(Dt,O);
        Check(!G.IsActive()&&G.GetRejection()==Rejection::StateInvalid,"invalid observation interval stops safely");
    }
    for(int Field=0;Field<8;++Field)for(double Bad:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {
        Guide G;Observation O;Check(G.Begin(O),"nonfinite setup");
        double* Fields[]={&O.AltitudeM,&O.SpeedMps,&O.HorizontalSpeedMps,&O.VerticalSpeedMps,&O.TiltDeg,&O.X,&O.Y,&O.Throttle};*Fields[Field]=Bad;
        G.Observe(.1,O);Check(!G.IsActive()&&G.GetRejection()==Rejection::StateInvalid,"nonfinite observation stops the guide");
        Check(!G.Begin(O)&&G.GetRejection()==Rejection::StateInvalid,"nonfinite observation cannot start");
    }
    for(int Field=0;Field<6;++Field)
    {
        Guide G;Observation O;
        if(Field==0)O.AltitudeM=-.01;if(Field==1)O.SpeedMps=-.01;if(Field==2)O.HorizontalSpeedMps=-.01;
        if(Field==3)O.TiltDeg=-.01;if(Field==4)O.TeamScore=-1;if(Field==5)O.Throttle=-.01;
        Check(!G.Begin(O)&&G.GetRejection()==Rejection::StateInvalid,"negative physical magnitudes and score rejected");
    }
    {
        Guide G;Observation O;O.TeamScore=7;Check(G.Begin(O),"score decrease setup");O.TeamScore=6;G.Observe(.1,O);
        Check(!G.IsActive()&&G.GetRejection()==Rejection::StateInvalid,"score decrease invalidates guide instead of awarding stale credit");
        O.TeamScore=0;Check(G.Begin(O),"fresh explicit start clears prior rejection");G.Stop();
        Check(!G.IsActive()&&G.GetRejection()==Rejection::UserStopped,"explicit stop records user action");
        G.Observe(.1,O);Check(!G.IsActive(),"observations cannot restart a stopped guide");
    }
    {
        Guide G;Observation O;Check(G.Begin(O),"advance revalidation setup");O.AltitudeM=1;ObserveFor(G,O,.35);O.Keyboard=false;
        Check(!G.Advance(O)&&!G.IsActive()&&G.GetRejection()==Rejection::KeyboardOnly,"advance revalidates current input source");
        O={};Check(G.Begin(O),"retry revalidation setup");O.AIActive=true;G.Retry(O);
        Check(!G.IsActive()&&G.GetRejection()==Rejection::AIActive,"retry revalidates current session");
    }
    // Preserve the original 190 checks above. These scenarios follow all six
    // observed tasks in order; they do not represent physical pilot/USB input.
    const int ExistingChecks=Checks,ExistingFailures=Failures;
    for(const int FPS:{20,30,60,120,240})
    {
        const double Dt=1./FPS;
        const auto Expect=[&](bool Good,const char* Name)
        {if(!Good)std::cerr<<"Sequence at "<<FPS<<" FPS: ";Check(Good,Name);};
        Guide G;Observation O;O.TeamScore=4;
        Expect(G.Begin(O),"continuous sequence starts grounded/disarmed with previous score");
        O.Armed=true;O.AllDisarmed=false;O.Throttle=.55;O.AltitudeM=.9;
        O.SpeedMps=.3;O.VerticalSpeedMps=.3;
        ObserveFor(G,O,.35,Dt);
        Expect(G.IsStepComplete()&&G.IsActive()&&G.GetStep()==Step::Takeoff,"observed takeoff waits for explicit next");
        Expect(G.Advance(O)&&G.GetStep()==Step::Hover&&!G.IsStepComplete(),"explicit next enters fresh hover");
        O.AltitudeM=1.2;O.SpeedMps=.02;O.HorizontalSpeedMps=.01;O.VerticalSpeedMps=.01;O.TiltDeg=2;
        ObserveFor(G,O,.75,Dt);
        Expect(!G.IsStepComplete()&&G.GetProgress()>.4&&G.GetProgress()<.7,"partial hover dwell is insufficient in continuous sequence");
        G.Retry(O);
        Expect(G.IsActive()&&G.GetStep()==Step::Hover&&!G.IsStepComplete()&&G.GetProgress()==0,"mid-flight hover retry clears dwell without changing task");
        ObserveFor(G,O,1.5,Dt);
        Expect(G.IsStepComplete()&&G.GetStep()==Step::Hover,"full post-retry hover dwell completes");
        Expect(G.Advance(O)&&G.GetStep()==Step::Move,"explicit next enters move");
        O.X=.6;O.Y=.2;O.SpeedMps=.3;O.HorizontalSpeedMps=.3;O.VerticalSpeedMps=0;
        G.Observe(Dt,O);
        Expect(!G.IsStepComplete()&&G.GetProgress()>.6&&G.GetProgress()<.7,"move begins from hover exit anchor");
        G.Retry(O);
        Expect(G.GetStep()==Step::Move&&G.GetProgress()==0&&!G.IsStepComplete(),"move retry captures a new anchor");
        O.X=1.;G.Observe(Dt,O);
        Expect(!G.IsStepComplete()&&G.GetProgress()<.5,"pre-retry movement cannot be reused");
        O.X=1.7;G.Observe(Dt,O);
        Expect(G.IsStepComplete()&&G.GetStep()==Step::Move,"new one metre movement completes move");
        Expect(G.Advance(O)&&G.GetStep()==Step::Land,"explicit next enters first landing");
        O.AltitudeM=.2;O.SpeedMps=O.HorizontalSpeedMps=O.VerticalSpeedMps=0;
        ObserveFor(G,O,.6,Dt);
        Expect(!G.IsStepComplete()&&G.GetProgress()==0,"touchdown with motors armed cannot finish first landing");
        O.Armed=false;O.AllDisarmed=true;O.Throttle=.3;ObserveFor(G,O,.5,Dt);
        Expect(G.IsStepComplete()&&G.GetStep()==Step::Land,"first disarmed settled landing latches");
        Expect(!G.Advance(O)&&G.IsActive()&&G.GetStep()==Step::Land&&G.GetRejection()==Rejection::ThrottleNotLow,"first landing next rechecks retained throttle");
        O.Throttle=0;O.NeutralInput=false;
        Expect(!G.Advance(O)&&G.GetStep()==Step::Land&&G.GetRejection()==Rejection::InputNotNeutral,"first landing next rechecks held flight keys");
        O.NeutralInput=true;O.Armed=true;O.AllDisarmed=false;
        Expect(!G.Advance(O)&&G.GetStep()==Step::Land&&G.GetRejection()==Rejection::LandingRequired,"first landing next rejects a later explicit rearm");
        O.Armed=false;O.AllDisarmed=true;O.AltitudeM=.4;
        Expect(!G.Advance(O)&&G.GetRejection()==Rejection::LandingRequired,"first landing next rejects current airborne position");
        O.AltitudeM=.2;O.SpeedMps=.3;O.HorizontalSpeedMps=.3;
        Expect(!G.Advance(O)&&G.GetRejection()==Rejection::LandingRequired,"first landing next rejects current movement");
        O.SpeedMps=O.HorizontalSpeedMps=0;G.Observe(Dt,O);
        Expect(G.GetRejection()==Rejection::None&&G.IsStepComplete()&&G.GetStep()==Step::Land,"safe inputs clear landing warning without auto advance");
        Expect(G.Advance(O)&&G.GetStep()==Step::Goal&&!G.IsStepComplete(),"safe explicit next enters goal with current score baseline");
        Expect(!G.Advance(O)&&G.GetStep()==Step::Goal,"double next cannot skip goal");
        O.Armed=true;O.AllDisarmed=false;O.Throttle=.55;O.AltitudeM=1.2;
        O.SpeedMps=.3;O.VerticalSpeedMps=.3;
        ObserveFor(G,O,.35,Dt);
        Expect(G.IsActive()&&G.GetStep()==Step::Goal&&!G.IsStepComplete(),"explicit rearm and second takeoff do not count the old score");
        Guide InputChanged=G;Observation OtherInput=O;OtherInput.Keyboard=false;
        InputChanged.Retry(OtherInput);
        Expect(!InputChanged.IsActive()&&InputChanged.GetRejection()==Rejection::KeyboardOnly,"goal retry rejects a changed input source");
        OtherInput.Keyboard=true;InputChanged.Observe(Dt,OtherInput);
        Expect(!InputChanged.IsActive(),"restored keyboard cannot automatically resume stopped goal guide");
        Guide ScoreReset=G;Observation ResetScore=O;ResetScore.TeamScore=0;
        ScoreReset.Observe(Dt,ResetScore);
        Expect(!ScoreReset.IsActive()&&ScoreReset.GetRejection()==Rejection::StateInvalid,"score reset during airborne goal invalidates stale progress");
        O.AltitudeM=3.;O.SpeedMps=.6;O.HorizontalSpeedMps=.6;O.VerticalSpeedMps=0;O.TeamScore=5;
        G.Observe(Dt,O);
        Expect(G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::Goal,"new airborne score latches without auto advancing");
        Expect(G.Advance(O)&&G.GetStep()==Step::FinalLand&&!G.IsStepComplete(),"explicit next after airborne goal starts separate final landing");
        Expect(!G.Advance(O)&&G.GetStep()==Step::FinalLand,"double next cannot skip final landing");
        ObserveFor(G,O,.6,Dt);
        Expect(!G.IsStepComplete()&&G.GetProgress()==0,"airborne armed goal flight cannot finish final landing");
        O.Armed=false;O.AllDisarmed=true;O.AltitudeM=1.;O.SpeedMps=.2;O.HorizontalSpeedMps=0;O.VerticalSpeedMps=-.2;
        ObserveFor(G,O,.6,Dt);
        Expect(!G.IsStepComplete(),"disarming in the air cannot finish final landing");
        O.Armed=true;O.AllDisarmed=false;O.AltitudeM=.2;O.SpeedMps=O.VerticalSpeedMps=0;
        ObserveFor(G,O,.6,Dt);
        Expect(!G.IsStepComplete(),"floor contact with motors running cannot finish final landing");
        O.Armed=false;O.AllDisarmed=true;O.SpeedMps=.3;O.HorizontalSpeedMps=.3;
        ObserveFor(G,O,.6,Dt);
        Expect(!G.IsStepComplete(),"fast disarmed floor movement cannot finish final landing");
        O.SpeedMps=O.HorizontalSpeedMps=0;ObserveFor(G,O,.25,Dt);
        Expect(!G.IsStepComplete()&&G.GetProgress()>0&&G.GetProgress()<1,"partial final landing dwell is insufficient");
        G.Retry(O);
        Expect(G.IsActive()&&G.GetStep()==Step::FinalLand&&!G.IsStepComplete()&&G.GetProgress()==0,"final landing retry discards earlier dwell");
        const int LandingFrames=static_cast<int>(std::ceil(.5/Dt-1e-9));
        for(int Frame=0;Frame<LandingFrames-1;++Frame)G.Observe(Dt,O);
        Expect(!G.IsStepComplete(),"final landing remains incomplete one observation before half second");
        G.Observe(Dt,O);
        Expect(G.IsStepComplete()&&G.IsActive()&&G.GetStep()==Step::FinalLand,"full half second final landing latches without completing guide");
        Expect(!G.Advance(O)&&G.GetRejection()==Rejection::ThrottleNotLow,"final confirmation rechecks retained flight throttle");
        O.Throttle=0;O.NeutralInput=false;
        Expect(!G.Advance(O)&&G.GetRejection()==Rejection::InputNotNeutral,"final confirmation rechecks held input");
        O.NeutralInput=true;G.Observe(Dt,O);
        Expect(G.IsStepComplete()&&G.GetStep()==Step::FinalLand&&G.GetRejection()==Rejection::None,"released final inputs still require explicit completion");
        Guide LateTimeError=G;LateTimeError.Observe(.25001,O);
        Expect(!LateTimeError.IsActive()&&LateTimeError.GetRejection()==Rejection::StateInvalid,"invalid dt still stops a latched final landing");
        Guide LateInputChange=G;Observation ChangedFinal=O;ChangedFinal.Keyboard=false;
        Expect(!LateInputChange.Advance(ChangedFinal)&&!LateInputChange.IsActive()&&LateInputChange.GetRejection()==Rejection::KeyboardOnly,"final next revalidates changed input despite landing latch");
        Expect(G.Advance(O)&&!G.IsActive()&&G.IsStepComplete()&&G.GetStep()==Step::Complete&&G.GetProgress()==1,"only safe explicit next completes all six observed tasks");
        Expect(!G.Advance(O),"completion ignores repeated next");
    }
    std::cout<<"Existing: "<<ExistingChecks<<" checks, "<<ExistingFailures<<" failures\n";
    std::cout<<"Added six-task sequences: "<<Checks-ExistingChecks<<" checks, "<<Failures-ExistingFailures<<" failures\n";
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";return Failures?1:0;
}
