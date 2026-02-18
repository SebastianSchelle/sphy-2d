#ifndef MATRIX2D_HPP
#define MATRIX2D_HPP

#include <std-inc.hpp>
#include <world-def.hpp>

namespace con
{

template <class T> struct Matrix2D {
  public:
    Matrix2D() : width(0), height(0), size(0) {}

    void init(uint32_t width, uint32_t height)
    {
        this->width = width;
        this->height = height;
        this->size = width * height;
        data.resize(size);
    }

    T *at(uint32_t x, uint32_t y)
    {
        if (x < width && y < height) {
            uint32_t idx = coordToIdx(x, y);
            return &data[idx];
        } else {
            return nullptr;
        }
    }

    T *at(uint32_t idx)
    {
        if (idx < size) {
            return &data[idx];
        } else {
            return nullptr;
        }
    }

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getSize() const { return size; }

    uint32_t coordToIdx(int x, int y) const { return y * width + x; }

    void indexToCoord(uint32_t id, uint32_t &x, uint32_t &y) const
    {
        x = id % width;
        y = (id - x) / width;
    }

    def::Direction getDir(uint32_t idPrev, uint32_t idNext)
    {
        uint32_t coordPrevX;
        uint32_t coordPrevY;
        uint32_t coordNextX;
        uint32_t coordNextY;
        indexToCoord(idPrev, coordPrevX, coordPrevY);
        indexToCoord(idNext, coordNextX, coordNextY);
        return getDir(coordPrevX, coordPrevY, coordNextX, coordNextY);
    }

    static def::Direction getDir(uint32_t coordPrevX, uint32_t coordPrevY,
                            uint32_t coordNextX, uint32_t coordNextY)
    {
        if (coordNextX > coordPrevX) {
            if (coordNextY > coordPrevY) {
                return def::Direction::SE;
            }
            if (coordNextY < coordPrevY) {
                return def::Direction::NE;
            } else {
                return def::Direction::E;
            }
        } else if (coordNextX < coordPrevX) {
            if (coordNextY > coordPrevY) {
                return def::Direction::SW;
            }
            if (coordNextY < coordPrevY) {
                return def::Direction::NW;
            } else {
                return def::Direction::W;
            }
        } else {
            if (coordNextY > coordPrevY) {
                return def::Direction::S;
            }
            if (coordNextY < coordPrevY) {
                return def::Direction::N;
            } else {
                return def::Direction::NONE;
            }
        }
    }

  private:
    uint32_t width;
    uint32_t height;
    uint32_t size;
    std::vector<T> data;
};

} // namespace con

#endif