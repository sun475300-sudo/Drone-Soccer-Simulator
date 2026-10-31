#include "AIObstaclePlanner.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <vector>

namespace
{
using namespace ImperialAIPath;
int Checks=0;
int Failures=0;

void Expect(bool Good,const char* Name)
{
    ++Checks;
    if(!Good)
    {
        ++Failures;
        std::cerr<<"FAIL "<<Name<<'\n';
    }
}

CircularObstacle Aircraft(double X,double Y)
{
    CircularObstacle Result{};
    Result.CenterM={X,Y};
    Result.Kind=ObstacleKind::Aircraft;
    return Result;
}

CircularObstacle Pillar(double X,double Y,double RadiusM)
{
    CircularObstacle Result{};
    Result.CenterM={X,Y};
    Result.RadiusM=RadiusM;
    Result.Kind=ObstacleKind::Pillar;
    return Result;
}

CircularObstacle Goal(double X,double Y,double RadiusM)
{
    CircularObstacle Result{};
    Result.CenterM={X,Y};
    Result.RadiusM=RadiusM;
    Result.Kind=ObstacleKind::GoalRing;
    return Result;
}

PlanRequest BaseRequest(PointM StartM,PointM TargetM)
{
    PlanRequest Request{};
    Request.StartM=StartM;
    Request.TargetM=TargetM;
    Request.AltitudeM=2.0;
    Request.AltitudeBandMinM=0.20;
    Request.AltitudeBandMaxM=4.30;
    Request.SafetyMarginM=0.05;
    Request.WallMarginM=0.25;
    Request.ReplanCooldownS=0.25;
    Request.MaxFailedReplans=2;
    return Request;
}

double DistanceSquared(PointM A,PointM B)
{
    const double Dx=A.X-B.X;
    const double Dy=A.Y-B.Y;
    return Dx*Dx+Dy*Dy;
}

double SegmentDistanceSquared(PointM PointMValue,PointM A,PointM B)
{
    const double Dx=B.X-A.X;
    const double Dy=B.Y-A.Y;
    const double LengthSquaredM=Dx*Dx+Dy*Dy;
    if(LengthSquaredM<=0.0)return DistanceSquared(PointMValue,A);
    double Fraction=((PointMValue.X-A.X)*Dx+(PointMValue.Y-A.Y)*Dy)/LengthSquaredM;
    if(Fraction<0.0)Fraction=0.0;
    if(Fraction>1.0)Fraction=1.0;
    return DistanceSquared(PointMValue,{A.X+Dx*Fraction,A.Y+Dy*Fraction});
}

double EffectiveRadiusM(const CircularObstacle& Obstacle,double SafetyMarginM)
{
    const double BaseM=Obstacle.Kind==ObstacleKind::Aircraft?AircraftGuardRadiusM:Obstacle.RadiusM;
    return BaseM+SafetyMarginM;
}

bool SegmentClear(PointM A,PointM B,const ObstacleSet& Obstacles,double SafetyMarginM,std::size_t IgnoreIndex=MaxObstacles)
{
    for(std::size_t Index=0U;Index<Obstacles.Count;++Index)
    {
        if(Index==IgnoreIndex)continue;
        const double RadiusM=EffectiveRadiusM(Obstacles.Items[Index],SafetyMarginM);
        if(SegmentDistanceSquared(Obstacles.Items[Index].CenterM,A,B)<RadiusM*RadiusM-0.0000001)return false;
    }
    return true;
}

bool OutputClear(const PlanResult& Plan,const PlanRequest& Request)
{
    PointM PreviousM=Request.StartM;
    for(std::size_t Index=0U;Index<Plan.WaypointCount;++Index)
    {
        const bool GoalAxisLastSegment=Plan.UsesGoalAxisApproach&&Index+1U==Plan.WaypointCount;
        const std::size_t IgnoreIndex=GoalAxisLastSegment?Request.GoalAxis.GoalObstacleIndex:MaxObstacles;
        if(!SegmentClear(PreviousM,Plan.Waypoints[Index],Request.Obstacles,Request.SafetyMarginM,IgnoreIndex))return false;
        PreviousM=Plan.Waypoints[Index];
    }
    return true;
}

bool OutputInArena(const PlanResult& Plan,const PlanRequest& Request)
{
    const double MinX=-ArenaLengthM*0.5+Request.WallMarginM;
    const double MaxX=ArenaLengthM*0.5-Request.WallMarginM;
    const double MinY=-ArenaWidthM*0.5+Request.WallMarginM;
    const double MaxY=ArenaWidthM*0.5-Request.WallMarginM;
    for(std::size_t Index=0U;Index<Plan.WaypointCount;++Index)
    {
        const PointM Point=Plan.Waypoints[Index];
        if(Point.X<MinX-0.0000001||Point.X>MaxX+0.0000001||
            Point.Y<MinY-0.0000001||Point.Y>MaxY+0.0000001)return false;
    }
    return true;
}

bool SamePlan(const PlanResult& A,const PlanResult& B)
{
    if(A.Reason!=B.Reason||A.WaypointCount!=B.WaypointCount||A.UsesGoalAxisApproach!=B.UsesGoalAxisApproach||
        A.LengthM!=B.LengthM)return false;
    for(std::size_t Index=0U;Index<A.WaypointCount;++Index)
    {
        if(A.Waypoints[Index].X!=B.Waypoints[Index].X||A.Waypoints[Index].Y!=B.Waypoints[Index].Y)return false;
    }
    return true;
}

struct QueueEntry
{
    double CostM=0.0;
    std::size_t Index=0U;
};

struct QueueOrder
{
    bool operator()(const QueueEntry& A,const QueueEntry& B) const
    {
        return A.CostM>B.CostM;
    }
};

double BruteGridShortestPath(PointM StartM,PointM TargetM,const ObstacleSet& Obstacles,double SafetyMarginM)
{
    constexpr int Columns=121;
    constexpr int Rows=81;
    constexpr double StepM=0.05;
    constexpr double MinXM=-3.0;
    constexpr double MinYM=-2.0;
    const std::size_t NodeCount=static_cast<std::size_t>(Columns*Rows);
    const auto IndexOf=[](int Column,int Row)
    {
        return static_cast<std::size_t>(Row*Columns+Column);
    };
    const auto PointAt=[](int Column,int Row)
    {
        return PointM{MinXM+static_cast<double>(Column)*StepM,MinYM+static_cast<double>(Row)*StepM};
    };
    const int StartColumn=0;
    const int StartRow=40;
    const int TargetColumn=120;
    const int TargetRow=40;
    const std::size_t StartIndex=IndexOf(StartColumn,StartRow);
    const std::size_t TargetIndex=IndexOf(TargetColumn,TargetRow);
    if(DistanceSquared(StartM,PointAt(StartColumn,StartRow))>0.000000001||
        DistanceSquared(TargetM,PointAt(TargetColumn,TargetRow))>0.000000001)return std::numeric_limits<double>::infinity();

    std::vector<double> Distances(NodeCount,std::numeric_limits<double>::infinity());
    std::vector<bool> Settled(NodeCount,false);
    std::priority_queue<QueueEntry,std::vector<QueueEntry>,QueueOrder> Queue;
    Distances[StartIndex]=0.0;
    Queue.push({0.0,StartIndex});
    while(!Queue.empty())
    {
        const QueueEntry Current=Queue.top();
        Queue.pop();
        if(Settled[Current.Index])continue;
        Settled[Current.Index]=true;
        if(Current.Index==TargetIndex)return Current.CostM;
        const int Row=static_cast<int>(Current.Index/static_cast<std::size_t>(Columns));
        const int Column=static_cast<int>(Current.Index%static_cast<std::size_t>(Columns));
        const PointM CurrentPoint=PointAt(Column,Row);
        for(int OffsetY=-1;OffsetY<=1;++OffsetY)
        {
            for(int OffsetX=-1;OffsetX<=1;++OffsetX)
            {
                if(OffsetX==0&&OffsetY==0)continue;
                const int NextColumn=Column+OffsetX;
                const int NextRow=Row+OffsetY;
                if(NextColumn<0||NextColumn>=Columns||NextRow<0||NextRow>=Rows)continue;
                const std::size_t NextIndex=IndexOf(NextColumn,NextRow);
                if(Settled[NextIndex])continue;
                const PointM NextPoint=PointAt(NextColumn,NextRow);
                if(!SegmentClear(CurrentPoint,NextPoint,Obstacles,SafetyMarginM))continue;
                const double StepCostM=(OffsetX==0||OffsetY==0)?StepM:std::sqrt(2.0)*StepM;
                const double CandidateM=Current.CostM+StepCostM;
                if(CandidateM<Distances[NextIndex])
                {
                    Distances[NextIndex]=CandidateM;
                    Queue.push({CandidateM,NextIndex});
                }
            }
        }
    }
    return std::numeric_limits<double>::infinity();
}
}

int main()
{
    {
        const PlanRequest Request=BaseRequest({-6.0,0.0},{6.0,0.0});
        const PlanResult Plan=PlanWaypoints(Request);
        Expect(Plan.Succeeded(),"empty arena accepts straight path");
        Expect(Plan.WaypointCount==1U&&Plan.Waypoints[0U].X==6.0&&Plan.Waypoints[0U].Y==0.0,
            "empty arena emits only target waypoint");
        Expect(std::abs(Plan.LengthM-12.0)<0.000000001,"empty arena length is straight-line distance");
        Expect(OutputClear(Plan,Request)&&OutputInArena(Plan,Request),"empty path is clear and bounded");
    }
    {
        PlanRequest Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.Obstacles.Items[0U]=Aircraft(0.0,0.0);
        Request.Obstacles.Count=1U;
        const PlanResult Plan=PlanWaypoints(Request);
        Expect(Plan.Succeeded(),"single aircraft obstacle has a tangent route");
        Expect(Plan.WaypointCount>2U&&Plan.LengthM>6.0,"single obstacle detour has extra waypoints and length");
        Expect(OutputClear(Plan,Request)&&OutputInArena(Plan,Request),"single obstacle output preserves expanded clearance and walls");
    }
    {
        PlanRequest Request=BaseRequest({-4.0,-1.5},{4.0,1.5});
        Request.Obstacles.Items[0U]=Aircraft(-0.8,-0.35);
        Request.Obstacles.Items[1U]=Pillar(0.7,0.55,0.30);
        Request.Obstacles.Items[2U]=Aircraft(2.0,0.15);
        Request.Obstacles.Count=3U;
        const PlanResult Plan=PlanWaypoints(Request);
        Expect(Plan.Succeeded(),"multiple circular obstacles have a route");
        Expect(Plan.WaypointCount>1U&&OutputClear(Plan,Request),"multiple obstacle route clears every expanded keep-out");
        Expect(OutputInArena(Plan,Request),"multiple obstacle route stays inside wall margin");
    }
    {
        PlanRequest Request=BaseRequest({-2.0,0.0},{0.0,0.0});
        Request.Obstacles.Items[0U]=Aircraft(0.0,0.0);
        Request.Obstacles.Count=1U;
        const PlanResult Plan=PlanWaypoints(Request);
        Expect(!Plan.Succeeded()&&Plan.Reason==Rejection::TargetInsideKeepOut,"target inside an aircraft keep-out is rejected");
    }
    {
        PlanRequest Request=BaseRequest({-6.0,0.0},{6.0,0.0});
        Request.Obstacles.Items[0U]=Pillar(0.0,0.0,3.30);
        Request.Obstacles.Count=1U;
        const PlanResult Plan=PlanWaypoints(Request);
        Expect(!Plan.Succeeded()&&Plan.Reason==Rejection::NoPath,"gap narrower than expanded clearance reports no path");
    }
    {
        PlanRequest Request=BaseRequest({-3.0,0.0},{0.0,0.0});
        Request.Obstacles.Items[0U]=Goal(0.0,0.0,0.50);
        Request.Obstacles.Count=1U;
        const PlanResult Blocked=PlanWaypoints(Request);
        Expect(!Blocked.Succeeded()&&Blocked.Reason==Rejection::TargetInsideKeepOut,"goal ring is a keep-out without an explicit axis request");
        Request.GoalAxis.Requested=true;
        Request.GoalAxis.GoalObstacleIndex=0U;
        Request.GoalAxis.OutwardAxisM={-1.0,0.0};
        const PlanResult ThroughAxis=PlanWaypoints(Request);
        Expect(ThroughAxis.Succeeded()&&ThroughAxis.UsesGoalAxisApproach,"explicit goal axis approach is admitted");
        Expect(ThroughAxis.WaypointCount>=2U&&ThroughAxis.Waypoints[ThroughAxis.WaypointCount-1U].X==0.0&&
            ThroughAxis.Waypoints[ThroughAxis.WaypointCount-1U].Y==0.0,"axis approach ends at the requested goal centre");
        Expect(OutputClear(ThroughAxis,Request),"axis exception skips only the final ring-centre segment");
    }
    {
        PlanRequest Request=BaseRequest({-7.75,0.0},{7.75,0.0});
        const PlanResult InclusiveBoundary=PlanWaypoints(Request);
        Expect(InclusiveBoundary.Succeeded()&&OutputInArena(InclusiveBoundary,Request),"wall margin boundary is inclusive");
        Request.StartM.X=-7.7501;
        const PlanResult Outside=PlanWaypoints(Request);
        Expect(!Outside.Succeeded()&&Outside.Reason==Rejection::OutsideNavigableArena,"outside wall margin is rejected");
    }
    {
        PlanRequest Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.StartM.X=std::numeric_limits<double>::quiet_NaN();
        Expect(PlanWaypoints(Request).Reason==Rejection::NonFiniteInput,"non-finite start is rejected");
        Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.AltitudeM=std::numeric_limits<double>::infinity();
        Expect(PlanWaypoints(Request).Reason==Rejection::NonFiniteInput,"non-finite altitude is rejected");
        Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.Obstacles.Items[0U]=Pillar(0.0,0.0,std::numeric_limits<double>::infinity());
        Request.Obstacles.Count=1U;
        Expect(PlanWaypoints(Request).Reason==Rejection::NonFiniteInput,"non-finite obstacle radius is rejected");
        Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.SafetyMarginM=0.0;
        Expect(PlanWaypoints(Request).Reason==Rejection::InvalidSafetyMargin,"unsafe zero raster margin is rejected");
    }
    {
        PlanRequest Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.Obstacles.Items[0U]=Aircraft(0.0,0.0);
        Request.Obstacles.Items[1U]=Pillar(1.0,0.8,0.25);
        Request.Obstacles.Count=2U;
        const PlanResult First=PlanWaypoints(Request);
        const PlanResult Second=PlanWaypoints(Request);
        Expect(First.Succeeded()&&SamePlan(First,Second),"identical inputs produce bitwise-identical route output");
    }
    {
        PlanRequest Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Request.Obstacles.Items[0U]=Aircraft(0.0,0.0);
        Request.Obstacles.Count=1U;
        const PlanResult Plan=PlanWaypoints(Request);
        const double BruteLengthM=BruteGridShortestPath(Request.StartM,Request.TargetM,Request.Obstacles,Request.SafetyMarginM);
        Expect(Plan.Succeeded()&&std::isfinite(BruteLengthM),"small-case tangent and brute grid routes both exist");
        Expect(std::abs(Plan.LengthM-BruteLengthM)<0.16,"tangent path length is close to small-case brute-force optimum");
    }
    {
        const PlanRequest Request=BaseRequest({-3.0,0.0},{3.0,0.0});
        Replanner Planner;
        const PlanResult Initial=Planner.Begin(Request);
        ObstacleSet Moving{};
        Moving.Items[0U]=Aircraft(0.0,0.0);
        Moving.Count=1U;
        const UpdateCode Limited=Planner.Update(0.10,Request.StartM,0U,Moving);
        const UpdateCode Replanned=Planner.Update(0.16,Request.StartM,0U,Moving);
        PlanRequest ReplannedRequest=BaseRequest({-3.0,0.0},{3.0,0.0});
        ReplannedRequest.Obstacles=Moving;
        Expect(Initial.Succeeded()&&Initial.WaypointCount==1U,"replan scenario starts with an unblocked direct route");
        Expect(Limited==UpdateCode::BlockedRateLimited,"moving obstacle blocks path but respects cooldown");
        Expect(Replanned==UpdateCode::Replanned&&Planner.GetPlan().WaypointCount>1U,"moving obstacle triggers a force-free waypoint replan");
        Expect(OutputClear(Planner.GetPlan(),ReplannedRequest),"replanned route is clear against the moving obstacle geometry");
    }
    {
        PlanRequest Request=BaseRequest({-6.0,0.0},{6.0,0.0});
        Request.ReplanCooldownS=0.10;
        Request.MaxFailedReplans=2;
        Replanner Planner;
        const PlanResult Initial=Planner.Begin(Request);
        ObstacleSet Blocked{};
        Blocked.Items[0U]=Pillar(0.0,0.0,3.30);
        Blocked.Count=1U;
        const UpdateCode FirstFailure=Planner.Update(0.10,Request.StartM,0U,Blocked);
        const bool FirstFailureRecorded=FirstFailure==UpdateCode::ReplanFailed&&Planner.GetFailedReplans()==1;
        const UpdateCode Landing=Planner.Update(0.10,Request.StartM,0U,Blocked);
        Expect(Initial.Succeeded(),"failed-replan scenario starts with a valid route");
        Expect(FirstFailureRecorded,"first blocked replan records an explicit failure");
        Expect(Landing==UpdateCode::LandingRequested&&Planner.GetFailedReplans()==2&&Planner.IsLandingRequested()&&
            Planner.GetLastRejection()==Rejection::NoPath,"maximum failed replans signals recovery-supervisor landing");
    }

    std::cout<<"Scope: pure deterministic C++ waypoint/replanning tests; no Unreal build/runtime, physical controller, network, other-PC or human-flight verification\n";
    std::cout<<Checks<<" checks, "<<Failures<<" failures\n";
    return Failures==0?0:1;
}
