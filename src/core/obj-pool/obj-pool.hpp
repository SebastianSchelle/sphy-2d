#ifndef OBJ_POOL_HPP
#define OBJ_POOL_HPP

#include <free-vector.hpp>
#include <functional>

namespace opool
{

template <class T> class ObjectPool
{
  public:
    ObjectPool()
    {
        pool.setDestroyFunction([this](T &item, typename con::FreeVec<T>::Handle handle){
            onDestroy(item, handle);
        });
    }
    ~ObjectPool() {}
    con::FreeVec<T>::Handle spawnObject(const T& object);
    void destroyObject(con::FreeVec<T>::Handle);
    void foreach (
        std::function<
            con::FreeVecForeachRet(T&, typename con::FreeVec<T>::Handle)> clb);
    T* getObject(con::FreeVec<T>::Handle handle);
    void setDestroyFunction(
        std::function<void(T&, typename con::FreeVec<T>::Handle)> clb)
    {
        onDestroyClb = clb;
    }
    void
    foreachNew(std::function<void(T&, typename con::FreeVec<T>::Handle)> clb);
    void foreachDestroyed(
        std::function<void(T&, typename con::FreeVec<T>::Handle)> clb);
    void clearNew()
    {
        newObjects.clear();
    }
    void clearDestroyed()
    {
        destroyedObjects.clear();
    }

  private:
    void onDestroy(T& item, typename con::FreeVec<T>::Handle handle);
    con::FreeVec<T> pool;
    vector<typename con::FreeVec<T>::Handle> newObjects;
    vector<std::pair<typename con::FreeVec<T>::Handle, T>> destroyedObjects;
    std::function<void(T&, typename con::FreeVec<T>::Handle)> onDestroyClb;
};

template <class T>
void ObjectPool<T>::onDestroy(T& item, typename con::FreeVec<T>::Handle handle)
{
    if (onDestroyClb)
    {
        onDestroyClb(item, handle);
    }
    destroyedObjects.push_back({handle, item});
}

template <class T>
void ObjectPool<T>::foreach (
    std::function<con::FreeVecForeachRet(T&, typename con::FreeVec<T>::Handle)>
        clb)
{
    pool.foreach (clb);
}

template <class T>
void ObjectPool<T>::foreachNew(
    std::function<void(T&, typename con::FreeVec<T>::Handle)> clb)
{
    for (auto handle : newObjects)
    {
        auto* obj = pool.getItem(handle);
        if (obj)
        {
            clb(*obj, handle);
        }
    }
}

template <class T>
void ObjectPool<T>::foreachDestroyed(
    std::function<void(T&, typename con::FreeVec<T>::Handle)> clb)
{
    for (auto obj : destroyedObjects)
    {
        clb(obj.second, obj.first);
    }
}

template <class T>
void ObjectPool<T>::destroyObject(con::FreeVec<T>::Handle handle)
{
    pool.removeItem(handle);
}

template <class T>
con::FreeVec<T>::Handle ObjectPool<T>::spawnObject(const T& object)
{
    auto handle = pool.addItem(object);
    newObjects.push_back(handle);
    return handle;
}

template <class T> T* ObjectPool<T>::getObject(con::FreeVec<T>::Handle handle)
{
    return pool.getItem(handle);
}

};  // namespace opool

#endif