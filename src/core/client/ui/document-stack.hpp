#ifndef DOCUMENT_STACK_HPP
#define DOCUMENT_STACK_HPP

#include <std-inc.hpp>

namespace ui
{

typedef std::function<void(const string&)> ShowDocClb;
typedef std::function<void(const string&)> HideDocClb;
typedef std::function<bool(const string&)> IsVisibleClb;

template <class T> class DocumentStack
{
  public:
    DocumentStack(ShowDocClb showClb,
                  HideDocClb hideClb,
                  IsVisibleClb visibleClb)
        : showDoc(showClb), hideDoc(hideClb), isVisible(visibleClb)
    {
    }

    void pushDocument(string id, const T& extra = T{})
    {
        if (!stack.empty())
        {
            hideDoc(stack.back().first);
        }
        stack.push_back({id, extra});
        showDoc(stack.back().first);
    }

    std::pair<string, T> popDocument(bool allowClose = true)
    {
        if (stack.size() > 1 || allowClose)
        {
            hideDoc(stack.back().first);
            stack.pop_back();
        }
        if (!stack.empty())
        {
            showDoc(stack.back().first);
            return stack.back();
        }
        return {"", T{}};
    }

    std::pair<string, T> top()
    {
        return stack.back().first;
    }

    size_t size() const
    {
        return stack.size();
    }

    bool isOpen() const
    {
        if (stack.empty())
        {
            return false;
        }
        return isVisible(stack.back().first);
    }

    void show()
    {
        showDoc(stack.back().first);
    }

    void hide()
    {
        hideDoc(stack.back().first);
    }

    void clear()
    {
        if (stack.size())
        {
            hideDoc(stack.back().first);
        }
        stack.clear();
    }

    T extra()
    {
        return stack.back().second;
    }

  private:
    vector<std::pair<string, T>> stack;
    ShowDocClb showDoc;
    HideDocClb hideDoc;
    IsVisibleClb isVisible;
};

}  // namespace ui

#endif