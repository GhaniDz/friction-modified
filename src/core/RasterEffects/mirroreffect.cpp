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

#include "mirroreffect.h"
#include "gpurendertools.h"
#include "openglrastereffectcaller.h"

#include "Animators/qpointfanimator.h"
#include "Animators/qrealanimator.h"
#include "appsupport.h"
#include "Boxes/boundingbox.h"
#include "canvas.h"

#include <QtMath>

MirrorEffect::MirrorEffect() :
    RasterEffect("mirror",
                 AppSupport::getRasterEffectHardwareSupport("Mirror",
                                                            HardwareSupport::gpuPreffered),
                 true,
                 RasterEffectType::MIRROR)
{
    mReflectionCenter = enve::make_shared<QPointFAnimator>("reflection center");
    mReflectionCenter->setValuesRange(-200, 200);
    mReflectionCenter->setBaseValue(50, 50);
    ca_addChild(mReflectionCenter);

    mReflectionAngle = enve::make_shared<QrealAnimator>(0, -360, 360, 0.1, "reflection angle");
    ca_addChild(mReflectionAngle);
}

class MirrorEffectCaller : public OpenGLRasterEffectCaller {
public:
    MirrorEffectCaller(const HardwareSupport hwSupport,
                       const qreal centerX,
                       const qreal centerY,
                       const qreal angle)
        : OpenGLRasterEffectCaller(sInitialized, sProgramId,
                                   ":/shaders/mirroreffect.frag",
                                   hwSupport)
        , mCenterX(centerX)
        , mCenterY(centerY)
        , mAngle(angle)
    {}

    void processCpu(CpuRenderTools& renderTools,
                    const CpuRenderData& data) override;
protected:
    void iniVars(QGL33 * const gl) const {
        sCenterXU = gl->glGetUniformLocation(sProgramId, "centerX");
        sCenterYU = gl->glGetUniformLocation(sProgramId, "centerY");
        sAngleU = gl->glGetUniformLocation(sProgramId, "angle");
    }

    void setVars(QGL33 * const gl) const {
        gl->glUseProgram(sProgramId);
        gl->glUniform1f(sCenterXU, mCenterX);
        gl->glUniform1f(sCenterYU, mCenterY);
        gl->glUniform1f(sAngleU, mAngle);
    }
private:
    static bool sInitialized;
    static GLuint sProgramId;

    static GLint sCenterXU;
    static GLint sCenterYU;
    static GLint sAngleU;

    const qreal mCenterX;
    const qreal mCenterY;
    const qreal mAngle;
};

bool MirrorEffectCaller::sInitialized = false;
GLuint MirrorEffectCaller::sProgramId = 0;

GLint MirrorEffectCaller::sCenterXU = -1;
GLint MirrorEffectCaller::sCenterYU = -1;
GLint MirrorEffectCaller::sAngleU = -1;

stdsptr<RasterEffectCaller> MirrorEffect::getEffectCaller(
        const qreal relFrame, const qreal resolution,
        const qreal influence, BoxRenderData * const data) const {
    const QPointF center = mReflectionCenter->getEffectiveValue(relFrame);
    const QPointF centered = (center - QPointF(50, 50)) * 0.01 * influence;
    const qreal centerNormX = 0.5 + centered.x();
    const qreal centerNormY = 0.5 + centered.y();
    const qreal angle = mReflectionAngle->getEffectiveValue(relFrame) * influence
                        * M_PI / 180.0;

    return enve::make_shared<MirrorEffectCaller>(
                instanceHwSupport(), centerNormX, centerNormY, angle);
}

void MirrorEffectCaller::processCpu(CpuRenderTools& renderTools,
                                    const CpuRenderData& data) {
    const auto &src = renderTools.fSrcBtmp;
    auto &dst = renderTools.fDstBtmp;

    const int srcW = src.width();
    const int srcH = src.height();

    const int xMin = data.fTexTile.left();
    const int xMax = data.fTexTile.right();
    const int yMin = data.fTexTile.top();
    const int yMax = data.fTexTile.bottom();

    const qreal cosA = qCos(mAngle);
    const qreal sinA = qSin(mAngle);

    const qreal cx = mCenterX * srcW;
    const qreal cy = mCenterY * srcH;

    for (int yi = yMin; yi <= yMax; ++yi) {
        auto *dstPx = static_cast<uchar*>(dst.getAddr(0, yi - yMin));
        for (int xi = xMin; xi <= xMax; ++xi) {
            // signed distance from the pixel to the mirror axis
            const qreal dx = xi - cx;
            const qreal dy = yi - cy;
            const qreal d = dx * (-sinA) + dy * cosA; // dot with normal

            int sx, sy;
            if (d >= 0) {
                // positive side: keep original
                sx = xi;
                sy = yi;
            } else {
                // negative side: reflect to the positive side
                // reflection: p' = p - 2 * d * normal
                // normal = (-sinA, cosA)
                const qreal rx = xi - 2 * d * (-sinA);
                const qreal ry = yi - 2 * d * cosA;
                sx = qRound(rx);
                sy = qRound(ry);
            }

            // clamp to source bounds
            sx = qBound(0, sx, srcW - 1);
            sy = qBound(0, sy, srcH - 1);

            const auto *srcPx = static_cast<const uchar*>(
                        src.getAddr(sx, sy));
            for (int i = 0; i < 4; ++i) {
                *dstPx++ = srcPx[i];
            }
        }
    }
}
