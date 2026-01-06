#ifndef GAME_LIB_HPP
#define GAME_LIB_HPP

#include <std-inc.hpp>

namespace con
{

template <class T> struct ItemWrapper
{
    T item;
    bool alive;
    uint16_t generation;
};

template <class T> class ItemLib
{
  public:
    class Handle
    {
      public:
        static Handle Invalid()
        {
            return Handle(0, 0);
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
    ItemLib() {}
    virtual ~ItemLib() {}

    Handle addItem(const std::string& name, const T& item);
    HandleUuid addWithRandomKey(const T& item);
    void removeItem(int idx);
    void removeItem(Handle handle);

    Handle getHandle(const std::string& name) const;
    T* getItem(int idx, bool getCorpse = false);
    T* getItem(Handle handle, bool getCorpse = false);
    ItemWrapper<T>* getWrappedItem(int idx);
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
    std::unordered_map<std::string, int> idMap;
    std::vector<ItemWrapper<T>> items;
    std::vector<int> freeSlots;
};

template <class T>
ItemLib<T>::Handle ItemLib<T>::addItem(const std::string& name, const T& item)
{
    auto it = idMap.find(name);
    if (it != idMap.end())
    {
        // Replace old item
        int idx = it->second;
        items[idx].item = item;
        items[idx].alive = true;
        items[idx].generation++;
        return Handle(idx, items[idx].generation);
    }
    else
    {
        int idx;
        if (freeSlots.empty())
        {
            ItemWrapper<T> wrapper = {item, true, 1};
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

        idMap[name] = idx;
        return Handle(idx, items[idx].generation);
    }
}

template <class T> ItemLib<T>::HandleUuid ItemLib<T>::addWithRandomKey(const T& item)
{
    std::string uuid = sec::uuid();
    return {addItem(uuid, item), uuid};
}

template <class T>
ItemLib<T>::Handle ItemLib<T>::getHandle(const std::string& name) const
{
    auto it = idMap.find(name);
    if (it != idMap.end())
    {
        return Handle(it->second, items[it->second].generation);
    }
    else
    {
        return Handle::Invalid();
    }
}

template <class T> ItemLib<T>::Handle ItemLib<T>::firstAliveHandle() const
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

template <class T> ItemWrapper<T>* ItemLib<T>::getWrappedItem(int idx)
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

template <class T> T* ItemLib<T>::getItem(int idx, bool getCorpse)
{
    ItemWrapper<T>* wrapper = getWrappedItem(idx);
    if (wrapper && (wrapper->alive || getCorpse))
    {
        return &wrapper->item;
    }
    else
    {
        return nullptr;
    }
}

template <class T> T* ItemLib<T>::getItem(Handle handle, bool getCorpse)
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

template <class T> uint16_t ItemLib<T>::getGeneration(int idx) const
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

template <class T> void ItemLib<T>::removeItem(int idx)
{
    if (idx < items.size() && idx >= 0 && items[idx].alive)
    {
        items[idx].alive = false;
        freeSlots.push_back(idx);
        for (const auto& [key, value] : idMap)
        {
            if (value == idx)
            {
                idMap.erase(key);
                break;
            }
        }
    }
}

template <class T> void ItemLib<T>::removeItem(Handle handle)
{
    if (handle.isValid())
    {
        removeItem(handle.getIdx());
    }
}

}  // namespace con

#endif
