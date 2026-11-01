#include <gtest/gtest.h>

#include <set>
#include <utility>

#include "services/CaseWorkflow.h"

using namespace csms;
using csms::CaseStatus;

namespace {
const CaseStatus kAll[] = {CaseStatus::Open, CaseStatus::InProgress, CaseStatus::Resolved,
                           CaseStatus::Closed};
}

TEST(CaseWorkflowTest, EveryPairMatchesTheDocumentedStateMachine) {
    const std::set<std::pair<CaseStatus, CaseStatus>> allowed = {
        {CaseStatus::Open, CaseStatus::InProgress},
        {CaseStatus::InProgress, CaseStatus::Open},
        {CaseStatus::InProgress, CaseStatus::Resolved},
        {CaseStatus::Resolved, CaseStatus::Closed},
    };
    for (const CaseStatus from : kAll) {
        for (const CaseStatus to : kAll) {
            EXPECT_EQ(workflow::isAllowedTransition(from, to), allowed.count({from, to}) == 1)
                << toString(from) << " -> " << toString(to);
        }
    }
}

TEST(CaseWorkflowTest, ClosedIsFinal) {
    for (const CaseStatus to : kAll) {
        EXPECT_FALSE(workflow::isAllowedTransition(CaseStatus::Closed, to));
        EXPECT_FALSE(workflow::isReopen(CaseStatus::Closed, to));
    }
    EXPECT_TRUE(workflow::allowedNextStatuses(CaseStatus::Closed).empty());
}

TEST(CaseWorkflowTest, ReopenIsOnlyResolvedToOpenAndIsNotAnOrdinaryTransition) {
    EXPECT_TRUE(workflow::isReopen(CaseStatus::Resolved, CaseStatus::Open));
    EXPECT_FALSE(workflow::isAllowedTransition(CaseStatus::Resolved, CaseStatus::Open));
    EXPECT_FALSE(workflow::isReopen(CaseStatus::InProgress, CaseStatus::Open));
    EXPECT_FALSE(workflow::isReopen(CaseStatus::Open, CaseStatus::Open));
}

TEST(CaseWorkflowTest, NoStatusMayTransitionToItself) {
    for (const CaseStatus s : kAll) {
        EXPECT_FALSE(workflow::isAllowedTransition(s, s));
    }
}

TEST(CaseWorkflowTest, AllowedNextStatusesListsOrdinaryMovesOnly) {
    using V = std::vector<CaseStatus>;
    EXPECT_EQ(workflow::allowedNextStatuses(CaseStatus::Open), V{CaseStatus::InProgress});
    EXPECT_EQ(workflow::allowedNextStatuses(CaseStatus::InProgress),
              (V{CaseStatus::Open, CaseStatus::Resolved}));
    EXPECT_EQ(workflow::allowedNextStatuses(CaseStatus::Resolved), V{CaseStatus::Closed});
}
