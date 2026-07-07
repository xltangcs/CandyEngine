#pragma once

#include "Candy/Core/Timestep.h"

namespace Candy {

class Entity;

class ScriptObject {
public:
    virtual ~ScriptObject() = default;

    virtual void OnConstruct() {}
    virtual void OnStart() {}
    virtual void OnTick(Timestep ts) {}
    virtual void OnDestroy() {}

    Entity* GetEntity() const { return m_Entity; }

private:
    Entity* m_Entity = nullptr;
    friend class ScriptSystem;
};

}
