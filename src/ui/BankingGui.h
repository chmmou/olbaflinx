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

#pragma once

// The Qt implementation of the gwenhywfar user interface. The header keeps its
// qt5 name across the version change; the library built from it for Qt 6 is
// libgwengui-qt6.
#include <gwen-gui-qt5/qt5_gui.hpp>

#include <gwenhywfar/gui_be.h>

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QTimer>

namespace olbaflinx::ui {

/**
 * @brief The user interface the banking backend asks, usable from a session
 *  that runs in a thread of its own.
 *
 * A banking session blocks and therefore runs beside the window. Its callbacks
 * arrive in that thread, and every one of them that touches a widget has to
 * cross back. This class overrides those and hands them to the thread it was
 * built in, waiting for the answer.
 *
 * The callbacks come in two kinds and are taken over in two ways. The dialogs
 * are virtual functions of the C++ binding and are overridden below. The
 * progress of a session is not: gwenhywfar keeps those as function pointers on
 * the C interface, and they reach the widgets of the progress dialog just as
 * directly. They are exchanged for forwarding ones in the constructor.
 *
 * Ownership: the creator owns the instance. It outlives the banking instance
 * that uses it and is destroyed after it; the banking backend reaches into the
 * interface while it shuts down. Nothing here frees the banking side.
 *
 * Concurrency: an instance belongs to the thread that built it, and that thread
 * runs an event loop. A callback from any other thread is handed over and waited
 * for; a callback from the owning thread is answered on the spot, which is what
 * keeps a caller from the window itself out of a deadlock.
 *
 * Secrets: this class does not override the callbacks for a password or a
 * password status. A PIN and a TAN therefore travel the way gwenhywfar takes
 * them and never touch code of this application. What it does hold is the cache
 * gwenhywfar keeps a PIN in, because that cache belongs to the interface and the
 * banking backend never empties it.
 */
class BankingGui : public QT5_Gui
{
public:
    /**
     * How long a PIN stays in the cache of the interface after a fetch has
     * ended. A fetch that starts within that span finds it and asks for nothing.
     */
    static constexpr int passwordCacheLifetimeMs = 5 * 60 * 1000;

    /**
     * @param passwordCacheLifetimeMs How long the cached PIN outlives a fetch.
     *  The default is the span the application uses; a shorter one is what makes
     *  the expiry measurable without waiting for it.
     */
    explicit BankingGui(int passwordCacheLifetimeMs = BankingGui::passwordCacheLifetimeMs);
    ~BankingGui() override;

    BankingGui(const BankingGui &) = delete;
    BankingGui &operator=(const BankingGui &) = delete;
    BankingGui(BankingGui &&) = delete;
    BankingGui &operator=(BankingGui &&) = delete;

    /**
     * @brief Keep the cached PIN for a fetch that is starting.
     *
     * Stops the running expiry. Without it the cache could be emptied while a
     * session reads it, and a session asks for the PIN once per signed message.
     */
    void holdPasswordCache();

    /**
     * @brief Let the cached PIN expire, from now on.
     *
     * Called when a fetch has ended, whichever way. The next fetch within the
     * lifetime finds the PIN, a later one does not.
     */
    void expirePasswordCacheLater();

    /**
     * @brief Empty the cache of the interface at once.
     *
     * Works on the interface itself and not through GWEN_Gui_SetPasswordStatus,
     * which reaches for the interface of the calling thread and would find none
     * here.
     */
    void clearPasswordCache();

    /** Whether an expiry is on its way. */
    [[nodiscard]] bool isPasswordCacheExpiring() const;

protected:
    int execDialog(GWEN_DIALOG *dlg, uint32_t guiid) override;
    int openDialog(GWEN_DIALOG *dlg, uint32_t guiid) override;
    int closeDialog(GWEN_DIALOG *dlg) override;
    int runDialog(GWEN_DIALOG *dlg, int untilEnd) override;

    int getFileName(const char *caption,
                    GWEN_GUI_FILENAME_TYPE fnt,
                    uint32_t flags,
                    const char *patterns,
                    GWEN_BUFFER *pathBuffer,
                    uint32_t guiid) override;

private:
    /**
     * The five callbacks that carry the course of a session. Each of them hands
     * its work to the owning thread and then calls the function that gwenhywfar
     * had in place before, which is what actually writes into the dialog.
     *
     * The interface they are called with is the one they belong to, so an
     * instance finds itself through it and no global state is involved.
     */
    static uint32_t forwardProgressStart(GWEN_GUI *gui,
                                         uint32_t progressFlags,
                                         const char *title,
                                         const char *text,
                                         uint64_t total,
                                         uint32_t guiid);
    static int forwardProgressAdvance(GWEN_GUI *gui, uint32_t id, uint64_t progress);
    static int forwardProgressSetTotal(GWEN_GUI *gui, uint32_t id, uint64_t total);
    static int forwardProgressLog(GWEN_GUI *gui,
                                  uint32_t id,
                                  GWEN_LOGGER_LEVEL level,
                                  const char *text);
    static int forwardProgressEnd(GWEN_GUI *gui, uint32_t id);

    /**
     * Runs the given call in the thread this interface belongs to and answers
     * with its result.
     *
     * A call that already runs in that thread is made on the spot. Handing it
     * over instead would wait for a thread that is waiting for itself.
     */
    template<typename Callable> auto callOnOwnerThread(Callable call) -> decltype(call())
    {
        if (QThread::currentThread() == m_ownerThread.thread()) {
            return call();
        }

        decltype(call()) result{};
        QMetaObject::invokeMethod(&m_ownerThread,
                                  std::move(call),
                                  Qt::BlockingQueuedConnection,
                                  &result);

        return result;
    }

    /**
     * Lives in the thread that built this interface. It is what a callback from
     * a session hands its work to, and what the expiry of the cache runs on.
     */
    QObject m_ownerThread;
    QTimer m_passwordCacheExpiry;

    // What gwenhywfar had in place before the forwarding ones took over. They
    // do the work; the forwarding ones only decide which thread it happens in.
    GWEN_GUI_PROGRESS_START_FN m_progressStart = nullptr;
    GWEN_GUI_PROGRESS_ADVANCE_FN m_progressAdvance = nullptr;
    GWEN_GUI_PROGRESS_SETTOTAL_FN m_progressSetTotal = nullptr;
    GWEN_GUI_PROGRESS_LOG_FN m_progressLog = nullptr;
    GWEN_GUI_PROGRESS_END_FN m_progressEnd = nullptr;
};

} // namespace olbaflinx::ui
