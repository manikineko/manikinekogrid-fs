/**
 * @file llfloatermkoscripteditor.cpp
 * @brief External editor (VS Code / system editor) script editor (MKO plugin system).
 */

#include "llviewerprecompiledheaders.h"

#include "llfloatermkoscripteditor.h"

#include "llbutton.h"
#include "lldir.h"
#include "llexternaleditor.h"
#include "llfile.h"
#include "llfloaterreg.h"
#include "lllivefile.h"
#include "llnotificationsutil.h"
#include "lltextbox.h"
#include "mkopluginmanager.h"

#include <fstream>

class MkoScriptLiveFile : public LLLiveFile
{
public:
    MkoScriptLiveFile(const std::string& filename, LLFloaterMkoScriptEditor* editor)
        : LLLiveFile(filename, 1.0f),
          mEditor(editor)
    {
    }

    const std::string& getContent() const { return mContent; }

protected:
    bool loadFile() override
    {
        std::ifstream ifs(filename().c_str(), std::ios::binary);
        if (!ifs.is_open())
        {
            return false;
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        mContent = ss.str();
        return true;
    }

    void changed() override
    {
        if (mEditor)
        {
            mEditor->onExternalFileChanged(mContent);
        }
    }

private:
    LLFloaterMkoScriptEditor* mEditor;
    std::string mContent;
};

LLFloaterMkoScriptEditor::LLFloaterMkoScriptEditor(const LLSD& key)
:   LLFloater(key),
    mStatusText(nullptr),
    mHasLaunched(false),
    mLiveFile(nullptr)
{
    mScriptId = key.has("id") ? key["id"].asString() : "mko_script";
    mLanguage = key.has("language") ? key["language"].asString() : "lsl";
    mTitle = key.has("title") ? key["title"].asString() : "Script";
    mContent = key.has("content") ? key["content"].asString() : "";
    mLastSavedContent = mContent;
}

LLFloaterMkoScriptEditor::~LLFloaterMkoScriptEditor()
{
    delete mLiveFile;
    mLiveFile = nullptr;

    if (!mTempFile.empty())
    {
        LLFile::remove(mTempFile);
    }
}

bool LLFloaterMkoScriptEditor::postBuild()
{
    mStatusText = getChild<LLTextBox>("status_text");
    if (mStatusText)
    {
        mStatusText->setText(LLStringExplicit("Initializing external editor..."));
    }

    getChild<LLUICtrl>("save_btn")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickSave(); });
    getChild<LLUICtrl>("open_btn")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { launchExternalEditor(); });
    getChild<LLUICtrl>("close_btn")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickClose(); });

    return true;
}

bool LLFloaterMkoScriptEditor::matchesKey(const LLSD& key)
{
    return key.has("id") && key["id"].asString() == mScriptId;
}

// static
void LLFloaterMkoScriptEditor::showFromKey(const LLSD& key)
{
    if (!key.has("id"))
    {
        return;
    }
    LLFloaterReg::showInstance("mko_script_editor", key, true);
}

void LLFloaterMkoScriptEditor::onOpen(const LLSD& key)
{
    applyKeyParams(key);

    mTempFile = makeTempFileName();
    if (mTempFile.empty())
    {
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Could not create temporary file for external editor."));
        return;
    }

    // Write the current content to the temp file.
    {
        std::ofstream ofs(mTempFile.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", "Could not write temporary script file."));
            return;
        }
        ofs << mContent;
        ofs.close();
    }

    mLastSavedContent = mContent;
    updateTitle();

    // Start watching the file for external saves.
    delete mLiveFile;
    mLiveFile = new MkoScriptLiveFile(mTempFile, this);
    mLiveFile->addToEventTimer();

    // Launch the external editor.
    launchExternalEditor();
}

void LLFloaterMkoScriptEditor::onClose(bool app_quitting)
{
    if (!app_quitting)
    {
        LLSD msg;
        msg["id"] = mScriptId;
        MkoPluginManager::instance().broadcastToPlugins("MkoScriptEditorClosed", msg);
    }
    LLFloater::onClose(app_quitting);
}

void LLFloaterMkoScriptEditor::applyKeyParams(const LLSD& key)
{
    if (key.has("title"))
    {
        mTitle = key["title"].asString();
        updateTitle();
    }
    if (key.has("language"))
    {
        mLanguage = key["language"].asString();
    }
    if (key.has("content"))
    {
        mContent = key["content"].asString();
        mLastSavedContent = mContent;
    }
}

void LLFloaterMkoScriptEditor::updateTitle()
{
    setTitle(mTitle.empty() ? "Script" : mTitle);
}

std::string LLFloaterMkoScriptEditor::makeTempFileName() const
{
    std::string filename = "mko_script_" + mScriptId;
    // Replace any path separators or other unsafe characters.
    LLStringUtil::replaceChar(filename, '/', '_');
    LLStringUtil::replaceChar(filename, '\\', '_');
    filename += ".lsl";
    return gDirUtilp->getExpandedFilename(LL_PATH_CACHE, filename);
}

void LLFloaterMkoScriptEditor::launchExternalEditor()
{
    LLExternalEditor ed;
    LLExternalEditor::EErrorCode status = ed.setCommand("LL_SCRIPT_EDITOR");
    if (status != LLExternalEditor::EC_SUCCESS)
    {
        std::string err = LLExternalEditor::getErrorMessage(status);
        LL_WARNS("MkoPlugin") << "External editor setup failed: " << err << LL_ENDL;
        if (mStatusText)
        {
            mStatusText->setText(LLStringExplicit("External editor not set. Configure Preferences > Advanced > ExternalEditor or set LL_SCRIPT_EDITOR."));
        }
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", err));
        return;
    }

    status = ed.run(mTempFile);
    if (status != LLExternalEditor::EC_SUCCESS)
    {
        std::string err = LLExternalEditor::getErrorMessage(status);
        LL_WARNS("MkoPlugin") << "Failed to launch external editor: " << err << LL_ENDL;
        if (mStatusText)
        {
            mStatusText->setText("Failed to launch editor: " + err);
        }
        LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", err));
        return;
    }

    mHasLaunched = true;
    if (mStatusText)
    {
        mStatusText->setText("Editing " + mTempFile + " in external editor. Save the file to update the script.");
    }
    LL_INFOS("MkoPlugin") << "Launched external editor for script '" << mScriptId
                          << "' at " << mTempFile << LL_ENDL;
}

void LLFloaterMkoScriptEditor::onExternalFileChanged(const std::string& new_content)
{
    if (new_content == mLastSavedContent)
    {
        // No real change, ignore.
        return;
    }

    mContent = new_content;
    mLastSavedContent = new_content;
    broadcastSaved(new_content);

    if (mStatusText)
    {
        mStatusText->setText("External changes saved. " + std::to_string(new_content.size()) + " bytes.");
    }
}

void LLFloaterMkoScriptEditor::broadcastSaved(const std::string& content)
{
    LLSD msg;
    msg["id"] = mScriptId;
    msg["language"] = mLanguage;
    msg["content"] = content;

    // Notify any C++ listener (e.g., the legacy script preview floater) first.
    MkoPluginManager::instance().onScriptEditorSaved(msg);
    MkoPluginManager::instance().broadcastToPlugins("MkoScriptEditorSaved", msg);

    LL_INFOS("MkoPlugin") << "External editor saved script '" << mScriptId
                          << "' (" << content.size() << " bytes)" << LL_ENDL;
}

void LLFloaterMkoScriptEditor::onClickSave()
{
    if (mLiveFile)
    {
        if (mLiveFile->checkAndReload())
        {
            // checkAndReload() already triggered changed() if the file was updated.
            // If the file is unchanged but we want to force a save, read directly.
        }
        else
        {
            // Force a read of the current file.
            std::ifstream ifs(mTempFile.c_str(), std::ios::binary);
            if (ifs.is_open())
            {
                std::ostringstream ss;
                ss << ifs.rdbuf();
                onExternalFileChanged(ss.str());
            }
        }
    }
}

void LLFloaterMkoScriptEditor::onClickClose()
{
    closeFloater(false);
}
