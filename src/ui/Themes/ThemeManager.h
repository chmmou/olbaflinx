//
// Created by chmmou on 24.05.25.
//

#pragma once

#include <QtCore/QObject>

#include <QtWidgets/QApplication>

namespace olbaflinx::ui::themes {

/**
 * Loads style sheets and renders the icons of the user interface.
 *
 * Ownership: the creator owns the instance. The QApplication that is styled is
 * observed only, not owned.
 */
class ThemeManager final : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

    enum class Mode { Light, Dark };
    Q_ENUM(Mode)

    /**
     * Reads every registered theme file and hands the contents to the associated
     * QApplication as one style sheet, replacing what was set before.
     *
     * The themes are concatenated in the order of their names, so where two of
     * them state a rule for the same selector, the one read last wins. A theme
     * whose file cannot be read is logged and left out; the others still apply.
     *
     * If no QApplication instance has been associated with the ThemeManager,
     * this method has no effect.
     */
    void reload() const;

    /**
     * Registers the theme file and sets the style sheet on the application,
     * which must not be null. Several themes can be registered this way; see
     * reload() for the order they take effect in.
     */
    void apply(QApplication *application, const QString &filename) const;

    /**
     * Renders the icon of that name in the variant the mode asks for. An icon
     * that cannot be loaded answers with an empty pixmap.
     */
    static QPixmap pixmap(const QString &name, ThemeManager::Mode mode = ThemeManager::Mode::Light);

private:
    class Private;
    Private *d_ptr = nullptr;

    Q_DISABLE_COPY(ThemeManager)
};

} // namespace olbaflinx::ui::themes
