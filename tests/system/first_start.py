#!/usr/bin/env python3
"""Walks the way a user takes on a first start, over a WebDriver that speaks
to the accessibility interface.

The application is started against an empty configuration, a data vault is
created, opened and closed again. Nothing here reaches into the source of the
application: the driver sees what a screen reader sees, and every step is taken
through that.

Elements are addressed by their accessible id, never by their visible name. Qt
fills that id from the object name and hands it over as a path through the
object tree, so the entry that closes a vault arrives as
"QApplication.UiApp.appMenuBar.appFileMenu.appCloseStorageAction". The run
matches the last part of it: it is not translated, it does not move when a
label is reworded, and unlike the whole path it survives an element being
reparented.

**Why XPath and not the id strategy.** The server offers "accessibility id" as
a strategy, and it fits this run exactly - except that it only returns elements
that are visible and sensitive. Three of the steps below ask whether a command
is locked, and a locked command would then simply not be found, which reads
like a missing element instead of a disabled one. The XPath tree carries the
same id as an attribute and filters nothing, so that is what the run uses.
"""

import os
import secrets
import string
import sys
import tempfile
import threading
import time

from appium import webdriver
from appium.options.common.base import AppiumOptions
from appium.webdriver.common.appiumby import AppiumBy
from selenium.common.exceptions import NoSuchElementException

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402

VAULT_NAME = "outside-run"
SERVER = os.environ.get("SELENIUM_SERVER", "http://127.0.0.1:4723")

failures = []
announcements = []
started = time.monotonic()


def report(passed, what):
    print(f"{'ok  ' if passed else 'FAIL'}  [{time.monotonic() - started:6.1f}s]  {what}",
          flush=True)
    if not passed:
        failures.append(what)
    return passed


def a_password():
    """A password that satisfies what the application asks of one.

    Made per run and never written down. A repository is no place for one, and
    a run has no reason to take the same one twice.
    """
    parts = [
        secrets.choice(string.ascii_lowercase),
        secrets.choice(string.ascii_uppercase),
        secrets.choice(string.digits),
        secrets.choice("!$%&=?+*#"),
    ]
    parts += [secrets.choice(string.ascii_letters + string.digits) for _ in range(12)]
    secrets.SystemRandom().shuffle(parts)
    return "".join(parts)


def listen_for_announcements():
    """The one thing the driver does not forward.

    Asked over the protocol, the status bar carries no text at all: it hands
    out an empty name and no text interface. What it does instead is raise an
    announcement whenever its message changes, which is what reaches a screen
    reader. The driver has no verb for that, so the run listens on the bus
    itself, next to everything else it does through the driver.
    """
    listener = Atspi.EventListener.new(lambda event: announcements.append(event.any_data))
    listener.register("object:announcement")
    threading.Thread(target=Atspi.event_main, daemon=True).start()
    return listener


def by_identifier(identifier):
    """The last part of the accessible id, as an XPath over the tree the server
    builds. The dot in front keeps a short name from matching a longer one."""
    return f"//*[contains(@accessibility-id, '.{identifier}')]"


def find(driver, identifier):
    return driver.find_element(AppiumBy.XPATH, by_identifier(identifier))


def find_all(driver, identifier):
    return driver.find_elements(AppiumBy.XPATH, by_identifier(identifier))


def exists(driver, identifier):
    return bool(find_all(driver, identifier))


def wait_until(condition, seconds=20):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            if condition():
                return True
        except NoSuchElementException:
            pass
        time.sleep(0.3)
    return False


def start_driver(binary):
    """Starts the application against a configuration directory of its own.

    An empty one: the way this run walks is the way of a first start, and a
    setting left behind by an earlier run would make it a different way. The
    driver launches a command line, so the environment goes in front of it.
    """
    home = tempfile.mkdtemp(prefix="olbaflinx-outside-")

    # The language goes in front of the command as well. It reaches the driver
    # through the environment but not the application the driver starts, and a
    # second walk under another language would otherwise walk under the same
    # one twice without saying so.
    language = os.environ.get("LC_ALL") or os.environ.get("LANG") or "C.UTF-8"

    launch = (f"env HOME={home} XDG_CONFIG_HOME={home}/.config "
              f"LANG={language} LC_ALL={language} "
              f"QT_QPA_PLATFORM=xcb QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 {binary}")

    options = AppiumOptions()
    options.set_capability("app", launch)
    options.set_capability("platformName", "Linux")
    options.set_capability("automationName", "AT-SPI")
    return webdriver.Remote(command_executor=SERVER, options=options)


def take_menu_entry(driver, menu_identifier, entry_identifier):
    """Takes an entry of the menu bar.

    Pressing the entry alone does nothing while its menu is closed, so the
    title is opened first. That is the way a user goes as well.
    """
    for title in driver.find_elements(AppiumBy.XPATH, "//menu_bar/*"):
        title.click()
        if wait_until(lambda: find(driver, entry_identifier).is_displayed(), seconds=3):
            break
    find(driver, entry_identifier).click()


def walk_the_way(driver, password):
    report(exists(driver, "btnNewStorageItem"),
           "the first page offers the way to a new vault")
    report(bool(find(driver, "lblStorageInfoTitle").text),
           "the first page carries a heading that names the application")

    # The commands that need an open vault say so rather than failing when
    # taken. A run inside the process can ask whether an action is disabled;
    # only a run from outside sees that a user meets it disabled.
    for locked in ("appCloseStorageAction", "appSetupAssistantAction",
                   "appFetchTransactionsAction", "appResetLayoutAction"):
        report(not find(driver, locked).is_enabled(),
               f"{locked} is not available while no vault is open")
    for offered in ("appNewStorageAction", "appQuitAction"):
        report(find(driver, offered).is_enabled(),
               f"{offered} is available on the first page")

    find(driver, "btnNewStorageItem").click()
    report(wait_until(lambda: exists(driver, "UiNewStorageDialog")),
           "the dialog for a new vault opens")

    find(driver, "lineEditStorageName").send_keys(VAULT_NAME)
    find(driver, "lineEditPassword").send_keys(password)
    find(driver, "lineEditPasswordConfirm").send_keys(password)

    report(wait_until(lambda: find(driver, "pushButtonOk").is_enabled(), seconds=5),
           "the dialog accepts the entry and offers to confirm")
    find(driver, "pushButtonOk").click()
    report(wait_until(lambda: not exists(driver, "UiNewStorageDialog")),
           "the dialog closes after confirming")

    report(any(VAULT_NAME in (node.text or "") for node in find_all(driver, "lblStorageTitel")),
           "the entry in the overview carries the name of the vault")

    # A refused password is the shortest way to a message, and what matters
    # about it is where it lands. It used to raise a window the user had to
    # dismiss; it belongs in the status bar.
    announcements.clear()
    find(driver, "leStoragePassword").send_keys(password[::-1])
    find(driver, "btnOpenStorage").click()
    report(wait_until(lambda: bool(announcements), seconds=10),
           "a refused password is announced to whoever cannot see the bar")
    report(not driver.find_elements(AppiumBy.XPATH, "//dialog"),
           "no window opened to carry the refusal")
    report(find(driver, "pageStorages").is_displayed(),
           "the first page still stands after the refusal")

    find(driver, "leStoragePassword").clear()
    find(driver, "leStoragePassword").send_keys(password)
    find(driver, "btnOpenStorage").click()
    report(wait_until(lambda: find(driver, "pageBanking").is_displayed()),
           "the vault opens and the second page is shown")

    for unlocked in ("appCloseStorageAction", "appSetupAssistantAction",
                     "appResetLayoutAction"):
        report(find(driver, unlocked).is_enabled(),
               f"{unlocked} is available once a vault is open")

    # The fetch needs an account on top of an open vault, and this run has
    # none: it reaches no bank. What it can show is the state a user meets,
    # namely a command that stays where it is and says that it does not grip. A
    # command that vanished instead would tell a reader nothing at all.
    report(exists(driver, "appFetchTransactionsAction"),
           "appFetchTransactionsAction is still offered once a vault is open")
    report(not find(driver, "appFetchTransactionsAction").is_enabled(),
           "appFetchTransactionsAction is not available while no account is chosen")

    take_menu_entry(driver, "appFileMenu", "appCloseStorageAction")
    report(wait_until(lambda: find(driver, "pageStorages").is_displayed()),
           "closing the vault returns to the first page")
    report(not find(driver, "appCloseStorageAction").is_enabled(),
           "appCloseStorageAction is locked again afterwards")


def main():
    binary = os.environ.get("OLBAFLINX_BINARY")
    if not binary or not os.access(binary, os.X_OK):
        raise SystemExit("OLBAFLINX_BINARY does not name an executable")

    listen_for_announcements()
    driver = start_driver(binary)
    try:
        report(True, "the driver reached the application over the interface")
        walk_the_way(driver, a_password())

        # Not a step of the way, but what makes a second run of it worth
        # anything: the language the interface spoke. The steps above named no
        # visible text.
        spoken = driver.find_elements(AppiumBy.XPATH, "//menu_bar/*")[0].get_attribute("name")
        print(f"language-sample: {spoken}", flush=True)
    finally:
        driver.quit()

    print()
    if failures:
        print(f"{len(failures)} of the steps did not hold:", flush=True)
        for failure in failures:
            print(f"  {failure}", flush=True)
        return 1
    print("every step held", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
