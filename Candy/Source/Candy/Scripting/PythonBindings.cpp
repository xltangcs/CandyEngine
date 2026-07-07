#include <pybind11/pybind11.h>
#include <pybind11/embed.h>

#include "Candy/Scripting/ScriptObject.h"

namespace py = pybind11;

class PyScriptObject : public Candy::ScriptObject {
public:
    using Candy::ScriptObject::ScriptObject;

    void OnConstruct() override
    {
        PYBIND11_OVERRIDE(void, Candy::ScriptObject, OnConstruct);
    }

    void OnStart() override
    {
        PYBIND11_OVERRIDE(void, Candy::ScriptObject, OnStart);
    }

    void OnTick(Candy::Timestep ts) override
    {
        PYBIND11_OVERRIDE(void, Candy::ScriptObject, OnTick, (float)ts);
    }

    void OnDestroy() override
    {
        PYBIND11_OVERRIDE(void, Candy::ScriptObject, OnDestroy);
    }
};

PYBIND11_EMBEDDED_MODULE(candy, m)
{
    py::class_<Candy::ScriptObject, PyScriptObject>(m, "ScriptObject")
        .def(py::init<>())
        .def("on_construct", &Candy::ScriptObject::OnConstruct)
        .def("on_start", &Candy::ScriptObject::OnStart)
        .def("on_destroy", &Candy::ScriptObject::OnDestroy);
}
