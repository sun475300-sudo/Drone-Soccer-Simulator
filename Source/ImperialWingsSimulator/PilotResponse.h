#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ImperialInput
{
struct PilotResponse
{
    std::array<double,3> Expo{0,0,0};
    // ANGLE roll/pitch: target angular-rate limit. Yaw: full-stick rate.
    std::array<double,3> RateDegrees{180,180,180};
    bool Valid() const
    {
        for(int I=0;I<3;++I)
            if(!std::isfinite(Expo[I])||Expo[I]<0||Expo[I]>.8||
               !std::isfinite(RateDegrees[I])||RateDegrees[I]<30||RateDegrees[I]>360)return false;
        return true;
    }
    double Shape(int Axis,double Value) const
    {
        if(Axis<0||Axis>2||!std::isfinite(Value)||!Valid())return 0;
        const double X=std::clamp(Value,-1.,1.),E=Expo[Axis];
        return (1-E)*X+E*X*X*X;
    }
    std::array<double,2> LimitTiltRates(double X,double Y) const
    {
        if(!Valid()||!std::isfinite(X)||!std::isfinite(Y))return {0,0};
        constexpr double Radians=3.14159265358979323846/180.;
        const double Size=std::hypot(X/(RateDegrees[0]*Radians),Y/(RateDegrees[1]*Radians));
        const double Scale=Size>1?1/Size:1;
        return {X*Scale,Y*Scale}; // Equal defaults preserve the existing combined 180 deg/s bound.
    }
};

enum class ArmRequest { None, Arm, Disarm };
struct ButtonLearner
{
    std::uint32_t Previous=0;
    void Begin(std::uint32_t Buttons){Previous=Buttons;}
    int Sample(std::uint32_t Buttons)
    {
        const auto Changed=Buttons^Previous,Rising=Buttons&Changed;Previous=Buttons;
        // Other latched switches may stay on. Learn only one new press, never a
        // held button, a release, or an ambiguous simultaneous transition.
        if(!Rising||Changed!=Rising||(Rising&(Rising-1)))return -1;
        int Index=0;while(!(Rising&(std::uint32_t(1)<<Index)))++Index;
        return Index;
    }
};
struct ArmingEdgeGate
{
    bool Primed=false,PreviousPressed=false;
    void Reset(){Primed=false;PreviousPressed=false;}
    ArmRequest Step(bool Valid,bool Pressed,bool Toggle,bool Armed,double RawThrottle)
    {
        if(!Valid){Reset();return ArmRequest::None;}
        // Every new device/input context requires the switch OFF/button released first.
        if(!Primed){if(!Pressed)Primed=true;PreviousPressed=Pressed;return ArmRequest::None;}
        const bool Rising=Pressed&&!PreviousPressed;PreviousPressed=Pressed;
        if((!Toggle&&!Pressed&&Armed)||(Toggle&&Rising&&Armed))return ArmRequest::Disarm;
        if(Rising&&!Armed&&std::isfinite(RawThrottle)&&RawThrottle>=0&&RawThrottle<=.06)return ArmRequest::Arm;
        return ArmRequest::None;
    }
};
}
