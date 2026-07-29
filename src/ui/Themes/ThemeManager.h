//
// Created by chmmou on 24.05.25.
//

#pragma once

#include <QtCore/QObject>

#include <QtWidgets/QApplication>

namespace olbaflinx::ui::themes {

/**
 * @brief Loads style sheets and renders the icons of the user interface.
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
     * Reloads the currently registered themes and re-applies them to the associated
     * QApplication instance. This method clears the application's existing stylesheet
     * and loads the styles from the registered theme files.
     *
     * This function ensures all registered themes are reloaded and applied in sequential
     * order without duplicating or bypassing any theme.
     *
     * If no QApplication instance has been associated with the ThemeManager,
     * this method has no effect.
     */
    void reload() const;

    /**
     * Applies a specific theme to the provided QApplication instance by loading the styles
     * from the specified file. The method ensures a theme is registered,
     * sets the application styles accordingly, and allows multiple themes to be managed.
     *
     * @param application The QApplication instance to which the theme should be applied.
     *                     This must be non-null and should represent the main application instance.
     * @param filename The path to the theme file to be applied.
     *                 The file must contain valid stylesheet data.
     */
    void apply(QApplication *application, const QString &filename) const;

    /**
     * Retrieves a QPixmap object corresponding to the specified icon name
     * and mode (Light or Dark theme). This method loads the icon from a
     * predefined resource path, processes the SVG if necessary, and
     * returns a rendered pixmap.
     *
     * @param name The name of the icon resource to load.
     * @param mode The theme mode (Light or Dark) for selecting the icon variant.
     *             Defaults to Light mode if not specified.
     * @return A QPixmap representing the requested icon. Returns an empty
     *         QPixmap if the icon cannot be loaded.
     */
    static QPixmap pixmap(const QString &name, ThemeManager::Mode mode = ThemeManager::Mode::Light);

private:
    class Private;
    Private *d_ptr;

    Q_DISABLE_COPY(ThemeManager)
};

} // namespace olbaflinx::ui::themes
