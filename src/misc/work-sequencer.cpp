#include "work-sequencer.hpp"

namespace work
{

WorkSequencer::WorkSequencer(uint stepsPerCall)
    : stepsPerCall(stepsPerCall)
{
}

WorkSequencer::~WorkSequencer()
{
}

void WorkSequencer::addWorkFunction(std::function<void()> workFunction)
{
    workFunctions.push_back(workFunction);
}

void WorkSequencer::execute()
{
    int steps = 0;
    while(!workFunctions.empty())
    {
        workFunctions.back()();
        workFunctions.pop_back();
        steps++;
        if(steps >= stepsPerCall)
        {
            return;
        }
    }
}

}  // namespace work