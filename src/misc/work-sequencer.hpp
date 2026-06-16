#ifndef WORK_SEQUENCER_HPP
#define WORK_SEQUENCER_HPP

#include <std-inc.hpp>

namespace work
{

typedef std::function<bool()> WorkFunction;

class WorkSequencer
{
  public:
    WorkSequencer(uint stepsPerCall);
    ~WorkSequencer();
    void addWorkFunction(WorkFunction workFunction, bool last = false);
    void execute();
    void clear();
    void ack();

  private:
    uint stepsPerCall;
    bool needsAck;
    std::vector<WorkFunction> workFunctions;
};

}  // namespace work

#endif
