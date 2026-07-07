#pragma once

#include <Varjo.h>
#include <Varjo_types.h>

namespace VarjoXR {

class VarjoSession {
public:
    VarjoSession();
    ~VarjoSession();

    VarjoSession(const VarjoSession&) = delete;
    VarjoSession& operator=(const VarjoSession&) = delete;

    varjo_Session* get() const noexcept { return m_session; }
    operator varjo_Session*() const noexcept { return m_session; }

    void pollEvents();
    void throwIfError(const char* where) const;

private:
    varjo_Session* m_session = nullptr;
};

} // namespace VarjoXR
