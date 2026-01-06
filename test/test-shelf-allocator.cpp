#include "shelf-allocator.hpp"
#include "simple_svg_1.0.0.hpp"
#include "std-inc.hpp"
#include <gtest/gtest.h>
#include <random>

using namespace svg;


void drawShelf(int width,
               int height,
               std::vector<con::alloc::StoragePtr>& rects)
{
    Dimensions dimensions(width + 10, height + 10);
    Document doc("shelf-alloc-test.svg", Layout(dimensions, Layout::TopLeft));

    // Red image border.
    Polygon border(Stroke(1, Color::Red));
    border << Point(0, 0) << Point(dimensions.width, 0)
           << Point(dimensions.width, dimensions.height)
           << Point(0, dimensions.height);
    doc << border;

    for (auto& rect : rects)
    {
        doc << Rectangle(Point(rect.rect.x + 6, rect.rect.y + 6),
                         rect.rect.width-2,
                         rect.rect.height-2,
                         Color::Blue);
        if (rect.rect.x == 0)
        {
            doc << Text(Point(rect.rect.x + 5, rect.rect.y + 5 + rect.rect.height),
                        std::to_string(rect.rect.height),
                        Color::Silver,
                        Font(4, "Verdana"));
        }
    }
    doc.save();
}

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions)
{
    int shelfWidth = 2000;
    int shelfHeight = 2000;
    int bucketSize = 500;
    float excessHeightThreshold = 0.9f;
    int minRectSize = 20;
    int maxRectSize = 88;
    int numRects = 500;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution dist(minRectSize, maxRectSize);

    con::alloc::ShelfAllocator allocator(
        shelfWidth, shelfHeight, bucketSize, excessHeightThreshold);

    std::vector<con::alloc::StoragePtr> rects;
    for (int i = 0; i < numRects; i++)
    {
        con::alloc::StoragePtr rect{
            0, 0, 0, con::alloc::Rect{0, 0, dist(gen), dist(gen)}};
        if (!allocator.insertRect(rect))
        {
            drawShelf(shelfWidth, shelfHeight, rects);
            FAIL() << "Failed to insert rect" << rect.rect.width << "x"
                  << rect.rect.height;
            break;
        }
        rects.push_back(rect);
    }
    // for(int i = 0; i < rects.size(); i++)
    // {
    //     std::cout << "Removing rect: " << i << std::endl;
    //     allocator.remove(rects[i]);
    // }
    drawShelf(shelfWidth, shelfHeight, rects);
}
