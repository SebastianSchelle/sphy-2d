#ifndef SHELF_ALLOCATOR_HPP
#define SHELF_ALLOCATOR_HPP

#include <cassert>
#include <functional>
#include <set>
#include <vector>

namespace con::alloc
{

struct Rect
{
    int x;
    int y;
    int width;
    int height;
};

struct StoragePtr
{
    int shelfId;
    int bucketId;
    int slotId;
    Rect rect;
};

class Bucket
{
  public:
    Bucket(int width, int xOffs)
    {
        this->xOffs = xOffs;
        bucketWidth = width;
        remainingWidth = width;
        slotsUsed = 0;
        slotsRemoved = 0;
    }
    ~Bucket() {}

    bool insertRect(StoragePtr& storagePtr)
    {
        if (remainingWidth >= storagePtr.rect.width)
        {
            storagePtr.rect.x = xOffs + bucketWidth - remainingWidth;
            storagePtr.slotId = slotsUsed;
            remainingWidth -= storagePtr.rect.width;
            slotsUsed++;
            return true;
        }
        return false;
    }

    bool removeRect(const StoragePtr& storagePtr)
    {
        int slotId = storagePtr.slotId;
        if (slotId >= 0 && slotId < slotsUsed)
        {
            slotsRemoved++;
            if (slotsRemoved == slotsUsed)
            {
                remainingWidth = bucketWidth;
                slotsUsed = 0;
                slotsRemoved = 0;
                return true;
            }
        }
        return false;
    }

    int getSlotsUsed() const
    {
        return slotsUsed;
    }

    int storedRects() const
    {
        return slotsUsed - slotsRemoved;
    }

  private:
    int xOffs;
    int bucketWidth;
    int remainingWidth;
    int slotsUsed;
    int slotsRemoved;
};

class Shelf
{
  public:
    Shelf(int width, int height, int bucketSize, int yOffs)
    {
        this->yOffs = yOffs;
        this->shelfWidth = width;
        this->shelfHeight = height;

        assert(width % bucketSize == 0
               && "Width must be divisible by bucket size");

        int numBuckets = width / bucketSize;
        for (int i = 0; i < numBuckets; i++)
        {
            buckets.push_back(Bucket(bucketSize, i * bucketSize));
        }
    }
    ~Shelf() {}

    bool insertRect(StoragePtr& storagePtr)
    {
        if (shelfHeight >= storagePtr.rect.height)
        {
            for (int i = 0; i < buckets.size(); i++)
            {
                if (buckets[i].insertRect(storagePtr))
                {
                    storagePtr.bucketId = i;
                    storagePtr.rect.y = yOffs;
                    return true;
                }
            }
        }
        return false;
    }

    bool removeRect(const StoragePtr& storagePtr)
    {
        int bucketId = storagePtr.bucketId;
        if (bucketId >= 0 && bucketId < buckets.size())
        {
            if (buckets[bucketId].removeRect(storagePtr))
            {
                return isEmpty();
            }
        }
        return false;
    }

    bool isEmpty() const
    {
        for (int i = 0; i < buckets.size(); i++)
        {
            if (buckets[i].getSlotsUsed() > 0)
            {
                return false;
            }
        }
        return true;
    }

    int getWidth() const
    {
        return shelfWidth;
    }
    int getHeight() const
    {
        return shelfHeight;
    }
    int getYOffs() const
    {
        return yOffs;
    }
    int storedRects() const
    {
        int count = 0;
        for (int i = 0; i < buckets.size(); i++)
        {
            count += buckets[i].storedRects();
        }
        return count;
    }
  private:
    std::vector<Bucket> buckets;
    int shelfWidth;
    int shelfHeight;
    int bucketSize;
    int yOffs;
};

class ShelfAllocator
{
  public:
    ShelfAllocator(int width,
                   int height,
                   int bucketSize,
                   float excessHeightThreshold = 0.7f)
        : sortedShelves(
              [this](int a, int b)
              {
                // Safety check: if indices are out of bounds, use index comparison
                // This can happen if sortedShelves contains stale indices after removals
                if (a < 0 || b < 0 || a >= (int)shelves.size() || b >= (int)shelves.size())
                {
                    // Return consistent ordering for invalid indices
                    return a < b;
                }
                if (shelves[a].getHeight() != shelves[b].getHeight())
                    return shelves[a].getHeight() < shelves[b].getHeight();
                else
                    return a < b; /* tie breaker */
              })
    {
        shelfWidth = width;
        shelfHeight = height;

        this->bucketSize = bucketSize;
        this->excessHeightThreshold = excessHeightThreshold;
        headRoom = height;
    }
    ~ShelfAllocator() {}

    bool insertRect(StoragePtr& storagePtr)
    {
        // Remove any stale indices from sortedShelves before iterating
        auto it = sortedShelves.begin();
        while (it != sortedShelves.end())
        {
            if (*it < 0 || *it >= (int)shelves.size())
            {
                it = sortedShelves.erase(it);
            }
            else
            {
                ++it;
            }
        }
        
        for (auto& shelf : sortedShelves)
        {
            int shelfHeight = shelves[shelf].getHeight();
            // std::cout << "Shelf id: " << shelf << " height: " << shelfHeight
            //           << " - rect height: " << storagePtr.rect.height
            //           << std::endl;
            if (shelfHeight
                >= storagePtr.rect
                       .height)  // Found a shelf that can fit the rect
            {
                // if shelf has excessive height, try make another
                if (shelfHeight * excessHeightThreshold
                    > (float)storagePtr.rect.height)
                {
                    if (putIntoNewShelf(storagePtr))
                    {
                        return true;
                    }
                }
                if (shelves[shelf].insertRect(storagePtr))
                {
                    storagePtr.shelfId = shelf;
                    return true;
                }
            }
        }
        // No valid candidate found, create a new shelf
        return putIntoNewShelf(storagePtr);
    }

    void remove(const StoragePtr& storagePtr)
    {
        int shelfId = storagePtr.shelfId;
        if (shelfId >= 0 && shelfId < shelves.size())
        {
            if (shelves[shelfId].removeRect(storagePtr))
            {
                // shelf has been reset, check if this is the top most shelf
                if (shelfId == shelves.size() - 1)
                {
                    while (!shelves.empty() && shelves.back().isEmpty())
                    {
                        int backIndex = shelves.size() - 1;
                        headRoom += shelves.back().getHeight();
                        sortedShelves.erase(backIndex);
                        shelves.pop_back();
                    }
                }
            }
        }
    }

    int storedRects() const
    {
        int count = 0;
        for (int i = 0; i < shelves.size(); i++)
        {
            count += shelves[i].storedRects();
        }
        return count;
    }

  private:
    bool putIntoNewShelf(StoragePtr& storagePtr)
    {
        Shelf* newShelf = createShelf(storagePtr.rect.height);
        if (newShelf)
        {
            // Put into new shelf, fit guaranteed
            if (newShelf->insertRect(storagePtr))
            {
                storagePtr.shelfId = shelves.size() - 1;
                return true;
            }
        }
        return false;
    }

    Shelf* createShelf(int height)
    {
        if (headRoom >= height)
        {
            int lastYoffs = shelves.size() > 0 ? shelves.back().getYOffs() : 0;
            int lastHeight =
                shelves.size() > 0 ? shelves.back().getHeight() : 0;
            shelves.push_back(
                Shelf(shelfWidth, height, bucketSize, lastYoffs + lastHeight));
            Shelf* newShelf = &shelves.back();
            headRoom -= height;
            sortedShelves.insert(shelves.size() - 1);
            return newShelf;
        }
        return nullptr;
    }

    int shelfWidth;
    int shelfHeight;
    int bucketSize;
    std::vector<Shelf> shelves;
    std::set<int, std::function<bool(int, int)>> sortedShelves;
    int headRoom;
    float excessHeightThreshold = 0.8f;
};

}  // namespace con::alloc

#endif
