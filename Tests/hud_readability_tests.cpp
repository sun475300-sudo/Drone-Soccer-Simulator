#include "HUDReadability.h"
#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <cmath>

int main()
{
    int Checks=0,Failures=0;
    const auto Check=[&](bool Ok,const char* Name)
    {++Checks;if(!Ok){++Failures;std::cerr<<"FAIL "<<Name<<"\n";}};
    // Verify the viewport pixel contract across actual small/native/large canvases.
    for(float Scale:std::array<float,7>{.4f,.6f,.72f,.8f,1.f,1.08f,1.5f})
        for(float Requested:std::array<float,8>{9,10,11,12,13,16,24,43})
        {
            const float Actual=ImperialHUDReadability::LogicalTextSize(Requested,Scale);
            Check(Actual*Scale>=9.999f,"minimum10viewportpixels");
            Check(Actual>=Requested,"neverreduceoriginalsize");
            if(Requested*Scale>=10.f)Check(Actual==Requested,"retainreadableoriginalsize");
        }
    for(float Bad:std::array<float,4>{0,-1,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
    {
        Check(ImperialHUDReadability::LogicalTextSize(12,Bad)==12,"invalidscalefallback");
        Check(ImperialHUDReadability::LogicalTextSize(Bad,.5f)==20,"invalidsizefallback");
    }
    const std::u16string Name=u"Roll \U0001F681 \uC870\uC885\uAE30";
    std::size_t Length=Name.size();
    int Removed=0;
    while(Length)
    {
        const std::size_t Previous=ImperialHUDReadability::PreviousCodePointLength(Name.c_str(),Length);
        Check(Previous<Length,"truncationmustprogress");
        Check(Previous==0||Name[Previous-1]<0xd800||Name[Previous-1]>0xdbff,"nohighsurrogatetail");
        Length=Previous;++Removed;
    }
    Check(Removed==10,"unicodecodepointcount");
    Check(ImperialHUDReadability::PreviousCodePointLength(u"",0)==0,"empty");
    Check(ImperialHUDReadability::PreviousCodePointLength(u"\U0001F681",2)==0,"wholeemoji");
    Check(ImperialHUDReadability::PreviousCodePointLength(u"A\U0001F681",3)==1,"emojiwithprefix");
    Check(ImperialHUDReadability::PreviousCodePointLength(u"\U0001F681A",3)==2,"latinwithprefix");
    std::cout<<"hud_readability_checks="<<Checks<<" failures="<<Failures<<"\n";
    return Failures?1:0;
}
