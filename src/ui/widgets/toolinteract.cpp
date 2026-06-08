/*
#
# Friction - https://friction.graphics
#
# Copyright (c) Ole-André Rodlie and contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# See 'README.md' for more information.
#
*/

#include "toolinteract.h"

#include "Private/document.h"
#include "Private/esettings.h"
#include "GUI/coloranimatorbutton.h"

#include <QSpinBox>
#include <QLabel>
#include <QWidgetAction>
#include <QMenu>
#include <QHBoxLayout>

using namespace Friction;
using namespace Friction::Ui;

void ToolInteract::setupGizmoStatic(QWidget *parentWidget,
                                    QHBoxLayout *layout,
                                    QObject *context)
{
    const auto button = new QToolButton(parentWidget);
    const auto menu = new QMenu(button);
    const auto doc = Document::sInstance;

    button->setObjectName("ToolBoxGizmo");
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setFocusPolicy(Qt::NoFocus);
    button->setMenu(menu);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(16, 16));

    {
        const auto icon = QIcon::fromTheme("gizmos_off");
        const auto iconChecked = QIcon::fromTheme("gizmos_on");
        const auto act = new QAction(tr("Gizmos"), button);
        const bool visible = doc->getGizmoVisibility(Core::Gizmos::Interact::All);

        const QString gizmosOn = tr("Gizmos in On");
        const QString gizmosOff = tr("Gizmos is Off");

        act->setCheckable(true);
        act->setChecked(visible);
        act->setText(visible ? gizmosOn : gizmosOff);
        act->setIcon(visible ? iconChecked : icon);

        menu->addAction(act);
        button->setDefaultAction(act);

        QObject::connect(act, &QAction::triggered,
                context, [doc] () {
            const auto gizmo = Core::Gizmos::Interact::All;
            doc->setGizmoVisibility(gizmo, !doc->getGizmoVisibility(gizmo));
        });
        QObject::connect(doc, &Document::gizmoVisibilityChanged,
                context, [act, icon, iconChecked, gizmosOn, gizmosOff]
                (Core::Gizmos::Interact i, bool visible) {
            const auto gizmo = Core::Gizmos::Interact::All;
            if (gizmo != i) { return; }
            act->blockSignals(true);
            act->setChecked(visible);
            act->setText(visible ? gizmosOn : gizmosOff);
            act->setIcon(visible ? iconChecked : icon);
            act->blockSignals(false);
        });
    }
    menu->addSeparator();

    {
        const QList<Core::Gizmos::Interact> gizmos = {
            Core::Gizmos::Interact::Position,
            Core::Gizmos::Interact::Rotate,
            Core::Gizmos::Interact::Scale,
            Core::Gizmos::Interact::Shear
        };
        for (const auto &ti : gizmos) {
            const bool visible = doc->getGizmoVisibility(ti);
            QString text;
            switch(ti) {
            case Core::Gizmos::Interact::Position: text = tr("Position"); break;
            case Core::Gizmos::Interact::Rotate:   text = tr("Rotate"); break;
            case Core::Gizmos::Interact::Scale:    text = tr("Scale"); break;
            case Core::Gizmos::Interact::Shear:    text = tr("Shear"); break;
            default: continue;
            }
            const auto act = menu->addAction(text);
            act->setCheckable(true);
            act->setChecked(visible);

            QObject::connect(act, &QAction::triggered,
                    context, [doc, ti] () {
                doc->setGizmoVisibility(ti, !doc->getGizmoVisibility(ti));
            });
            QObject::connect(doc, &Document::gizmoVisibilityChanged,
                    context, [act, ti] (Core::Gizmos::Interact i, bool visible) {
                if (ti != i) { return; }
                act->blockSignals(true);
                act->setChecked(visible);
                act->blockSignals(false);
            });
        }
    }

    layout->addWidget(button);
}

void ToolInteract::setupSnapStatic(QWidget *parentWidget,
                                   QHBoxLayout *layout,
                                   QObject *context)
{
    const auto grid = Document::sInstance->getGrid();
    const auto button = new QToolButton(parentWidget);
    const auto menu = new QMenu(button);

    button->setObjectName("ToolBoxGizmo");
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setFocusPolicy(Qt::NoFocus);
    button->setMenu(menu);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(16, 16));

    {
        const auto icon = QIcon::fromTheme("snap_off");
        const auto iconChecked = QIcon::fromTheme("snap_on");
        const QString snapOn = tr("Snapping is On");
        const QString snapOff = tr("Snapping is Off");

        const auto act = new QAction(button);
        const bool snap = grid->getSettings().snapEnabled;

        act->setCheckable(true);
        act->setChecked(snap);
        act->setText(snap ? snapOn : snapOff);
        act->setIcon(snap ? iconChecked : icon);
        act->setShortcut(QKeySequence("Shift+Tab"));

        QObject::connect(act, &QAction::triggered,
                context, [grid, act, icon, iconChecked, snapOn, snapOff]
                (const bool checked) {
            act->setText(checked ? snapOn : snapOff);
            act->setIcon(checked ? iconChecked : icon);
            grid->setOption(Core::Grid::Option::SnapEnabled, checked, true);
        });
        QObject::connect(grid, &Core::Grid::changed,
                context, [act, icon, iconChecked, snapOn, snapOff]
                (const Core::Grid::Settings &settings) {
            if (settings.snapEnabled == act->isChecked()) { return; }
            act->blockSignals(true);
            act->setChecked(settings.snapEnabled);
            act->setText(settings.snapEnabled ? snapOn : snapOff);
            act->setIcon(settings.snapEnabled ? iconChecked : icon);
            act->blockSignals(false);
        });

        menu->addAction(act);
        button->setDefaultAction(act);
    }
    menu->addSeparator();

    {
        const QList<Core::Grid::Option> snapOpts = {
            Core::Grid::Option::SnapToCanvas,
            Core::Grid::Option::SnapToBoxes,
            Core::Grid::Option::SnapToNodes,
            Core::Grid::Option::SnapToPivots,
            Core::Grid::Option::SnapToGrid,
        };
        for (const auto &option : snapOpts) {
            const auto act = new QAction(button);
            act->setCheckable(true);
            act->setChecked(grid->getOption(option).toBool());
            switch (option) {
            case Core::Grid::Option::SnapToCanvas: act->setText(tr("Snap to Canvas")); break;
            case Core::Grid::Option::SnapToBoxes:  act->setText(tr("Snap to Boxes")); break;
            case Core::Grid::Option::SnapToNodes:  act->setText(tr("Snap to Nodes")); break;
            case Core::Grid::Option::SnapToPivots: act->setText(tr("Snap to Pivots")); break;
            case Core::Grid::Option::SnapToGrid:   act->setText(tr("Snap to Grid (if visible)")); break;
            default: break;
            }
            menu->addAction(act);

            QObject::connect(act, &QAction::triggered,
                    context, [grid, option] (const bool checked) {
                grid->setOption(option, checked, true);
            });
            QObject::connect(grid, &Core::Grid::changed,
                    context, [act, option] (const Core::Grid::Settings &settings) {
                bool checked = false;
                switch (option) {
                case Core::Grid::Option::SnapToCanvas: checked = settings.snapToCanvas; break;
                case Core::Grid::Option::SnapToBoxes:  checked = settings.snapToBoxes; break;
                case Core::Grid::Option::SnapToNodes:  checked = settings.snapToNodes; break;
                case Core::Grid::Option::SnapToPivots: checked = settings.snapToPivots; break;
                case Core::Grid::Option::SnapToGrid:   checked = settings.snapToGrid; break;
                default: return;
                }
                if (checked == act->isChecked()) { return; }
                act->blockSignals(true);
                act->setChecked(checked);
                act->blockSignals(false);
            });
        }
    }
    menu->addSeparator();

    {
        const QList<Core::Grid::Option> anchorOpts = {
            Core::Grid::Option::AnchorPivot,
            Core::Grid::Option::AnchorBounds,
            Core::Grid::Option::AnchorNodes,
        };
        for (const auto &option : anchorOpts) {
            const auto act = new QAction(button);
            act->setCheckable(true);
            act->setChecked(grid->getOption(option).toBool());
            switch (option) {
            case Core::Grid::Option::AnchorPivot:  act->setText(tr("Anchor Pivot")); break;
            case Core::Grid::Option::AnchorBounds: act->setText(tr("Anchor Bounds")); break;
            case Core::Grid::Option::AnchorNodes:  act->setText(tr("Anchor Nodes")); break;
            default: break;
            }
            menu->addAction(act);

            QObject::connect(act, &QAction::triggered,
                    context, [grid, option] (const bool checked) {
                grid->setOption(option, checked, true);
            });
            QObject::connect(grid, &Core::Grid::changed,
                    context, [act, option] (const Core::Grid::Settings &settings) {
                bool checked = false;
                switch (option) {
                case Core::Grid::Option::AnchorPivot:  checked = settings.snapAnchorPivot; break;
                case Core::Grid::Option::AnchorBounds: checked = settings.snapAnchorBounds; break;
                case Core::Grid::Option::AnchorNodes:  checked = settings.snapAnchorNodes; break;
                default: return;
                }
                if (checked == act->isChecked()) { return; }
                act->blockSignals(true);
                act->setChecked(checked);
                act->blockSignals(false);
            });
        }
    }

    layout->addWidget(button);
}

void ToolInteract::setupGridStatic(QWidget *parentWidget,
                                   QHBoxLayout *layout,
                                   QObject *context)
{
    const auto grid = Document::sInstance->getGrid();
    const auto button = new QToolButton(parentWidget);
    const auto menu = new QMenu(button);

    button->setObjectName("ToolBoxGizmo");
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setFocusPolicy(Qt::NoFocus);
    button->setMenu(menu);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(16, 16));

    {
        const auto icon = QIcon::fromTheme("grid_off");
        const auto iconChecked = QIcon::fromTheme("grid_on");
        const QString gridOn = tr("Grid is On");
        const QString gridOff = tr("Grid is Off");
        const auto act = new QAction(tr("Grid"), button);
        const bool gridShow = grid->getSettings().show;

        act->setCheckable(true);
        act->setChecked(gridShow);
        act->setText(gridShow ? gridOn : gridOff);
        act->setIcon(gridShow ? iconChecked : icon);

        menu->addAction(act);
        button->setDefaultAction(act);

        QObject::connect(act, &QAction::triggered,
                context, [grid, act, icon, iconChecked, gridOn, gridOff]
                (const bool checked) {
            act->setText(checked ? gridOn : gridOff);
            act->setIcon(checked ? iconChecked : icon);
            grid->setOption(Core::Grid::Option::Show, checked, true);
        });
        QObject::connect(grid, &Core::Grid::changed,
                context, [act, icon, iconChecked, gridOn, gridOff]
                (const Core::Grid::Settings &settings) {
            if (settings.show == act->isChecked()) { return; }
            act->blockSignals(true);
            act->setChecked(settings.show);
            act->setText(settings.show ? gridOn : gridOff);
            act->setIcon(settings.show ? iconChecked : icon);
            act->blockSignals(false);
        });
    }

    menu->addSeparator();
    const auto optMenu = menu->addMenu(QIcon::fromTheme("preferences"),
                                       tr("Settings"));
    menu->addSeparator();

    {
        const QList<Core::Grid::Option> dimOpts = {
            Core::Grid::Option::SizeX, Core::Grid::Option::SizeY,
            Core::Grid::Option::OriginX, Core::Grid::Option::OriginY,
            Core::Grid::Option::MajorEveryX, Core::Grid::Option::MajorEveryY
        };
        for (const auto &option : dimOpts) {
            const auto act = new QWidgetAction(button);
            const auto wid = new QWidget(button);
            const auto lay = new QHBoxLayout(wid);
            const auto spin = new QSpinBox(wid);
            QString labelText;
            spin->setRange(0, 9999);

            const auto &settings = grid->getSettings();
            switch (option) {
            case Core::Grid::Option::SizeX:       spin->setValue(settings.sizeX); labelText = tr("Size X"); break;
            case Core::Grid::Option::SizeY:       spin->setValue(settings.sizeY); labelText = tr("Size Y"); break;
            case Core::Grid::Option::OriginX:     spin->setValue(settings.originX); labelText = tr("Origin X"); break;
            case Core::Grid::Option::OriginY:     spin->setValue(settings.originY); labelText = tr("Origin Y"); break;
            case Core::Grid::Option::MajorEveryX: spin->setValue(settings.majorEveryX); labelText = tr("Major X"); break;
            case Core::Grid::Option::MajorEveryY: spin->setValue(settings.majorEveryY); labelText = tr("Major Y"); break;
            default: break;
            }
            const auto label = new QLabel(labelText, wid);
            wid->setContentsMargins(0, 0, 0, 0);
            lay->setContentsMargins(5, 2, 5, 2);
            lay->addWidget(label);
            lay->addWidget(spin);
            act->setDefaultWidget(wid);
            menu->addAction(act);

            QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                    context, [grid, option] (const int value) {
                grid->setOption(option, value, false);
            });
            QObject::connect(grid, &Core::Grid::changed,
                    context, [spin, option] (const Core::Grid::Settings &settings) {
                int value = -1;
                switch (option) {
                case Core::Grid::Option::SizeX:       value = settings.sizeX; break;
                case Core::Grid::Option::SizeY:       value = settings.sizeY; break;
                case Core::Grid::Option::OriginX:     value = settings.originX; break;
                case Core::Grid::Option::OriginY:     value = settings.originY; break;
                case Core::Grid::Option::MajorEveryX: value = settings.majorEveryX; break;
                case Core::Grid::Option::MajorEveryY: value = settings.majorEveryY; break;
                default: return;
                }
                if (value == spin->value()) { return; }
                spin->blockSignals(true);
                spin->setValue(value);
                spin->blockSignals(false);
            });
        }
    }

    {
        const auto act = new QWidgetAction(button);
        const auto wid = new QWidget(button);
        const auto lay = new QHBoxLayout(wid);
        const auto spin = new QSpinBox(wid);
        const auto label = new QLabel(tr("Threshold"), wid);
        wid->setContentsMargins(0, 0, 0, 0);
        lay->setMargin(4);
        lay->addWidget(label);
        lay->addWidget(spin);
        act->setDefaultWidget(wid);
        menu->addAction(act);
        spin->setRange(0, 9999);
        spin->setValue(grid->getSettings().snapThresholdPx);

        QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                context, [grid] (const int value) {
            grid->setOption(Core::Grid::Option::SnapThresholdPx, value, true);
        });
        QObject::connect(grid, &Core::Grid::changed,
                context, [spin] (const Core::Grid::Settings &settings) {
            if (settings.snapThresholdPx == spin->value()) { return; }
            spin->blockSignals(true);
            spin->setValue(settings.snapThresholdPx);
            spin->blockSignals(false);
        });
    }

    menu->addSeparator();

    {
        const auto act = new QWidgetAction(button);
        const auto wid = new QWidget(button);
        const auto lay = new QHBoxLayout(wid);
        const auto label = new QLabel(tr("Colors"), wid);
        const auto color1 = new ColorAnimatorButton(grid->getSettings().color, wid);
        const auto color2 = new ColorAnimatorButton(grid->getSettings().colorMajor, wid);
        wid->setContentsMargins(0, 0, 0, 0);
        lay->setContentsMargins(5, 2, 10, 2);
        lay->addWidget(label);
        lay->addWidget(color1);
        lay->addWidget(color2);
        act->setDefaultWidget(wid);
        menu->addAction(act);

        QObject::connect(color1, &ColorAnimatorButton::colorChanged,
                context, [grid] (const QColor &color) {
            grid->setOption(Core::Grid::Option::Color, color, false);
        });
        QObject::connect(color2, &ColorAnimatorButton::colorChanged,
                context, [grid] (const QColor &color) {
            grid->setOption(Core::Grid::Option::ColorMajor, color, false);
        });
        QObject::connect(grid, &Core::Grid::changed,
                context, [color1, color2] (const Core::Grid::Settings &settings) {
            if (settings.color != color1->color()) {
                color1->blockSignals(true);
                color1->setColor(settings.color);
                color1->blockSignals(false);
            }
            if (settings.colorMajor != color2->color()) {
                color2->blockSignals(true);
                color2->setColor(settings.colorMajor);
                color2->blockSignals(false);
            }
        });
    }

    menu->addSeparator();

    {
        const auto act = new QAction(tr("Draw on Top"), button);
        act->setCheckable(true);
        act->setChecked(grid->getSettings().drawOnTop);
        menu->addAction(act);

        QObject::connect(act, &QAction::triggered,
                context, [grid] (const bool checked) {
            grid->setOption(Core::Grid::Option::DrawOnTop, checked, false);
        });
        QObject::connect(grid, &Core::Grid::changed,
                context, [act] (const Core::Grid::Settings &settings) {
            if (settings.drawOnTop == act->isChecked()) { return; }
            act->blockSignals(true);
            act->setChecked(settings.drawOnTop);
            act->blockSignals(false);
        });
    }

    menu->addSeparator();

    {
        const auto act = new QAction(QIcon::fromTheme("loop_back"),
                                     tr("Reset Grid"), button);
        optMenu->addAction(act);
        QObject::connect(act, &QAction::triggered,
                context, [grid] () {
            auto settings = grid->getSettings();
            const auto fallback = Core::Grid::Settings();
            settings.sizeX = fallback.sizeX;
            settings.sizeY = fallback.sizeY;
            settings.originX = fallback.originX;
            settings.originY = fallback.originY;
            settings.snapThresholdPx = fallback.snapThresholdPx;
            settings.majorEveryX = fallback.majorEveryX;
            settings.majorEveryY = fallback.majorEveryY;
            settings.color = fallback.color;
            settings.colorMajor = fallback.colorMajor;
            settings.drawOnTop = fallback.drawOnTop;
            grid->setSettings(settings);
        });
    }
    {
        const auto act = new QAction(QIcon::fromTheme("loop_back"),
                                     tr("Reset Default"), button);
        optMenu->addAction(act);
        QObject::connect(act, &QAction::triggered,
                context, [grid] () {
            auto defaults = eSettings::sInstance->fGrid;
            const auto fallback = Core::Grid::Settings();
            defaults.sizeX = fallback.sizeX;
            defaults.sizeY = fallback.sizeY;
            defaults.originX = fallback.originX;
            defaults.originY = fallback.originY;
            defaults.snapThresholdPx = fallback.snapThresholdPx;
            defaults.majorEveryX = fallback.majorEveryX;
            defaults.majorEveryY = fallback.majorEveryY;
            defaults.color = fallback.color;
            defaults.colorMajor = fallback.colorMajor;
            defaults.drawOnTop = fallback.drawOnTop;
            grid->saveSettings(defaults);
            eSettings::sInstance->fGrid = defaults;
        });
    }
    optMenu->addSeparator();
    {
        const auto act = new QAction(QIcon::fromTheme("file_folder"),
                                     tr("Load from Default"), button);
        optMenu->addAction(act);
        QObject::connect(act, &QAction::triggered,
                context, [grid] () {
            auto settings = grid->getSettings();
            const auto fallback = eSettings::sInstance->fGrid;
            settings.sizeX = fallback.sizeX;
            settings.sizeY = fallback.sizeY;
            settings.originX = fallback.originX;
            settings.originY = fallback.originY;
            settings.snapThresholdPx = fallback.snapThresholdPx;
            settings.majorEveryX = fallback.majorEveryX;
            settings.majorEveryY = fallback.majorEveryY;
            settings.color = fallback.color;
            settings.colorMajor = fallback.colorMajor;
            settings.drawOnTop = fallback.drawOnTop;
            grid->setSettings(settings);
        });
    }
    {
        const auto act = new QAction(QIcon::fromTheme("disk_drive"),
                                     tr("Save as Default"), button);
        optMenu->addAction(act);
        QObject::connect(act, &QAction::triggered,
                context, [grid] () {
            const auto settings = grid->getSettings();
            auto defaults = eSettings::sInstance->fGrid;
            defaults.sizeX = settings.sizeX;
            defaults.sizeY = settings.sizeY;
            defaults.originX = settings.originX;
            defaults.originY = settings.originY;
            defaults.snapThresholdPx = settings.snapThresholdPx;
            defaults.majorEveryX = settings.majorEveryX;
            defaults.majorEveryY = settings.majorEveryY;
            defaults.color = settings.color;
            defaults.colorMajor = settings.colorMajor;
            defaults.drawOnTop = settings.drawOnTop;
            grid->saveSettings(defaults);
            eSettings::sInstance->fGrid = defaults;
        });
    }

    layout->addWidget(button);
}

void ToolInteract::addButtonsToLayout(QHBoxLayout *layout,
                                      QWidget *parentWidget)
{
    setupGizmoStatic(parentWidget, layout, parentWidget);
    setupSnapStatic(parentWidget, layout, parentWidget);
    setupGridStatic(parentWidget, layout, parentWidget);
}

ToolInteract::ToolInteract(QWidget *parent)
    : ToolBar("ToolInteract", parent, true)
{
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setContextMenuPolicy(Qt::NoContextMenu);
    setAllowedAreas(Qt::AllToolBarAreas);
    setFloatable(true);
    setWindowTitle(tr("Tool Interact"));

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *container = new QWidget(this);
    container->setLayout(layout);
    addWidget(container);

    addButtonsToLayout(layout, container);
}
