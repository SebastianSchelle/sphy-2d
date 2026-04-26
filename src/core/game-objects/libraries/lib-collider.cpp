#include <lib-collider.hpp>

namespace gobj
{

Collider Collider::fromYaml(const YAML::Node& node,
                            mod::ResourceMap& resourceMap)
{
    Collider collider;
    TRY_YAML_DICT(collider.vertices, node["vertices"], std::vector<vec2>());
    TRY_YAML_DICT(collider.restitution, node["restitution"], 0.5f);
    return collider;
}

}  // namespace gobj