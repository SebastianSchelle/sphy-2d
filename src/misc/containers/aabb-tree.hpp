#ifndef AABB_TREE_HPP
#define AABB_TREE_HPP

#include <cassert>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <stack>
#include <std-inc.hpp>
#include <vector>

namespace con
{

// =====================
// Basic math
// =====================

inline glm::vec2 minVec(const glm::vec2& a, const glm::vec2& b)
{
    return glm::vec2(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y);
}
inline glm::vec2 maxVec(const glm::vec2& a, const glm::vec2& b)
{
    return glm::vec2(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y);
}

// =====================
// AABB
// =====================
struct AABB
{
    glm::vec2 lower;
    glm::vec2 upper;

    float perimeter() const
    {
        float wx = upper.x - lower.x;
        float wy = upper.y - lower.y;
        return 2.0f * (wx + wy);
    }

    bool overlaps(const AABB& other) const
    {
        if (upper.x < other.lower.x || lower.x > other.upper.x)
            return false;
        if (upper.y < other.lower.y || lower.y > other.upper.y)
            return false;
        return true;
    }

    static AABB combine(const AABB& a, const AABB& b)
    {
        return {minVec(a.lower, b.lower), maxVec(a.upper, b.upper)};
    }

    void fatten(float margin)
    {
        lower.x -= margin;
        lower.y -= margin;
        upper.x += margin;
        upper.y += margin;
    }

    bool contains(const AABB& other) const
    {
        if (other.lower.x < lower.x || other.upper.x > upper.x)
            return false;
        if (other.lower.y < lower.y || other.upper.y > upper.y)
            return false;
        return true;
    }
};


// =====================
// Dynamic Tree
// =====================
template <typename T> class DynamicAABBTree
{
    struct Node
    {
        AABB box;
        int parent = -1;
        int left = -1;
        int right = -1;
        int height = 0;
        T id;  // user data

        bool isLeaf() const
        {
            return left == -1;
        }
    };

  public:
    DynamicAABBTree()
    {
        nodes.reserve(1024);
        allocateNode();  // root placeholder
    }

    int createProxy(AABB& box, const T& id)
    {
        box.fatten(fatMargin);
        int node = allocateNode();
        nodes[node].box = box;
        nodes[node].id = id;
        nodes[node].height = 0;

        insertLeaf(node);
        return node;
    }

    void destroyProxy(int node)
    {
        removeLeaf(node);
        freeNode(node);
    }

    void moveProxy(int node, AABB newBox)
    {
        if (nodes[node].box.contains(newBox))
        {
            return;
        }
        newBox.fatten(fatMargin);
        removeLeaf(node);
        nodes[node].box = newBox;
        insertLeaf(node);
    }

    template <typename Callback> void query(const AABB& box, Callback cb) const
    {
        if (root == -1)
            return;

        std::stack<int> stack;
        stack.push(root);

        while (!stack.empty())
        {
            int node = stack.top();
            stack.pop();

            if (!nodes[node].box.overlaps(box))
                continue;

            if (nodes[node].isLeaf())
            {
                cb(nodes[node].id);
            }
            else
            {
                stack.push(nodes[node].left);
                stack.push(nodes[node].right);
            }
        }
    }

    void getAllAABBs(std::vector<AABB>& aabbs) const
    {
        aabbs.clear();
        if (root == -1)
            return;

        std::stack<int> stack;
        stack.push(root);

        while (!stack.empty())
        {
            int node = stack.top();
            stack.pop();

            aabbs.push_back(nodes[node].box);
            if (!nodes[node].isLeaf())
            {
                stack.push(nodes[node].left);
                stack.push(nodes[node].right);
            }
        }
    }

  private:
    std::vector<Node> nodes;
    int root = -1;
    int freeList = -1;
    const float fatMargin = 10.0f;

    int allocateNode()
    {
        if (freeList != -1)
        {
            int n = freeList;
            freeList = nodes[n].parent;
            nodes[n] = Node{};
            return n;
        }
        nodes.push_back(Node{});
        return (int)nodes.size() - 1;
    }

    void freeNode(int n)
    {
        nodes[n].parent = freeList;
        freeList = n;
    }

    void insertLeaf(int leaf)
    {
        if (root == -1)
        {
            root = leaf;
            nodes[root].parent = -1;
            return;
        }

        int index = root;
        AABB leafBox = nodes[leaf].box;

        while (!nodes[index].isLeaf())
        {
            int left = nodes[index].left;
            int right = nodes[index].right;

            float area = nodes[index].box.perimeter();
            AABB combined = AABB::combine(nodes[index].box, leafBox);
            float cost = 2.0f * combined.perimeter();

            float inheritanceCost = 2.0f * (combined.perimeter() - area);

            float costLeft;
            if (nodes[left].isLeaf())
            {
                AABB a = AABB::combine(leafBox, nodes[left].box);
                costLeft = a.perimeter() + inheritanceCost;
            }
            else
            {
                AABB a = AABB::combine(leafBox, nodes[left].box);
                float oldArea = nodes[left].box.perimeter();
                float newArea = a.perimeter();
                costLeft = (newArea - oldArea) + inheritanceCost;
            }

            float costRight;
            if (nodes[right].isLeaf())
            {
                AABB a = AABB::combine(leafBox, nodes[right].box);
                costRight = a.perimeter() + inheritanceCost;
            }
            else
            {
                AABB a = AABB::combine(leafBox, nodes[right].box);
                float oldArea = nodes[right].box.perimeter();
                float newArea = a.perimeter();
                costRight = (newArea - oldArea) + inheritanceCost;
            }

            if (cost < costLeft && cost < costRight)
                break;

            index = (costLeft < costRight) ? left : right;
        }

        int sibling = index;

        int oldParent = nodes[sibling].parent;
        int newParent = allocateNode();
        nodes[newParent].parent = oldParent;
        nodes[newParent].box = AABB::combine(leafBox, nodes[sibling].box);
        nodes[newParent].height = nodes[sibling].height + 1;
        nodes[newParent].left = sibling;
        nodes[newParent].right = leaf;

        nodes[sibling].parent = newParent;
        nodes[leaf].parent = newParent;

        if (oldParent == -1)
        {
            root = newParent;
        }
        else
        {
            if (nodes[oldParent].left == sibling)
                nodes[oldParent].left = newParent;
            else
                nodes[oldParent].right = newParent;
        }

        fixUpwards(newParent);
    }

    void removeLeaf(int leaf)
    {
        if (leaf == root)
        {
            root = -1;
            return;
        }

        int parent = nodes[leaf].parent;
        int grandParent = nodes[parent].parent;
        int sibling = (nodes[parent].left == leaf) ? nodes[parent].right
                                                   : nodes[parent].left;

        if (grandParent != -1)
        {
            if (nodes[grandParent].left == parent)
                nodes[grandParent].left = sibling;
            else
                nodes[grandParent].right = sibling;

            nodes[sibling].parent = grandParent;
            freeNode(parent);

            fixUpwards(grandParent);
        }
        else
        {
            root = sibling;
            nodes[sibling].parent = -1;
            freeNode(parent);
        }
    }

    void fixUpwards(int index)
    {
        while (index != -1)
        {
            index = balance(index);

            int left = nodes[index].left;
            int right = nodes[index].right;

            nodes[index].height =
                1 + std::max(nodes[left].height, nodes[right].height);
            nodes[index].box = AABB::combine(nodes[left].box, nodes[right].box);

            index = nodes[index].parent;
        }
    }

    int balance(int iA)
    {
        Node& A = nodes[iA];
        if (A.isLeaf() || A.height < 2)
            return iA;

        int iB = A.left;
        int iC = A.right;
        Node& B = nodes[iB];
        Node& C = nodes[iC];

        int balance = C.height - B.height;

        if (balance > 1)
        {
            int iF = C.left;
            int iG = C.right;
            Node& F = nodes[iF];
            Node& G = nodes[iG];

            C.left = iA;
            C.parent = A.parent;
            A.parent = iC;

            if (C.parent != -1)
            {
                if (nodes[C.parent].left == iA)
                    nodes[C.parent].left = iC;
                else
                    nodes[C.parent].right = iC;
            }
            else
                root = iC;

            if (F.height > G.height)
            {
                C.right = iF;
                A.right = iG;
                G.parent = iA;
            }
            else
            {
                C.right = iG;
                A.right = iF;
                F.parent = iA;
            }

            A.box = AABB::combine(nodes[A.left].box, nodes[A.right].box);
            C.box = AABB::combine(A.box, nodes[C.right].box);

            A.height =
                1 + std::max(nodes[A.left].height, nodes[A.right].height);
            C.height = 1 + std::max(A.height, nodes[C.right].height);

            return iC;
        }

        if (balance < -1)
        {
            int iD = B.left;
            int iE = B.right;
            Node& D = nodes[iD];
            Node& E = nodes[iE];

            B.left = iA;
            B.parent = A.parent;
            A.parent = iB;

            if (B.parent != -1)
            {
                if (nodes[B.parent].left == iA)
                    nodes[B.parent].left = iB;
                else
                    nodes[B.parent].right = iB;
            }
            else
                root = iB;

            if (D.height > E.height)
            {
                B.right = iD;
                A.left = iE;
                E.parent = iA;
            }
            else
            {
                B.right = iE;
                A.left = iD;
                D.parent = iA;
            }

            A.box = AABB::combine(nodes[A.left].box, nodes[A.right].box);
            B.box = AABB::combine(A.box, nodes[B.right].box);

            A.height =
                1 + std::max(nodes[A.left].height, nodes[A.right].height);
            B.height = 1 + std::max(A.height, nodes[B.right].height);

            return iB;
        }

        return iA;
    }
};

#define SER_AABB                                                               \
    SOBJ(o.lower);                                                             \
    SOBJ(o.upper);
EXT_SER(AABB, SER_AABB)
EXT_DES(AABB, SER_AABB)

}  // namespace con

#endif