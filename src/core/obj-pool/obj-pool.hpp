#ifndef OBJ_POOL_HPP
#define OBJ_POOL_HPP

#include <free-vector.hpp>

namespace opool
{

template <class T> class ObjectPool
{
  public:
    ObjectPool() {}
    ~ObjectPool() {}
    con::FreeVec<T>::Handle
    spawnObject(const T& object);
    void destroyProjectile(con::FreeVec<T>::Handle);
    void foreach (
        std::function<
            con::FreeVecForeachRet(T&, typename con::FreeVec<T>::Handle)> clb);

  private:
    con::FreeVec<T> pool;
};

template <class T>
void ObjectPool<T>::foreach (
    std::function<con::FreeVecForeachRet(T&, typename con::FreeVec<T>::Handle)>
        clb)
{
    pool.foreach(clb);
}

template <class T>
void ObjectPool<T>::destroyProjectile(con::FreeVec<T>::Handle handle)
{
    pool.removeItem(handle);
}

template <class T>
con::FreeVec<T>::Handle ObjectPool<T>::spawnObject(const T& object)
{
    auto handle = pool.addItem(object);
    return handle;
}

};  // namespace opool

#endif