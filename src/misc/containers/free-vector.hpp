#ifndef FREE_VECTOR_HPP
#define FREE_VECTOR_HPP

#include <std-inc.hpp>

namespace con
{

template <class T> struct FreeVecItemWrapper
{
    T item;
    bool alive;
    uint16_t generation;
};

template <class T> class FreeVec
{
  public:
    class Handle
    {
      public:
        static Handle Invalid()
        {
            return Handle(0, 0);
        }
        Handle(GenericHandle genericHandle)
            : idx(genericHandle.idx), generation(genericHandle.gen)
        {
        }
        GenericHandle toGenericHandle() const
        {
            return {idx, generation};
        }
        const std::string toString() const
        {
            return fmt::format("({}, {})", idx, generation);
        }
        Handle(uint16_t idx, uint16_t generation)
            : idx(idx), generation(generation)
        {
        }
        Handle(uint32_t value) : idx(value & 0xffff), generation(value >> 16) {}
        uint32_t value() const
        {
            return ((uint32_t)generation << 16) | (uint16_t)idx;
        }
        bool isValid() const
        {
            return generation != 0;
        }
        uint16_t getIdx() const
        {
            return idx;
        }
        uint16_t getGeneration() const
        {
            return generation;
        }

      private:
        uint16_t idx;
        uint16_t generation;
    };
    struct HandleUuid
    {
        Handle handle;
        std::string uuid;
    };


  public:
    FreeVec() {}
    virtual ~FreeVec() {}

    Handle addItem(const T& item);
    void removeItem(int idx);
    void removeItem(Handle handle);

    T* getItem(int idx, bool getCorpse = false);
    T* getItem(Handle handle, bool getCorpse = false);
    FreeVecItemWrapper<T>* getWrappedItem(int idx);
    uint16_t getGeneration(int idx) const;
    Handle firstAliveHandle() const;

    int size() const
    {
        return items.size();
    }
    bool isEmpty() const
    {
        return items.empty();
    }
    int getFreeSlotCount() const
    {
        return freeSlots.size();
    }

    Handle getHandle(int idx) const
    {
        if (idx < items.size() && idx >= 0 && items[idx].alive)
        {
            return Handle(idx, items[idx].generation);
        }
        else
        {
            return Handle::Invalid();
        }
    }

    // const std::unordered_map<std::string, int> &getIdMap() const { return
    // idMap; };

  protected:
  private:
    std::vector<FreeVecItemWrapper<T>> items;
    std::vector<int> freeSlots;
};

template <class T> FreeVec<T>::Handle FreeVec<T>::addItem(const T& item)
{
    int idx;
    if (freeSlots.empty())
    {
        FreeVecItemWrapper<T> wrapper = {item, true, 1};
        items.push_back(wrapper);
        idx = items.size() - 1;
    }
    else
    {
        idx = freeSlots.back();
        freeSlots.pop_back();
        items[idx].item = item;
        items[idx].alive = true;
        items[idx].generation++;
    }

    return Handle(idx, items[idx].generation);
}

template <class T>
FreeVec<T>::Handle FreeVec<T>::firstAliveHandle() const
{
    for (int i = 0; i < items.size(); i++)
    {
        if (items[i].alive)
        {
            return Handle(i, items[i].generation);
        }
    }
    return Handle::Invalid();
}

template <class T> FreeVecItemWrapper<T>* FreeVec<T>::getWrappedItem(int idx)
{
    if (idx < items.size() && idx >= 0 && items[idx].alive)
    {
        return &items[idx];
    }
    else
    {
        return nullptr;
    }
}

template <class T> T* FreeVec<T>::getItem(int idx, bool getCorpse)
{
    FreeVecItemWrapper<T>* wrapper = getWrappedItem(idx);
    if (wrapper && (wrapper->alive || getCorpse))
    {
        return &wrapper->item;
    }
    else
    {
        return nullptr;
    }
}

template <class T> T* FreeVec<T>::getItem(Handle handle, bool getCorpse)
{
    if (handle.isValid())
    {
        return getItem(handle.getIdx(), getCorpse);
    }
    else
    {
        return nullptr;
    }
}

template <class T> uint16_t FreeVec<T>::getGeneration(int idx) const
{
    if (idx < items.size() && idx >= 0 && items[idx].alive)
    {
        return items[idx].generation;
    }
    else
    {
        return 0;
    }
}

template <class T> void FreeVec<T>::removeItem(int idx)
{
    if (idx < items.size() && idx >= 0 && items[idx].alive)
    {
        items[idx].alive = false;
        freeSlots.push_back(idx);
    }
}

template <class T> void FreeVec<T>::removeItem(Handle handle)
{
    if (handle.isValid())
    {
        removeItem(handle.getIdx());
    }
}

}  // namespace con

#endif