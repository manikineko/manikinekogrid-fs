/**
 * @file llfloatermkohealth.h
 * @brief MKO health system floater: avatar health and region health.
 *
 * Shows the avatar's server-driven health (legacy "Health" message,
 * fully compatible with Second Life and OpenSim) together with a live
 * region health dashboard (fps, time dilation, agent/object counts).
 */

#ifndef LL_LLFLOATERMKOHEALTH_H
#define LL_LLFLOATERMKOHEALTH_H

#include "llfloater.h"

class LLProgressBar;

class LLFloaterMkoHealth : public LLFloater
{
public:
    LLFloaterMkoHealth(const LLSD& key);
    virtual ~LLFloaterMkoHealth();

    /* virtual */ bool postBuild();
    /* virtual */ void onOpen(const LLSD& key);
    /* virtual */ void draw();

private:
    void refresh();

    LLProgressBar*  mHealthBar;
    F64             mLastRefreshSeconds;
};

#endif // LL_LLFLOATERMKOHEALTH_H
