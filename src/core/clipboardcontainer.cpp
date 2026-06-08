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

#include "clipboardcontainer.h"
#include "Boxes/boundingbox.h"
#include "Boxes/containerbox.h"
#include "Properties/boxtargetproperty.h"
#include "PathEffects/patheffectcollection.h"
#include "Animators/SmartPath/smartpathcollection.h"
#include "Properties/boxtargetproperty.h"
#include "RasterEffects/rastereffectcollection.h"
#include "TransformEffects/transformeffectcollection.h"
#include "BlendEffects/blendeffectcollection.h"
#include "BlendEffects/layermaskeffect.h"
#include "Boxes/pathbox.h"
#include "../modules/ae_masks/aemaskmodule.h"
#include "canvas.h"

qsptr<BoundingBox> readIdCreateBox(eReadStream& src);

namespace {

enum class CopiedBlendEffectKind : int {
    normal,
    layerMask
};

void writeMaskPath(PathBox* const maskPath, eWriteStream& writeStream) {
    const bool hasMaskPath = maskPath;
    writeStream << hasMaskPath;
    if(!hasMaskPath) {
        return;
    }
    const auto future = writeStream.planFuturePos();
    maskPath->writeIdentifier(writeStream);
    maskPath->writeBoundingBox(writeStream);
    writeStream.writeCheckpoint();
    writeStream.assignFuturePos(future);
}

qsptr<PathBox> readMaskPath(eReadStream& readStream) {
    bool hasMaskPath;
    readStream >> hasMaskPath;
    if(!hasMaskPath) {
        return nullptr;
    }
    const auto futurePos = readStream.readFuturePos();
    try {
        const auto box = readIdCreateBox(readStream);
        box->readBoundingBox(readStream);
        readStream.readCheckpoint("Error reading layer mask path");
        return qSharedPointerCast<PathBox>(box);
    } catch(const std::exception& e) {
        readStream.seek(futurePos);
        gPrintExceptionCritical(e);
        return nullptr;
    }
}

}

Clipboard::Clipboard(const ClipboardType type) : mType(type) {}

ClipboardType Clipboard::getType() const { return mType; }

void Clipboard::write(const Clipboard::Writer &writer) {
    QBuffer buffer(&mData);
    buffer.open(QIODevice::WriteOnly);
    eWriteStream writeStream(&buffer);
    writer(writeStream);
    writeStream.writeFutureTable();
    buffer.close();
}

void Clipboard::read(const Clipboard::Reader &reader) {
    QBuffer buffer(&mData);
    buffer.open(QIODevice::ReadOnly);
    eReadStream readStream(&buffer);
    buffer.seek(buffer.size() - qint64(sizeof(int)));
    readStream.readFutureTable();
    buffer.seek(0);
    reader(readStream);
    buffer.close();
}

EffectsClipboard::EffectsClipboard(const QList<RasterEffect*> &rasterEffects,
                                   const QList<TransformEffect*> &transformEffects,
                                   const QList<BlendEffect*> &blendEffects) :
    Clipboard(ClipboardType::effects) {
    write([&rasterEffects, &transformEffects, &blendEffects](eWriteStream& writeStream) {
        int nEffects = 0;
        for(const auto effect : rasterEffects) if(effect) nEffects++;
        for(const auto effect : transformEffects) if(effect) nEffects++;
        for(const auto effect : blendEffects) if(effect) nEffects++;
        writeStream << nEffects;
        for(const auto effect : rasterEffects) {
            if(!effect) continue;
            writeStream << static_cast<int>(EffectsType::raster);
            const auto future = writeStream.planFuturePos();
            writeRasterEffectType(effect, writeStream);
            effect->prp_writeProperty(writeStream);
            writeStream.assignFuturePos(future);
        }
        for(const auto effect : transformEffects) {
            if(!effect) continue;
            writeStream << static_cast<int>(EffectsType::transform);
            const auto future = writeStream.planFuturePos();
            writeTransformEffectType(effect, writeStream);
            effect->prp_writeProperty(writeStream);
            writeStream.assignFuturePos(future);
        }
        for(const auto effect : blendEffects) {
            if(!effect) continue;
            writeStream << static_cast<int>(EffectsType::blend);
            const auto future = writeStream.planFuturePos();
            const auto layerMask = enve_cast<LayerMaskEffect*>(effect);
            writeStream << static_cast<int>(layerMask ? CopiedBlendEffectKind::layerMask
                                                      : CopiedBlendEffectKind::normal);
            if(layerMask) {
                writeMaskPath(layerMask->maskPathSource(), writeStream);
            }
            writeBlendEffectType(effect, writeStream);
            effect->prp_writeProperty(writeStream);
            writeStream.assignFuturePos(future);
        }
    });
}

EffectsClipboard::EffectsClipboard(const QList<RasterEffect*> &rasterEffects) :
    EffectsClipboard(rasterEffects, {}, {}) {}

EffectsClipboard::EffectsClipboard(const QList<TransformEffect*> &transformEffects) :
    EffectsClipboard({}, transformEffects, {}) {}

EffectsClipboard::EffectsClipboard(const QList<BlendEffect*> &blendEffects) :
    EffectsClipboard({}, {}, blendEffects) {}

EffectsClipboard::EffectsClipboard(RasterEffect * const rasterEffect) :
    EffectsClipboard(QList<RasterEffect*>() << rasterEffect) {}

EffectsClipboard::EffectsClipboard(TransformEffect * const transformEffect) :
    EffectsClipboard(QList<TransformEffect*>() << transformEffect) {}

EffectsClipboard::EffectsClipboard(BlendEffect * const blendEffect) :
    EffectsClipboard(QList<BlendEffect*>() << blendEffect) {}

bool EffectsClipboard::pasteTo(BoundingBox * const target) {
    if(!target) return false;

    bool pasted = false;
    read([target, &pasted](eReadStream& readStream) {
        int nEffects;
        readStream >> nEffects;
        for(int i = 0; i < nEffects; i++) {
            int typeInt;
            readStream >> typeInt;
            const auto type = static_cast<EffectsType>(typeInt);
            const auto futurePos = readStream.readFuturePos();
            try {
                if(type == EffectsType::raster) {
                    const auto effect = readIdCreateRasterEffect(readStream);
                    effect->prp_readProperty(readStream);
                    target->addRasterEffect(effect);
                } else if(type == EffectsType::transform) {
                    const auto effect = readIdCreateTransformEffect(readStream);
                    effect->prp_readProperty(readStream);
                    target->addTransformEffect(effect);
                } else if(type == EffectsType::blend) {
                    int kindInt;
                    readStream >> kindInt;
                    const auto kind = static_cast<CopiedBlendEffectKind>(kindInt);
                    const auto maskPath = kind == CopiedBlendEffectKind::layerMask ?
                                readMaskPath(readStream) : qsptr<PathBox>();
                    const auto effect = readIdCreateBlendEffect(readStream);
                    effect->prp_readProperty(readStream);
                    if(const auto layerMask = enve_cast<LayerMaskEffect*>(effect.get())) {
                        if(maskPath) {
                            const QString maskName = AeMaskModule::nextMaskName(target, nullptr);
                            maskPath->prp_setName(maskName);
                            AeMaskModule::attachLayerMaskPath(target, maskPath.get());
                            layerMask->setClipPathSource(maskPath.get());
                            target->addBlendEffect(effect);
                            layerMask->syncMaskDisplayName();
                            target->ensureBlendEffectsVisible();
                            target->refreshCanvasControls();
                            target->prp_afterWholeInfluenceRangeChanged();
                            if(auto* const scene = target->getParentScene()) {
                                AeMaskModule::syncSelection(scene, target);
                                scene->requestUpdate();
                            }
                        } else {
                            layerMask->setClipPathSource(nullptr);
                            target->addBlendEffect(effect);
                        }
                    } else {
                        target->addBlendEffect(effect);
                    }
                }
                pasted = true;
            } catch(const std::exception& e) {
                readStream.seek(futurePos);
                gPrintExceptionCritical(e);
            }
        }
    });
    return pasted;
}

BoxesClipboard::BoxesClipboard(const QList<BoundingBox*> &src) :
    Clipboard(ClipboardType::boxes) {
    const auto writer = [&src](eWriteStream& writeStream) {
        const int nBoxes = src.count();
        writeStream << nBoxes;

        const bool isBox = true;
        const int iCount = src.count();
        for(int i = iCount - 1; i >= 0; i--) {
            const auto& iBox = src[i];
            const auto future = writeStream.planFuturePos();
            writeStream << isBox;
            iBox->writeIdentifier(writeStream);
            iBox->writeBoundingBox(writeStream);
            writeStream.writeCheckpoint();
            writeStream.assignFuturePos(future);
        }
    };
    write(writer);
    BoundingBox::sClearWriteBoxes();
}

void BoxesClipboard::pasteTo(ContainerBox* const parent) {
    const int oldCount = parent->getContainedBoxesCount();
    const auto reader = [parent](eReadStream& readStream) {
        try {
            parent->readAllContained(readStream);
        } catch(const std::exception& e) {
            gPrintExceptionCritical(e);
        }
    };
    read(reader);
    const int newCount = parent->getContainedBoxesCount();
    const auto parentScene = parent->getParentScene();
    if(parentScene) {
        const auto& list = parent->getContainedBoxes();
        for(int i = 0; i < newCount - oldCount; i++) {
            const auto& box = list.at(i);
            parentScene->addBoxToSelection(box);
        }
    }
}

KeysClipboard::KeysClipboard() : Clipboard(ClipboardType::keys) {}

void KeysClipboard::paste(const int pasteFrame, const bool merge,
                          const std::function<void(Key*)>& selectAction) {
    QList<Key*> rKeys;
    int firstKeyFrame = FrameRange::EMAX;

    QList<QList<stdsptr<Key>>> animatorKeys;
    for(const auto &animData : mAnimatorData) {
        Animator * const animator = animData.first;
        if(!animator) continue;
        QList<stdsptr<Key>> keys;
        QBuffer buffer(const_cast<QByteArray*>(&animData.second));
        buffer.open(QIODevice::ReadOnly);
        eReadStream readStream(&buffer);
        int nKeys; readStream >> nKeys;
        for(int i = 0; i < nKeys; i++) {
            const auto keyT = animator->anim_createKey();
            keyT->readKey(readStream);
            if(keyT->getAbsFrame() < firstKeyFrame)
                firstKeyFrame = keyT->getAbsFrame();
            keys << keyT;
        }
        buffer.close();

        animatorKeys << keys;
    }
    if(firstKeyFrame == FrameRange::EMAX) return;
    const int dFrame = pasteFrame - firstKeyFrame;

    int keysId = 0;
    for(const auto &animData : mAnimatorData) {
        Animator * const animator = animData.first;
        if(!animator) continue;
        const auto& keys = animatorKeys.at(keysId);
        for(const auto& key : keys) {
            key->setRelFrame(key->getRelFrame() + dFrame);
            animator->anim_appendKeyAction(key);
            rKeys.append(key.get());
            if(selectAction) selectAction(key.get());
        }
        if(merge) animator->anim_mergeKeysIfNeeded();
        keysId++;
    }
}

void KeysClipboard::addTargetAnimator(
        Animator *anim, const QByteArray &keyData) {
    mAnimatorData << AnimatorKeyDataPair(QPointer<Animator>(anim), keyData);
}

PropertyClipboard::PropertyClipboard(const Property* const source) :
    Clipboard(ClipboardType::property),
    mContentType(std::type_index(typeid(*source))) {
    const auto writer = [source](eWriteStream& writeStream) {
        source->prp_writeProperty(writeStream);
    };
    write(writer);
}

bool PropertyClipboard::paste(Property * const target) {
    if(!compatibleTarget(target)) return false;
    const auto reader = [target](eReadStream& readStream) {
        target->prp_readProperty(readStream);
    };
    read(reader);
    return true;
}

BoxClipboard::BoxClipboard(const BoundingBox* const source) :
    Clipboard(ClipboardType::box),
    mContentType(std::type_index(typeid(*source))) {
    const auto writer = [source](eWriteStream& writeStream) {
        source->writeBoundingBox(writeStream);
    };
    write(writer);
}

bool BoxClipboard::paste(BoundingBox * const target) {
    if(!compatibleTarget(target)) return false;
    const auto reader = [target](eReadStream& readStream) {
        target->readBoundingBox(readStream);
    };
    read(reader);
    return true;
}
