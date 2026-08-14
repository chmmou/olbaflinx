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

using namespace olbaflinx::ui;

BankingGui::BankingGui(int passwordCacheLifetimeMs)
{
    m_passwordCacheExpiry.setSingleShot(true);
    m_passwordCacheExpiry.setInterval(passwordCacheLifetimeMs);

    QObject::connect(&m_passwordCacheExpiry, &QTimer::timeout, &m_ownerThread, [this] {
        clearPasswordCache();
    });
}

BankingGui::~BankingGui()
{
    // Whatever is still cached goes with the interface. gwenhywfar releases the
    // cache without overwriting it, so the last thing this class does is empty
    // it itself.
    clearPasswordCache();
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
