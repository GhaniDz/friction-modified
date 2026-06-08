/*
#
# Friction - https://friction.graphics
#
# Copyright (c) Ole-André Rodlie and contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
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

// Fork of enve - Copyright (C) 2016-2020 Maurycy Liebner

#include "canvaswrappernode.h"
#include "widgets/scenechooser.h"
#include "widgets/editablecombobox.h"
#include "widgets/toolinteract.h"
#include "Private/document.h"
#include "Private/esettings.h"
#include "mainwindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>
#include <QComboBox>
#include <QTimer>

class CanvasWrapperMenuBar : public StackWrapperMenu {
public:
    CanvasWrapperMenuBar(Document& document, CanvasWindow * const window) :
        mDocument(document), mWindow(window) {
        setObjectName("AeViewerHeader");
        setProperty("aeViewerHeader", true);
        mSceneMenu = new SceneChooser(mDocument, false, this);
        addMenu(mSceneMenu);
        connect(mSceneMenu, &SceneChooser::currentChanged,
                this, &CanvasWrapperMenuBar::activateScene);
        connect(window, &CanvasWindow::currentSceneChanged,
                this, &CanvasWrapperMenuBar::syncCurrentScene);
    }

    void activateScene(Canvas * const scene) {
        if (!scene) {
            syncCurrentScene(nullptr);
            return;
        }
        mCurrentScene = scene;
        mWindow->setCurrentCanvas(scene);
        if (auto *window = MainWindow::sGetInstance()) {
            window->activateSceneWorkspace(scene);
        } else {
            mDocument.setActiveScene(scene);
        }
    }

    void syncCurrentScene(Canvas * const scene) {
        mCurrentScene = scene;
        const QSignalBlocker blocker(mSceneMenu);
        mSceneMenu->setCurrentScene(scene);
    }

    Canvas* getCurrentScene() const { return mCurrentScene; }
private:
    Document& mDocument;
    CanvasWindow* const mWindow;
    SceneChooser * mSceneMenu;
    Canvas * mCurrentScene = nullptr;
    std::map<Canvas*, QAction*> mSceneToAct;
};

class CanvasWrapperFooter : public QWidget {
public:
    explicit CanvasWrapperFooter(CanvasWindow * const window,
                                 QWidget *parent = nullptr)
        : QWidget(parent)
        , mWindow(window)
    {
        setObjectName("AeViewerFooter");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(28);
        const auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 1, 4, 1);
        layout->setSpacing(4);

        const auto label = new QLabel(tr("Preview"), this);
        label->setMinimumWidth(0);
        layout->addWidget(label);

        const auto resolutionLabel = new QLabel(tr("Resolution"), this);
        layout->addWidget(resolutionLabel);

        mResolutionCombo = new EditableComboBox(this);
        mResolutionCombo->addItem(QStringLiteral("Auto"));
        mResolutionCombo->addItem(QStringLiteral("100 %"));
        mResolutionCombo->addItem(QStringLiteral("75 %"));
        mResolutionCombo->addItem(QStringLiteral("50 %"));
        mResolutionCombo->addItem(QStringLiteral("25 %"));
        mResolutionCombo->setFixedHeight(22);
        mResolutionCombo->setMinimumWidth(84);
        mResolutionCombo->setInsertPolicy(QComboBox::NoInsert);
        if (mResolutionCombo->lineEdit()) {
            mResolutionCombo->lineEdit()->setInputMask(QStringLiteral("D00 %"));
        }
        syncResolutionFromScene();
        layout->addWidget(mResolutionCombo);

        mSnapshotButton = new QToolButton(this);
        mSnapshotButton->setText(tr("Snapshot"));
        mSnapshotButton->setToolTip(tr("Capture the current composition viewer frame."));
        mSnapshotButton->setAutoRaise(true);
        mSnapshotButton->setFixedHeight(22);
        layout->addWidget(mSnapshotButton);

        mCropButton = new QToolButton(this);
        mCropButton->setText(tr("Crop"));
        mCropButton->setToolTip(tr("Draw a crop region in the viewer, then confirm to resize the composition."));
        mCropButton->setAutoRaise(true);
        mCropButton->setFixedHeight(22);
        layout->addWidget(mCropButton);

        mRecallButton = new QToolButton(this);
        mRecallButton->setText(tr("Show Snapshot"));
        mRecallButton->setToolTip(tr("Toggle the stored snapshot preview (F5)."));
        mRecallButton->setEnabled(false);
        mRecallButton->setAutoRaise(true);
        mRecallButton->setFixedHeight(22);
        layout->addWidget(mRecallButton);

        mTransparencyGridButton = new QToolButton(this);
        mTransparencyGridButton->setIcon(QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/transparency-grid.svg")));
        mTransparencyGridButton->setToolTip(tr("Toggle transparency grid (checkerboard background)."));
        mTransparencyGridButton->setCheckable(true);
        mTransparencyGridButton->setAutoRaise(true);
        mTransparencyGridButton->setFixedHeight(22);
        mTransparencyGridButton->setIconSize(QSize(18, 18));
        layout->addWidget(mTransparencyGridButton);

        layout->addStretch();

        {
            const auto sep = new QLabel(QStringLiteral("|"), this);
            sep->setStyleSheet(QStringLiteral("color: #555;"));
            layout->addWidget(sep);
        }

        Friction::Ui::ToolInteract::addButtonsToLayout(layout, this);

        const auto hint = new QLabel(tr("F5"), this);
        hint->setToolTip(tr("Toggle snapshot recall"));
        layout->addWidget(hint);

        connect(mSnapshotButton, &QToolButton::clicked,
                this, [this]() {
            if (!mWindow) { return; }
            mSnapshotButton->setEnabled(false);
            mSnapshotButton->setText(tr("Capturing..."));
            QTimer::singleShot(0, this, [this]() {
                const bool captured = mWindow && mWindow->captureSnapshot();
                mSnapshotButton->setEnabled(true);
                mSnapshotButton->setText(tr("Snapshot"));
                if (captured) {
                    if (auto *mw = MainWindow::sGetInstance()) {
                        if (mw->statusBar()) {
                            mw->statusBar()->showMessage(tr("AE: Snapshot captured"), 2000);
                        }
                    }
                }
            });
        });

        connect(mResolutionCombo, &QComboBox::currentTextChanged,
                this, [this](const QString &text) {
            applyResolutionText(text);
        });

        connect(mCropButton, &QToolButton::clicked,
                this, [this]() {
            if (!mWindow) { return; }
            if (mWindow->cropModeActive()) {
                mWindow->cancelCropSelectionMode();
            } else {
                mWindow->startCropSelectionMode();
                if (auto *mw = MainWindow::sGetInstance()) {
                    if (mw->statusBar()) {
                        mw->statusBar()->showMessage(
                            tr("AE: Drag in the viewer to crop. Right click or Esc to cancel."),
                            3000);
                    }
                }
            }
        });

        connect(mRecallButton, &QToolButton::clicked,
                this, [this]() {
            if (mWindow) { mWindow->toggleSnapshotView(); }
        });

        connect(mWindow, &CanvasWindow::snapshotStateChanged,
                this, [this](bool hasSnapshot, bool showingSnapshot) {
            mRecallButton->setEnabled(hasSnapshot);
            mRecallButton->setText(showingSnapshot ?
                                       tr("Hide Snapshot") :
                                       tr("Show Snapshot"));
        });

        connect(mWindow, &CanvasWindow::cropModeChanged,
                this, [this](bool active) {
            mCropButton->setText(active ? tr("Cancel Crop") : tr("Crop"));
        });

        connect(mWindow, &CanvasWindow::currentSceneChanged,
                this, [this](Canvas *) {
            syncResolutionFromScene();
            syncTransparencyGridButton();
        });

        connect(mTransparencyGridButton, &QToolButton::toggled,
                this, [this](bool checked) {
            if (!mWindow) { return; }
            Q_UNUSED(checked)
            mWindow->toggleTransparencyGrid();
        });
    }

private:
    void syncResolutionFromScene()
    {
        if (!mResolutionCombo) {
            return;
        }
        const qreal globalRes = eSettings::sPreviewResolution();
        mResolutionCombo->blockSignals(true);
        if (globalRes < 0.0) {
            mResolutionCombo->setCurrentText(QStringLiteral("Auto"));
        } else {
            mResolutionCombo->setCurrentText(
                QStringLiteral("%1 %").arg(qRound(globalRes * 100.0)));
        }
        mResolutionCombo->blockSignals(false);
    }

    void applyResolutionText(const QString &text)
    {
        if (!mWindow) {
            return;
        }
        qreal resolution = 0.0;
        if (text.trimmed().compare(QStringLiteral("Auto"), Qt::CaseInsensitive) == 0) {
            resolution = -1.0;
        } else {
            QString cleaned = text;
            cleaned.remove('%');
            cleaned = cleaned.simplified();
            bool ok = false;
            const int pct = cleaned.toInt(&ok);
            if (!ok || pct <= 0) { return; }
            resolution = qBound<qreal>(0.01, qreal(pct) / 100.0, 1.0);
        }
        eSettings::setPreviewResolution(resolution);
        mWindow->setResolution(resolution);
        syncResolutionFromScene();

        if (auto *mw = MainWindow::sGetInstance()) {
            if (mw->statusBar()) {
                mw->statusBar()->showMessage(
                    tr("AE: Preview Resolution %1")
                        .arg(resolution < 0.0 ? QStringLiteral("Auto")
                             : QStringLiteral("%1 %").arg(qRound(resolution * 100.0))),
                    2000);
            }
        }
    }

    CanvasWindow * const mWindow;
    QComboBox *mResolutionCombo = nullptr;
    QToolButton *mSnapshotButton = nullptr;
    QToolButton *mCropButton = nullptr;
    QToolButton *mRecallButton = nullptr;
    QToolButton *mTransparencyGridButton = nullptr;

    void syncTransparencyGridButton()
    {
        if (!mTransparencyGridButton || !mWindow) { return; }
        const auto *canvas = mWindow->getCurrentCanvas();
        const bool on = canvas ? canvas->isTransparencyGridOn() : false;
        mTransparencyGridButton->blockSignals(true);
        mTransparencyGridButton->setChecked(on);
        mTransparencyGridButton->blockSignals(false);
    }
};

CanvasWrapperNode::CanvasWrapperNode(Canvas* const scene) :
    WidgetWrapperNode([](Canvas* const scene) {
        return new CanvasWrapperNode(scene);
    }) {
    mCanvasWindow = new CanvasWindow(*Document::sInstance, this);
    mMenu = new CanvasWrapperMenuBar(*Document::sInstance, mCanvasWindow);
    const auto central = new QWidget(this);
    const auto layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mCanvasWindow);
    layout->addWidget(new CanvasWrapperFooter(mCanvasWindow, central));
    setMenuBar(mMenu);
    setCentralWidget(central);
    mMenu->syncCurrentScene(scene);
}

void CanvasWrapperNode::readData(eReadStream &src) {
    mCanvasWindow->readState(src);
    mMenu->syncCurrentScene(mCanvasWindow->getCurrentCanvas());
}

void CanvasWrapperNode::writeData(eWriteStream &dst) {
    mCanvasWindow->writeState(dst);
}

void CanvasWrapperNode::readDataXEV(XevReadBoxesHandler& boxReadHandler,
                                    const QDomElement& ele,
                                    RuntimeIdToWriteId& objListIdConv) {
    Q_UNUSED(objListIdConv)
    mCanvasWindow->readStateXEV(boxReadHandler, ele);
    mMenu->syncCurrentScene(mCanvasWindow->getCurrentCanvas());
}

void CanvasWrapperNode::writeDataXEV(QDomElement& ele, QDomDocument& doc,
                                     RuntimeIdToWriteId& objListIdConv) {
    Q_UNUSED(objListIdConv)
    mCanvasWindow->writeStateXEV(ele, doc);
}
