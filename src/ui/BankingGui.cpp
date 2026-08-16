/**
 * Copyright (C) 2022-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ui/BankingGui.h"

#include "ui/Logging.h"

#include <gwenhywfar/db.h>
#include <gwenhywfar/gui.h>
#include <gwenhywfar/inherit.h>

using namespace olbaflinx::ui;

// Lets an instance be found from the C interface it belongs to. The binding
// this class derives from does the same for itself, and the two are kept apart
// by the type name the macro builds its key from.
GWEN_INHERIT(GWEN_GUI, BankingGui)

namespace {

/**
 * The instance an interface belongs to, or nothing where the interface is not
 * one of ours. gwenhywfar hands a callback the interface and no further
 * context, and this is what turns the one into the other.
 */
BankingGui *ownerOf(GWEN_GUI *gui)
{
    return gui == nullptr ? nullptr : GWEN_INHERIT_GETDATA(GWEN_GUI, BankingGui, gui);
}

/**
 * Makes an interface the one of the current thread while it lives and puts back
 * what was there before.
 *
 * gwenhywfar holds its interface per thread and looks it up in the thread a
 * call runs in, not in the one it was made from. A progress that was handed
 * over therefore arrives in a thread that knows nothing of the session, and the
 * dialogs it wants to open would answer that no interface is there.
 */
class ThreadInterface
{
public:
    explicit ThreadInterface(GWEN_GUI *gui)
        : m_previous(GWEN_Gui_GetGui())
    {
        // Held over the change: setting a new interface releases the one it
        // replaces, and that one is what has to go back afterwards.
        if (m_previous != nullptr) {
            GWEN_Gui_Attach(m_previous);
        }

        GWEN_Gui_SetGui(gui);
    }

    ~ThreadInterface()
    {
        GWEN_Gui_SetGui(m_previous);

        if (m_previous != nullptr) {
            GWEN_Gui_free(m_previous);
        }
    }

    ThreadInterface(const ThreadInterface &) = delete;
    ThreadInterface &operator=(const ThreadInterface &) = delete;
    ThreadInterface(ThreadInterface &&) = delete;
    ThreadInterface &operator=(ThreadInterface &&) = delete;

private:
    GWEN_GUI *m_previous;
};

} // namespace

BankingGui::BankingGui(int passwordCacheLifetimeMs)
{
    m_passwordCacheExpiry.setSingleShot(true);
    m_passwordCacheExpiry.setInterval(passwordCacheLifetimeMs);

    QObject::connect(&m_passwordCacheExpiry, &QTimer::timeout, &m_ownerThread, [this] {
        clearPasswordCache();
    });

    GWEN_GUI *gui = getCInterface();

    GWEN_INHERIT_SETDATA(GWEN_GUI, BankingGui, gui, this, nullptr);

    m_progressStart = GWEN_Gui_SetProgressStartFn(gui, &BankingGui::forwardProgressStart);
    m_progressAdvance = GWEN_Gui_SetProgressAdvanceFn(gui, &BankingGui::forwardProgressAdvance);
    m_progressSetTotal = GWEN_Gui_SetProgressSetTotalFn(gui, &BankingGui::forwardProgressSetTotal);
    m_progressLog = GWEN_Gui_SetProgressLogFn(gui, &BankingGui::forwardProgressLog);
    m_progressEnd = GWEN_Gui_SetProgressEndFn(gui, &BankingGui::forwardProgressEnd);
}

BankingGui::~BankingGui()
{
    GWEN_GUI *gui = getCInterface();

    // Put back before the link is given up. A callback that arrived afterwards
    // would otherwise look for an instance that is no longer there.
    GWEN_Gui_SetProgressStartFn(gui, m_progressStart);
    GWEN_Gui_SetProgressAdvanceFn(gui, m_progressAdvance);
    GWEN_Gui_SetProgressSetTotalFn(gui, m_progressSetTotal);
    GWEN_Gui_SetProgressLogFn(gui, m_progressLog);
    GWEN_Gui_SetProgressEndFn(gui, m_progressEnd);

    GWEN_INHERIT_UNLINK(GWEN_GUI, BankingGui, gui)

    // Whatever is still cached goes with the interface. gwenhywfar releases the
    // cache without overwriting it, so the last thing this class does is empty
    // it itself.
    clearPasswordCache();
}

uint32_t BankingGui::forwardProgressStart(GWEN_GUI *gui,
                                          uint32_t progressFlags,
                                          const char *title,
                                          const char *text,
                                          uint64_t total,
                                          uint32_t guiid)
{
    BankingGui *self = ownerOf(gui);
    if (self == nullptr || self->m_progressStart == nullptr) {
        return 0;
    }

    return self->callOnOwnerThread([self, gui, progressFlags, title, text, total, guiid] {
        const ThreadInterface interface(gui);

        return self->m_progressStart(gui, progressFlags, title, text, total, guiid);
    });
}

int BankingGui::forwardProgressAdvance(GWEN_GUI *gui, uint32_t id, uint64_t progress)
{
    BankingGui *self = ownerOf(gui);
    if (self == nullptr || self->m_progressAdvance == nullptr) {
        return GWEN_ERROR_NOT_SUPPORTED;
    }

    const int result = self->callOnOwnerThread([self, gui, id, progress] {
        const ThreadInterface interface(gui);

        return self->m_progressAdvance(gui, id, progress);
    });

    self->noteAbort(result);

    return result;
}

int BankingGui::forwardProgressSetTotal(GWEN_GUI *gui, uint32_t id, uint64_t total)
{
    BankingGui *self = ownerOf(gui);
    if (self == nullptr || self->m_progressSetTotal == nullptr) {
        return GWEN_ERROR_NOT_SUPPORTED;
    }

    return self->callOnOwnerThread([self, gui, id, total] {
        const ThreadInterface interface(gui);

        return self->m_progressSetTotal(gui, id, total);
    });
}

int BankingGui::forwardProgressLog(GWEN_GUI *gui,
                                   uint32_t id,
                                   GWEN_LOGGER_LEVEL level,
                                   const char *text)
{
    BankingGui *self = ownerOf(gui);
    if (self == nullptr || self->m_progressLog == nullptr) {
        return GWEN_ERROR_NOT_SUPPORTED;
    }

    const int result = self->callOnOwnerThread([self, gui, id, level, text] {
        const ThreadInterface interface(gui);

        return self->m_progressLog(gui, id, level, text);
    });

    self->noteAbort(result);

    return result;
}

int BankingGui::forwardProgressEnd(GWEN_GUI *gui, uint32_t id)
{
    BankingGui *self = ownerOf(gui);
    if (self == nullptr || self->m_progressEnd == nullptr) {
        return GWEN_ERROR_NOT_SUPPORTED;
    }

    return self->callOnOwnerThread([self, gui, id] {
        const ThreadInterface interface(gui);

        return self->m_progressEnd(gui, id);
    });
}

bool BankingGui::userAborted() const
{
    return m_userAborted;
}

void BankingGui::forgetAbort()
{
    m_userAborted = false;
}

/**
 * The progress callbacks are where the wish to stop arrives: gwenhywfar asks
 * through them whether the running operation is to be given up, and any answer
 * other than nought means it is. The one that says the user asked for it is
 * kept apart from a callback that simply could not be served.
 */
void BankingGui::noteAbort(int result)
{
    if (result == GWEN_ERROR_USER_ABORTED) {
        m_userAborted = true;
    }
}

void BankingGui::holdPasswordCache()
{
    m_passwordCacheExpiry.stop();
}

void BankingGui::expirePasswordCacheLater()
{
    m_passwordCacheExpiry.start();
}

void BankingGui::clearPasswordCache()
{
    GWEN_DB_NODE *passwords = GWEN_Gui_GetPasswordDb(getCInterface());
    if (passwords == nullptr) {
        return;
    }

    GWEN_DB_ClearGroup(passwords, nullptr);

    qCDebug(lcUi) << "the cached credentials of the banking interface were dropped";
}

bool BankingGui::isPasswordCacheExpiring() const
{
    return m_passwordCacheExpiry.isActive();
}

int BankingGui::execDialog(GWEN_DIALOG *dlg, uint32_t guiid)
{
    return callOnOwnerThread([this, dlg, guiid] { return QT5_Gui::execDialog(dlg, guiid); });
}

int BankingGui::openDialog(GWEN_DIALOG *dlg, uint32_t guiid)
{
    return callOnOwnerThread([this, dlg, guiid] { return QT5_Gui::openDialog(dlg, guiid); });
}

int BankingGui::closeDialog(GWEN_DIALOG *dlg)
{
    return callOnOwnerThread([this, dlg] { return QT5_Gui::closeDialog(dlg); });
}

int BankingGui::runDialog(GWEN_DIALOG *dlg, int untilEnd)
{
    return callOnOwnerThread([this, dlg, untilEnd] { return QT5_Gui::runDialog(dlg, untilEnd); });
}

int BankingGui::getFileName(const char *caption,
                            GWEN_GUI_FILENAME_TYPE fnt,
                            uint32_t flags,
                            const char *patterns,
                            GWEN_BUFFER *pathBuffer,
                            uint32_t guiid)
{
    return callOnOwnerThread([this, caption, fnt, flags, patterns, pathBuffer, guiid] {
        return QT5_Gui::getFileName(caption, fnt, flags, patterns, pathBuffer, guiid);
    });
}
