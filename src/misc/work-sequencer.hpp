#ifndef WORK_SEQUENCER_HPP
#define WORK_SEQUENCER_HPP

#include <std-inc.hpp>

namespace work
{

class WorkSequencer
{
  public:
    WorkSequencer(uint stepsPerCall);
    ~WorkSequencer();
    void addWorkFunction(std::function<void()> workFunction);
    void execute();

  private:
    uint stepsPerCall;
    std::vector<std::function<void()>> workFunctions;
};

}  // namespace work

#endif
