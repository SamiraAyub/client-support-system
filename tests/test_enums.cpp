#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "models/Enums.h"

using namespace csms;

TEST(EnumsTest, PriorityRoundTrips) {
    for (Priority p : {Priority::Low, Priority::Medium, Priority::High}) {
        EXPECT_EQ(priorityFromString(toString(p)), p);
    }
}

TEST(EnumsTest, CaseStatusRoundTrips) {
    for (CaseStatus s : {CaseStatus::Open, CaseStatus::InProgress,
                         CaseStatus::Resolved, CaseStatus::Closed}) {
        EXPECT_EQ(caseStatusFromString(toString(s)), s);
    }
}

TEST(EnumsTest, StaffRoleRoundTrips) {
    for (StaffRole r : {StaffRole::Administrator, StaffRole::SupportAgent}) {
        EXPECT_EQ(staffRoleFromString(toString(r)), r);
    }
}

TEST(EnumsTest, DisplayStringsAreHumanFriendly) {
    EXPECT_EQ(toDisplayString(CaseStatus::InProgress), "In Progress");
    EXPECT_EQ(toDisplayString(StaffRole::SupportAgent), "Support Agent");
    EXPECT_EQ(toDisplayString(Priority::High), "High");
}

TEST(EnumsTest, InvalidTextThrowsValidationError) {
    EXPECT_THROW(priorityFromString("Urgent"), ValidationError);
    EXPECT_THROW(caseStatusFromString("Done"), ValidationError);
    EXPECT_THROW(staffRoleFromString("Manager"), ValidationError);
    EXPECT_THROW(priorityFromString(""), ValidationError);
}

TEST(EnumsTest, ParsingIsCaseSensitiveOnStoredForm) {
    // The stored form is exact; friendly input parsing belongs to the CLI layer.
    EXPECT_THROW(priorityFromString("low"), ValidationError);
}
