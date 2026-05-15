#include <gtest/gtest.h>

#include "app/domain.hpp"

TEST(Domain, HasNightBonusMatchesSupportedKinds) {
    EXPECT_TRUE(app::HasNightBonus(app::PlaceKind::kBar));
    EXPECT_TRUE(app::HasNightBonus(app::PlaceKind::kCinema));
    EXPECT_FALSE(app::HasNightBonus(app::PlaceKind::kMuseum));
    EXPECT_FALSE(app::HasNightBonus(app::PlaceKind::kPark));
}
