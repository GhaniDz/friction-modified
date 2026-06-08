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

#ifndef MOTIONV2DIALOG_H
#define MOTIONV2DIALOG_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "widgets/labeledslider.h"

class GraphKey;

class MotionV2Dialog : public QWidget
{
    Q_OBJECT
public:
    explicit MotionV2Dialog(QWidget *parent = nullptr);

private slots:
    void applyEaseIn(int value);
    void applyEaseOut(int value);
    void applyEaseInOut(int value);

private:
    LabeledSlider *mEaseInSlider;
    LabeledSlider *mEaseOutSlider;
    LabeledSlider *mEaseInOutSlider;
};

#endif // MOTIONV2DIALOG_H
