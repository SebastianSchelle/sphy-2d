#ifndef DOCUMENT_STACK_HPP
#define DOCUMENT_STACK_HPP

#include <std-inc.hpp>

namespace ui
{

class DocumentStack
{
  public:
    bool pushDocument(string id);
    string popDocument();
    string top();
    size_t size();
    bool isOpen();

  private:
    vector<string> stack;
    bool open = false;
};

}  // namespace ui

#endif