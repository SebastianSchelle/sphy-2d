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

struct IdxUuid
{
    int idx;
    std::string uuid;
};

template <class T> class ItemLib
{
  public:
    ItemLib() {}
    virtual ~ItemLib() {}

    int addItem(const std::string& name, const T& item);
    IdxUuid addWithRandomKey(const T& item);
    void removeItem(int idx);

    int getIndex(const std::string& name) const;
    T* getItem(int idx);
    ItemWrapper<T>* getWrappedItem(int idx);
    uint16_t getGeneration(int idx) const;

    int size() const
    {
        return items.size();
    }
    int getFreeSlotCount() const
    {
        return freeSlots.size();
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
int ItemLib<T>::addItem(const std::string& name, const T& item)
{
    auto it = idMap.find(name);
    if (it != idMap.end())
    {
        // Replace old item
        int idx = it->second;
        items[idx].item = item;
        items[idx].alive = true;
        items[idx].generation++;
        return idx;
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
        return idx;
    }
}

template <class T> IdxUuid ItemLib<T>::addWithRandomKey(const T& item)
{
    std::string uuid = sec::uuid();
    int idx = addItem(uuid, item);
    return IdxUuid{idx, uuid};
}

template <class T> int ItemLib<T>::getIndex(const std::string& name) const
{
    auto it = idMap.find(name);
    if (it != idMap.end())
    {
        return it->second;
    }
    else
    {
        return -1;
    }
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

template <class T> T* ItemLib<T>::getItem(int idx)
{
    ItemWrapper<T>* wrapper = getWrappedItem(idx);
    if (wrapper)
    {
        return &wrapper->item;
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

}  // namespace con

#endif
