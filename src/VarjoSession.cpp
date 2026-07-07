#include <VarjoXR/VarjoSession.hpp>

#include <stdexcept>
#include <string>

namespace VarjoXR {

VarjoSession::VarjoSession() {
    if (!varjo_IsAvailable()) {
        throw std::runtime_error("Varjo runtime is not available.");
    }

    m_session = varjo_SessionInit();
    if (!m_session) {
        throw std::runtime_error("varjo_SessionInit failed.");
    }

    throwIfError("varjo_SessionInit");
}

VarjoSession::~VarjoSession() {
    if (m_session) {
        varjo_SessionShutDown(m_session);
        m_session = nullptr;
    }
}

void VarjoSession::pollEvents() {
    varjo_Event evt{};
    while (varjo_PollEvent(m_session, &evt)) {
        // P1: events are consumed so the runtime queue does not grow.
    }
}

void VarjoSession::throwIfError(const char* where) const {
    const varjo_Error error = varjo_GetError(m_session);
    if (error == varjo_NoError) return;

    std::string message = where ? where : "Varjo call";
    message += " failed: ";
    message += varjo_GetErrorDesc(error);
    throw std::runtime_error(message);
}

} // namespace VarjoXR
