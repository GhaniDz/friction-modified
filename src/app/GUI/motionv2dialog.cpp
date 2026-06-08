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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# See 'README.md' for more information.
#
*/

#include "motionv2dialog.h"
#include "mainwindow.h"
#include "GUI/timelinedockwidget.h"
#include "GUI/timelinewidget.h"
#include "GUI/keysview.h"
#include "Animators/qrealanimator.h"
#include "Animators/graphanimator.h"
#include "Animators/graphkey.h"
#include "Private/document.h"

#include <QSlider>
#include <QLabel>

MotionV2Dialog::MotionV2Dialog(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("MotionV2Panel"));
    setWindowTitle(tr("Motion V2"));

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    const auto titleLabel = new QLabel(tr("Motion V2"));
    titleLabel->setAlignment(Qt::AlignCenter);
    {
        QFont f = titleLabel->font();
        f.setBold(true);
        titleLabel->setFont(f);
    }
    layout->addWidget(titleLabel);

    mEaseInSlider = new LabeledSlider(tr("Ease In"));
    mEaseInSlider->slider()->setRange(0, 100);
    mEaseInSlider->slider()->setValue(0);
    connect(mEaseInSlider->slider(), &QSlider::valueChanged,
            this, &MotionV2Dialog::applyEaseIn);
    layout->addLayout(mEaseInSlider);

    mEaseInOutSlider = new LabeledSlider(tr("Ease In Out"));
    mEaseInOutSlider->slider()->setRange(0, 100);
    mEaseInOutSlider->slider()->setValue(0);
    connect(mEaseInOutSlider->slider(), &QSlider::valueChanged,
            this, &MotionV2Dialog::applyEaseInOut);
    layout->addLayout(mEaseInOutSlider);

    mEaseOutSlider = new LabeledSlider(tr("Ease Out"));
    mEaseOutSlider->slider()->setRange(0, 100);
    mEaseOutSlider->slider()->setValue(0);
    connect(mEaseOutSlider->slider(), &QSlider::valueChanged,
            this, &MotionV2Dialog::applyEaseOut);
    layout->addLayout(mEaseOutSlider);

    layout->addStretch();
}

static KeysView *getKeysView()
{
    const auto mw = MainWindow::sGetInstance();
    if (!mw) return nullptr;
    const auto td = mw->getTimeLineWidget();
    if (!td) return nullptr;
    const auto tw = td->currentTimelineWidget();
    if (!tw) return nullptr;
    return tw->getKeysView();
}

void MotionV2Dialog::applyEaseIn(int value)
{
    const qreal amount = qBound(0.0, value / 100.0, 1.0);
    auto keysView = getKeysView();
    if (!keysView) return;

    const auto selectedAnims = keysView->selectedGraphAnimators();
    for (const auto &anim : selectedAnims) {
        const auto qrealAnim = dynamic_cast<QrealAnimator*>(anim);
        if (!qrealAnim) continue;
        const auto &selectedKeys = qrealAnim->anim_getSelectedKeys();

        for (const auto &key : selectedKeys) {
            auto graphKey = dynamic_cast<GraphKey*>(key);
            if (!graphKey) continue;
            auto prevKey = dynamic_cast<GraphKey*>(graphKey->getPrevKey<Key>());
            if (!prevKey) continue;

            const qreal dFrame = graphKey->getRelFrame() - prevKey->getRelFrame();
            if (qFuzzyIsNull(dFrame)) continue;
            const qreal dValue = graphKey->getValueForGraph() - prevKey->getValueForGraph();
            const qreal x = 1.0 - 0.80 * amount;

            graphKey->setCtrlsModeAction(CtrlsMode::corner);
            graphKey->setC0EnabledAction(true);
            graphKey->setC0Frame(prevKey->getRelFrame() + dFrame * x);
            graphKey->setC0Value(prevKey->getValueForGraph() + dValue * 1.0);
        }
    }
    Document::sInstance->actionFinished();
}

void MotionV2Dialog::applyEaseOut(int value)
{
    const qreal amount = qBound(0.0, value / 100.0, 1.0);
    auto keysView = getKeysView();
    if (!keysView) return;

    const auto selectedAnims = keysView->selectedGraphAnimators();
    for (const auto &anim : selectedAnims) {
        const auto qrealAnim = dynamic_cast<QrealAnimator*>(anim);
        if (!qrealAnim) continue;
        const auto &selectedKeys = qrealAnim->anim_getSelectedKeys();

        for (const auto &key : selectedKeys) {
            auto graphKey = dynamic_cast<GraphKey*>(key);
            if (!graphKey) continue;
            auto nextKey = dynamic_cast<GraphKey*>(graphKey->getNextKey<Key>());
            if (!nextKey) continue;

            const qreal dFrame = nextKey->getRelFrame() - graphKey->getRelFrame();
            if (qFuzzyIsNull(dFrame)) continue;
            const qreal dValue = nextKey->getValueForGraph() - graphKey->getValueForGraph();
            const qreal x = 0.80 * amount;

            graphKey->setCtrlsModeAction(CtrlsMode::corner);
            graphKey->setC1EnabledAction(true);
            graphKey->setC1Frame(graphKey->getRelFrame() + dFrame * x);
            graphKey->setC1Value(graphKey->getValueForGraph() + dValue * 0.0);
        }
    }
    Document::sInstance->actionFinished();
}

void MotionV2Dialog::applyEaseInOut(int value)
{
    const qreal amount = qBound(0.0, value / 100.0, 1.0);
    auto keysView = getKeysView();
    if (!keysView) return;

    const auto selectedAnims = keysView->selectedGraphAnimators();
    for (const auto &anim : selectedAnims) {
        const auto qrealAnim = dynamic_cast<QrealAnimator*>(anim);
        if (!qrealAnim) continue;
        const auto &selectedKeys = qrealAnim->anim_getSelectedKeys();

        for (const auto &key : selectedKeys) {
            auto graphKey = dynamic_cast<GraphKey*>(key);
            if (!graphKey) continue;
            auto prevKey = dynamic_cast<GraphKey*>(graphKey->getPrevKey<Key>());
            auto nextKey = dynamic_cast<GraphKey*>(graphKey->getNextKey<Key>());

            if (prevKey) {
                const qreal dFrame = graphKey->getRelFrame() - prevKey->getRelFrame();
                if (!qFuzzyIsNull(dFrame)) {
                    const qreal dValue = graphKey->getValueForGraph() - prevKey->getValueForGraph();
                    const qreal x = 1.0 - 0.80 * amount;
                    graphKey->setC0EnabledAction(true);
                    graphKey->setC0Frame(prevKey->getRelFrame() + dFrame * x);
                    graphKey->setC0Value(prevKey->getValueForGraph() + dValue * 1.0);
                }
            }

            if (nextKey) {
                const qreal dFrame = nextKey->getRelFrame() - graphKey->getRelFrame();
                if (!qFuzzyIsNull(dFrame)) {
                    const qreal dValue = nextKey->getValueForGraph() - graphKey->getValueForGraph();
                    const qreal x = 0.80 * amount;
                    graphKey->setC1EnabledAction(true);
                    graphKey->setC1Frame(graphKey->getRelFrame() + dFrame * x);
                    graphKey->setC1Value(graphKey->getValueForGraph() + dValue * 0.0);
                }
            }

            graphKey->setCtrlsModeAction(CtrlsMode::corner);
        }
    }
    Document::sInstance->actionFinished();
}
