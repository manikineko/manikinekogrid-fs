/**
 * @file llfloatermkohtmloverlay.h
 * @brief Plugin-driven HTML overlay floater (MKO plugin system).
 *
 * Renders inline HTML or a web page inside a lightweight floater using
 * the viewer's media (web) engine. Created and controlled by plugins
 * via the MKO plugin API (register_html_overlay etc.).
 */

#ifndef LL_LLFLOATERMKOHTMLOVERLAY_H
#define LL_LLFLOATERMKOHTMLOVERLAY_H

#include "llfloater.h"

#include <string>

class LLMediaCtrl;

class LLFloaterMkoHtmlOverlay : public LLFloater
{
public:
    LLFloaterMkoHtmlOverlay(const LLSD& key);
    virtual ~LLFloaterMkoHtmlOverlay();

    // Show (creating if needed) the overlay registered under key["id"].
    // Key fields: id, title, url, html, x, y, width, height, opacity,
    // closable, visible.
    static void showFromKey(const LLSD& key);

    void navigateToUrl(const std::string& url);
    // Renders the given inline HTML via a temporary local file.
    void setHtml(const std::string& html);

    const std::string& getOverlayId() const { return mOverlayId; }

    /* virtual */ bool postBuild();
    /* virtual */ bool matchesKey(const LLSD& key);
    /* virtual */ void onOpen(const LLSD& key);
    /* virtual */ void onClose(bool app_quitting);

private:
    void applyKeyParams(const LLSD& key);

    std::string     mOverlayId;
    LLMediaCtrl*    mBrowser;
    std::string     mPendingUrl;
    std::string     mPendingHtml;
    std::string     mHtmlFile;   // temp file backing inline HTML, if any
};

#endif // LL_LLFLOATERMKOHTMLOVERLAY_H
