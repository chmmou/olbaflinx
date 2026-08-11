#!/usr/bin/env python3
"""Walks the way a user takes on a first start, seeing only what the
accessibility interface shows.

The application is started against an empty configuration, a data vault is
created, opened and closed again. Nothing here reaches into the source of the
application: every element is found through the same interface a screen reader
reads, and every step is taken through it.

Elements are addressed by their accessible id, never by their visible name. Qt
fills that id from the object name and hands it over as a path through the
object tree, so the entry that closes a vault arrives as
"QApplication.UiApp.appMenuBar.appFileMenu.appCloseStorageAction". Only the
last part is matched: it is not translated, it does not move when a label is
reworded, and unlike the whole path it survives an element being reparented.
The visible name would fail the first of those, which is why the run reads it
only to report which language it met.

Three things this run sees that a test inside the process cannot: that a user
meets the commands for an open vault disabled, that the entry in the overview
carries the name the vault was given, and that a message about a failure
reaches the status bar without a window opening for it.
"""

import os
import secrets
import string
import subprocess
import sys
import tempfile
import threading
import time

from dogtail.config import config

config.logDebugToFile = False
config.logDebugToStdOut = False
config.searchCutoffCount = 20
config.actionDelay = 0.4
config.typingDelay = 0.05

from dogtail import predicate, tree  # noqa: E402  (config has to precede the import)
from dogtail.tree import SearchError  # noqa: E402

import gi  # noqa: E402

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi  # noqa: E402

APPLICATION_NAME = "OlbaFlinx"
VAULT_NAME = "outside-run"

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


def identifier_of(node):
    try:
        return (node.accessibleId or "").split(".")[-1]
    except Exception:
        return ""


class WithIdentifier(predicate.Predicate):
    def __init__(self, wanted):
        self.wanted = wanted
        self.satisfiedByNode = lambda node: identifier_of(node) == wanted

    def describeSearchResult(self):
        return f"the element known as {self.wanted}"


def find(root, wanted, retry=True):
    return root.findChild(WithIdentifier(wanted), retry=retry)


def exists(root, wanted):
    """Whether the element is there at all.

    A search that finds nothing raises rather than answering, which is right
    where the element is expected and wrong where its absence is the question.
    """
    try:
        return find(root, wanted, retry=False) is not None
    except SearchError:
        return False


def listen_for_announcements():
    """The status bar hands its text to no one; it announces it.

    Asked over the interface, the bar carries an empty name and no text. What
    it does instead is raise an announcement whenever its message changes,
    which is what reaches a screen reader and what this run listens for.
    """
    listener = Atspi.EventListener.new(lambda event: announcements.append(event.any_data))
    listener.register("object:announcement")
    threading.Thread(target=Atspi.event_main, daemon=True).start()
    return listener


def wait_for_application(seconds=30):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            return tree.root.application(APPLICATION_NAME)
        except Exception:
            time.sleep(0.5)
    raise SystemExit(f"{APPLICATION_NAME} never appeared on the accessibility bus")


def wait_until(condition, seconds=20):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.3)
    return False


def start_application(binary):
    """Starts the application against a configuration directory of its own.

    An empty one: the way this run walks is the way of a first start, and a
    setting left behind by an earlier run would make it a different way.
    """
    home = tempfile.mkdtemp(prefix="olbaflinx-outside-")
    environment = dict(os.environ)
    environment["HOME"] = home
    environment["XDG_CONFIG_HOME"] = os.path.join(home, ".config")
    environment["QT_QPA_PLATFORM"] = "xcb"
    return subprocess.Popen([binary], env=environment)


def take_menu_entry(app, menu_identifier, entry_identifier):
    """Takes an entry of the menu bar.

    Pressing the entry alone does nothing while its menu is closed, so the
    title is opened first. That is the way a user goes as well.
    """
    for title in find(app, "appMenuBar").children:
        if entry_identifier in [identifier_of(e) for e in
                                find(app, menu_identifier).children]:
            title.doActionNamed("ShowMenu")
            break
    wait_until(lambda: find(app, menu_identifier).showing, seconds=5)
    find(app, entry_identifier).doActionNamed("Press")


def walk_the_way(app, password):
    # What a first start owes the user before anything exists.
    report(exists(app, "btnNewStorageItem"), "the first page offers the way to a new vault")
    report(bool(find(app, "lblStorageInfoTitle").name),
           "the first page carries a heading that names the application")

    # The commands that need an open vault say so rather than failing when
    # taken. A run inside the process can ask whether an action is disabled;
    # only a run from outside sees that a user meets it disabled.
    for locked in ("appCloseStorageAction", "appSetupAssistantAction",
                   "appFetchTransactionsAction", "appResetLayoutAction"):
        report(not find(app, locked).sensitive,
               f"{locked} is not available while no vault is open")
    for offered in ("appNewStorageAction", "appQuitAction"):
        report(find(app, offered).sensitive, f"{offered} is available on the first page")

    # Creating the vault.
    find(app, "btnNewStorageItem").click()
    report(wait_until(lambda: exists(app, "UiNewStorageDialog")),
           "the dialog for a new vault opens")

    dialog = find(app, "UiNewStorageDialog")
    find(dialog, "lineEditStorageName").text = VAULT_NAME
    find(dialog, "lineEditPassword").text = password
    find(dialog, "lineEditPasswordConfirm").text = password

    confirm = find(dialog, "pushButtonOk")
    report(wait_until(lambda: confirm.sensitive, seconds=5),
           "the dialog accepts the entry and offers to confirm")
    confirm.click()
    report(wait_until(lambda: not exists(app, "UiNewStorageDialog")),
           "the dialog closes after confirming")

    # The entry names the vault the user just named.
    report(any(VAULT_NAME in (node.name or "")
               for node in app.findChildren(WithIdentifier("lblStorageTitel"))),
           "the entry in the overview carries the name of the vault")

    # A refused password is the shortest way to a message, and what matters
    # about it is where it lands. It used to raise a window the user had to
    # dismiss; it belongs in the status bar.
    announcements.clear()
    find(app, "leStoragePassword").text = password[::-1]
    find(app, "btnOpenStorage").click()
    report(wait_until(lambda: bool(announcements), seconds=10),
           "a refused password is announced to whoever cannot see the bar")
    report(not app.findChildren(predicate.GenericPredicate(roleName="dialog")),
           "no window opened to carry that message")
    report(find(app, "pageStorages").showing, "the first page still stands after the refusal")

    # Opening it for real.
    find(app, "leStoragePassword").text = password
    find(app, "btnOpenStorage").click()
    report(wait_until(lambda: find(app, "pageBanking").showing),
           "the vault opens and the second page is shown")

    for unlocked in ("appCloseStorageAction", "appSetupAssistantAction",
                     "appResetLayoutAction"):
        report(find(app, unlocked).sensitive, f"{unlocked} is available once a vault is open")

    # Closing it again, through the menu.
    take_menu_entry(app, "appFileMenu", "appCloseStorageAction")
    report(wait_until(lambda: find(app, "pageStorages").showing),
           "closing the vault returns to the first page")
    report(not find(app, "appCloseStorageAction").sensitive,
           "appCloseStorageAction is locked again afterwards")


def main():
    binary = os.environ.get("OLBAFLINX_BINARY")
    if not binary or not os.access(binary, os.X_OK):
        raise SystemExit("OLBAFLINX_BINARY does not name an executable")

    listen_for_announcements()
    process = start_application(binary)
    try:
        app = wait_for_application()
        report(True, "the application announces itself over the interface")
        walk_the_way(app, a_password())

        # Not a step of the way, but what makes the second run of it worth
        # anything: the language the interface spoke. The steps above named no
        # visible text, so this line is expected to differ between two runs
        # while every step holds in both.
        spoken = find(app, "appMenuBar").children[0].name
        print(f"language-sample: {spoken}", flush=True)
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()

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
