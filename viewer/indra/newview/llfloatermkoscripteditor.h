/**
 * @file llfloatermkoscripteditor.h
 * @brief External editor (VS Code / system editor) script editor (MKO plugin system).
 *
 * Opened by plugins via open_script_editor(). The floater writes the script
 * to a temporary file, launches the configured external editor, and watches
 * the file for changes. When the file is saved externally, the floater
 * broadcasts "MkoScriptEditorSaved" and invokes any registered C++ save
 * callbacks through MkoPluginManager.
 */

#ifndef LL_LLFLOATERMKOSCRIPTEDITOR_H
#define LL_LLFLOATERMKOSCRIPTEDITOR_H

#include "llfloater.h"

#include <string>

class LLLiveFile;
class LLTextBox;

class LLFloaterMkoScriptEditor : public LLFloater
{
public:
    LLFloaterMkoScriptEditor(const LLSD& key);
    virtual ~LLFloaterMkoScriptEditor();

    static void showFromKey(const LLSD& key);

    const std::string& getScriptId() const { return mScriptId; }
    std::string getLanguage() const { return mLanguage; }

    /* virtual */ bool postBuild();
    /* virtual */ bool matchesKey(const LLSD& key);
    /* virtual */ void onOpen(const LLSD& key);
    /* virtual */ void onClose(bool app_quitting);

    void onExternalFileChanged(const std::string& new_content);

private:
    void applyKeyParams(const LLSD& key);
    void updateTitle();
    void launchExternalEditor();
    void broadcastSaved(const std::string& content);
    std::string makeTempFileName() const;

    void onClickSave();
    void onClickClose();

    std::string     mScriptId;
    std::string     mLanguage;
    std::string     mTitle;
    std::string     mContent;
    std::string     mLastSavedContent;
    std::string     mTempFile;

    LLTextBox*      mStatusText;
    bool            mHasLaunched;
    LLLiveFile*     mLiveFile;
};

#endif // LL_LLFLOATERMKOSCRIPTEDITOR_H
