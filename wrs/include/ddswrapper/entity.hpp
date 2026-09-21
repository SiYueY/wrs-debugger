#ifndef DDSWRAPPER_ENTITY_HPP_
#define DDSWRAPPER_ENTITY_HPP_

namespace ddswrapper {

class Entity {
public:
    virtual ~Entity() = default;

private:
    virtual void shutdown() noexcept = 0;

    friend class Node;
    friend class Context;
};

}  // namespace ddswrapper

#endif  // DDSWRAPPER_ENTITY_HPP_
