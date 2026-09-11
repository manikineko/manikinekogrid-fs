/**
 * @file llfloatermkohtmloverlay.cpp
 * @brief Plugin-driven HTML overlay floater (MKO plugin system).
 */

#include "llviewerprecompiledheaders.h"

#include "llfloatermkohtmloverlay.h"

#include "llmediactrl.h"
#include "llfloaterreg.h"
#include "llviewerwindow.h"
#include "lldir.h"
#include "mkopluginmanager.h"

#include <cstdio>
#include <fstream>

LLFloaterMkoHtmlOverlay::LLFloaterMkoHtmlOverlay(const LLSD& key)
:   LLFloater(key),
    mBrowser(nullptr)
{
    mOverlayId = key.has("id") ? key["id"].asString() : "mko_overlay";
}

LLFloaterMkoHtmlOverlay::~LLFloaterMkoHtmlOverlay()
{
    if (!mHtmlFile.empty())
    {
        LLFile::remove(mHtmlFile);
    }
}

bool LLFloaterMkoHtmlOverlay::postBuild()
{
    mBrowser = getChild<LLMediaCtrl>("browser");
    return true;
}

bool LLFloaterMkoHtmlOverlay::matchesKey(const LLSD& key)
{
    return key.has("id") && key["id"].asString() == mOverlayId;
}

void LLFloaterMkoHtmlOverlay::onOpen(const LLSD& key)
{
    applyKeyParams(key);
}

// static
void LLFloaterMkoHtmlOverlay::showFromKey(const LLSD& key)
{
    if (!key.has("id"))
    {
        return;
    }
    LLFloaterReg::showInstance("mko_html_overlay", key, key.has("visible") ? key["visible"].asBoolean() : true);
}

void LLFloaterMkoHtmlOverlay::applyKeyParams(const LLSD& key)
{
    if (key.has("title"))
    {
        setTitle(key["title"].asString());
    }
    if (key.has("closable"))
    {
        setCanClose(key["closable"].asBoolean());
    }

    // Position and size.
    S32 width  = key.has("width")  ? (S32)key["width"].asInteger()  : 512;
    S32 height = key.has("height") ? (S32)key["height"].asInteger() : 384;
    S32 x = key.has("x") ? (S32)key["x"].asInteger() : -1;
    S32 y = key.has("y") ? (S32)key["y"].asInteger() : -1;

    LLRect rect = getRect();
    S32 left = x, bottom = y;
    if (x < 0 || y < 0)
    {
        // center on screen
        S32 view_w = gViewerWindow->getWindowWidthRaw();
        S32 view_h = gViewerWindow->getWindowHeightRaw();
        if (x < 0)
            left = (view_w - width) / 2;
        if (y < 0)
            bottom = (view_h - height) / 2;
    }
    rect.set(left, bottom + height, left + width, bottom);
    setShape(rect);

    if (key.has("opacity"))
    {
        F32 opacity = (F32)key["opacity"].asReal();
        if (opacity > 0.f && opacity < 1.f)
        {
            setBackgroundOpaque(false);
            LLColor4 color = getTransparentColor();
            color.setAlpha(opacity);
            setTransparentColor(LLUIColor(color));
        }
    }

    if (key.has("url"))
    {
        navigateToUrl(key["url"].asString());
    }
    else if (key.has("html"))
    {
        setHtml(key["html"].asString());
    }
}

void LLFloaterMkoHtmlOverlay::navigateToUrl(const std::string& url)
{
    mPendingUrl = url;
    mPendingHtml.clear();
    if (mBrowser && !url.empty())
    {
        mBrowser->navigateTo(url);
    }
}

void LLFloaterMkoHtmlOverlay::setHtml(const std::string& html)
{
    mPendingHtml = html;
    mPendingUrl.clear();
    if (!mBrowser || html.empty())
    {
        return;
    }

    // Write the HTML to a temp file and load it via a file:// URL.
    if (mHtmlFile.empty())
    {
        mHtmlFile = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
                     "mko_overlay_" + mOverlayId + ".html");
    }
    LLFile::remove(mHtmlFile);

    std::ofstream ofs(mHtmlFile.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (ofs.is_open())
    {
        ofs << html;
        ofs.close();
        mBrowser->navigateTo("file://" + mHtmlFile);
    }
    else
    {
        LL_WARNS("MkoPlugin") << "Failed to write HTML overlay temp file " << mHtmlFile << LL_ENDL;
    }
}

void LLFloaterMkoHtmlOverlay::onClose(bool app_quitting)
{
    if (!app_quitting)
    {
        LLSD msg;
        msg["id"] = mOverlayId;
        MkoPluginManager::instance().broadcastToPlugins("MkoOverlayClosed", msg);
    }
    LLFloater::onClose(app_quitting);
}
