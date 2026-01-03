#ifndef GAME_LIB_HPP
#define GAME_LIB_HPP

#include <std-inc.hpp>

namespace con
{

/**
 * @class ItemLib
 * @brief
 */
template <class T> class ItemLib
{
  public:
    /**
     * @brief default constructor.
     */
    ItemLib() {}

    /**
     * @brief default destructor.
     */
    virtual ~ItemLib() {}

    /**
     * @brief Add item to game lib.
     * @param name Name of the item.
     * @param item Item.
     * @return -1 if failed, index of item if successful.
     */
    int addItem(const std::string &name, const T &item);

    /**
     * @brief Get index of the library item.
     * @param name Name of the item.
     * @return -1 if not in library, index if found.
     */
    int getIndex(const std::string &name);

    /**
     * @brief Get item at index.
     * @param idx Index of item.
     * @return pointer to item. Return nullptr if index invalid.
     * @see getIndex
     */
    T *getItem(int idx);

    /**
     * @brief Get library size.
     * @return Library size.
     */
    int size() { return items.size(); }

    void syncWithServer() {
        uint16_t i = 0;
        while(syncMap.contains(i)) {
            std::string name = syncMap[i];
            if(!idMap.contains(name))
            {
                LG_E("Connect failed. Client is missing library item {}", name);
                // todo: trigger disconnect
                return;
            }
            if(idMap[name] == i) {
                LG_D("Library item {} is already in the right position", name);
            }
            else
            {
                LG_D("Swap library item {} to position {}", name, i);
            }
            ++i;
        }
    }

    const std::unordered_map<std::string, int> &getIdMap() const { return idMap; };

    std::unordered_map<int, std::string> syncMap;

  protected:
  private:
    std::unordered_map<std::string, int>
        idMap; /**< Map from item name to index. Named access should only be
                  used if needed. */
    std::vector<T> items; /**< Vector of library items. Index should be used
                             over name whenever possible. */
};

template <class T>
int ItemLib<T>::addItem(const std::string &name, const T &item)
{
    auto it = idMap.find(name);
    if (it != idMap.end()) {
        // Replace old item
        int idx = it->second;
        items[idx] = item;
        return idx;
    } else {
        // Add new item
        items.push_back(item);
        int idx = items.size() - 1;
        idMap[name] = idx;
        return idx;
    }
}

template <class T> int ItemLib<T>::getIndex(const std::string &name)
{
    auto it = idMap.find(name);
    if (it != idMap.end()) {
        return it->second;
    } else {
        return -1;
    }
}

template <class T> T *ItemLib<T>::getItem(int idx)
{
    if (idx < items.size()) {
        return &items.at(idx);
    } else {
        return nullptr;
    }
}

} // namespace ilib

#endif
