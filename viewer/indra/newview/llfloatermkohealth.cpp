/**
 * @file llfloatermkohealth.cpp
 * @brief MKO health system floater: avatar health and region health.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloatermkohealth.h"

#include "llprogressbar.h"
#include "lltextbase.h"
#include "llstatusbar.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "llfloaterreg.h"
#include "lltrans.h"
#include "mkopluginmanager.h"

static const F64 REFRESH_INTERVAL_SECONDS = 1.0;

LLFloaterMkoHealth::LLFloaterMkoHealth(const LLSD& key)
:   LLFloater(key),
    mHealthBar(nullptr),
    mLastRefreshSeconds(0.0)
{
}

LLFloaterMkoHealth::~LLFloaterMkoHealth()
{
}

bool LLFloaterMkoHealth::postBuild()
{
    mHealthBar = getChild<LLProgressBar>("health_bar");
    mLastRefreshSeconds = 0.0;
    return true;
}

void LLFloaterMkoHealth::onOpen(const LLSD& key)
{
    refresh();
}

void LLFloaterMkoHealth::draw()
{
    F64 now = LLTimer::getTotalTime() * 1.0e-6;
    if (now - mLastRefreshSeconds >= REFRESH_INTERVAL_SECONDS)
    {
        mLastRefreshSeconds = now;
        refresh();
    }
    LLFloater::draw();
}

void LLFloaterMkoHealth::refresh()
{
    // Avatar health (server-driven via the legacy "Health" message).
    S32 health = gStatusBar ? gStatusBar->getHealth() : 0;
    S32 max_health = MkoPluginManager::getAvatarHealthMax();
    if (max_health <= 0)
    {
        max_health = 100;
    }

    if (mHealthBar)
    {
        F32 pct = 1.0f;
        if (max_health > 0)
        {
            pct = (F32)llclamp<S32>(health, 0, max_health) / (F32)max_health;
        }
        mHealthBar->setValue(pct);
    }

    LLUICtrl* health_value = findChild<LLUICtrl>("health_value");
    if (health_value)
    {
        health_value->setValue(llformat("%d / %d", health, max_health));
    }

    // Region health dashboard.
    std::string stats_text = MkoPluginManager::getRegionStatsText();
    LLUICtrl* region_stats = findChild<LLUICtrl>("region_stats");
    if (region_stats)
    {
        region_stats->setValue(stats_text);
    }
}
