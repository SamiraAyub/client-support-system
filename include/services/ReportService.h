#pragma once
// Builds the management report. Any logged-in user may view it.

#include "models/Report.h"
#include "repositories/ReportRepository.h"
#include "services/Session.h"

namespace csms {

class ReportService {
public:
    ReportService(const ReportRepository& repository, const Session& session)
        : repository_(repository), session_(session) {}

    // Throws AuthorizationError when nobody is logged in.
    SystemReport generate() const;

private:
    const ReportRepository& repository_;
    const Session& session_;
};

} // namespace csms
