#include "work-sequencer.hpp"

namespace work
{

WorkSequencer::WorkSequencer(uint stepsPerCall)
    : stepsPerCall(stepsPerCall), needsAck(false)
{
}

WorkSequencer::~WorkSequencer() {}

void WorkSequencer::addWorkFunction(WorkFunction workFunction, bool last)
{
    if (last)
    {
        workFunctions.insert(workFunctions.begin(), workFunction);
    }
    else
    {
        workFunctions.push_back(workFunction);
    }
}

void WorkSequencer::execute()
{
    int steps = 0;
    if (needsAck)
    {
        return;
    }
    while (!workFunctions.empty())
    {
        needsAck = workFunctions.back()();
        workFunctions.pop_back();
        steps++;
        if (needsAck)
        {
            return;
        }
        else if (steps >= stepsPerCall)
        {
            return;
        }
    }
}

void WorkSequencer::ack()
{
    needsAck = false;
}

void WorkSequencer::clear()
{
    workFunctions.clear();
    needsAck = false;
}

}  // namespace work