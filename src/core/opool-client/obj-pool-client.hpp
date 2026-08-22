#ifndef OBJ_POOL_CLIENT_HPP
#define OBJ_POOL_CLIENT_HPP

#include <std-inc.hpp>

namespace opool
{

template <class T> class OpoolClient
{
  public:
    struct OpoolWrapper
    {
        uint16_t generation;
        bool active;
        T item;
    };
    OpoolClient() {}
    ~OpoolClient() {}
    void markInactive();
    void deleteInactive();
    void updateObject(const GenericHandle32& handle, const T::Params& p);
    void foreach (std::function<void(T& proj)> clb);

  private:
    unordered_map<uint32_t, OpoolWrapper> objects;
};

template <class T>
void OpoolClient<T>::updateObject(const GenericHandle32& handle,
                                      const T::Params& p)
{
    auto it = objects.find(handle.idx);
    if (it != objects.end())
    {
        OpoolWrapper& item = it->second;
        item.active = true;
        if (item.generation == handle.gen)
        {
            // update object
            item.item.update(p);
        }
        else
        {
            // new projectile on occupied space
            item.generation = handle.gen;
            item.item = T(p);
        }
    }
    else
    {
        // New object in new slot
        objects[handle.idx] = {
            .generation = handle.gen, .active = true, .item = T(p)};
    }
}

template <class T>
void OpoolClient<T>::foreach (std::function<void(T& proj)> clb)
{
    for (auto& item : objects)
    {
        if (item.second.active)
        {
            clb(item.second.item);
        }
    }
}

template <class T>
void OpoolClient<T>::markInactive()
{
    for (auto& it : objects)
    {
        auto& item = it.second;
        item.active = false;
    }
}

template <class T>
void OpoolClient<T>::deleteInactive()
{
    for (auto it = objects.begin(); it != objects.end();)
    {
        if (!it->second.active)
        {
            it = objects.erase(it);
        }
        else
        {
            it++;
        }
    }
}

}  // namespace opool

#endif