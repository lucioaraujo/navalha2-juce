#pragma once

#include <vector>

namespace navalha::validation
{
struct TruePeakFixture
{
    int caseNumber = 0;
    double expectedDbtp = 0.0;
    bool derivedTransient = false;
    std::vector<float> samples;
};

// Mathematical EBU Tech 3341 cases 15-23 at 48 kHz. Cases 20-23 are
// reproducible derivatives of the specification and not copies of EBU WAVs.
[[nodiscard]] std::vector<TruePeakFixture> makeEbuTruePeakFixtures();
}
