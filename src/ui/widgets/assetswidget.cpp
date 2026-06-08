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

#include "assetswidget.h"

#include <QVBoxLayout>
#include <QFileInfo>
#include <QHeaderView>
#include <QDrag>
#include <QMenu>
#include <QLineEdit>
#include <QLocale>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QMimeData>
#include <QSettings>
#include <QSet>
#include <QUrl>
#include <QDataStream>
#include <QHBoxLayout>
#include <QUuid>
#include <QTimer>

#include "appsupport.h"
#include "Private/document.h"
#include "canvas.h"
#include "FileCacheHandlers/videocachehandler.h"
#include "FileCacheHandlers/imagecachehandler.h"
#include "FileCacheHandlers/imagesequencecachehandler.h"
#include "filesourcescache.h"
#include "themesupport.h"
#include "Animators/transformanimator.h"
#include "CacheHandlers/soundcachehandler.h"
#include "FileCacheHandlers/svgfilecachehandler.h"
#include "FileCacheHandlers/animationcachehandler.h"

#include "GUI/global.h"

namespace {

constexpr const char* kSceneMimeType = "application/x-friction-scene";
constexpr int kScenePointerRole = Qt::UserRole + 20;
constexpr int kProjectFolderIdRole = Qt::UserRole + 21;
constexpr int kProjectFolderTypeRole = Qt::UserRole + 22;
constexpr int kProjectFolderRootSceneRole = Qt::UserRole + 23;
constexpr int kProjectFolderKindRole = Qt::UserRole + 24;
constexpr auto kProjectFolderTypeOra = "ora";
constexpr auto kProjectFolderTypeManual = "manual";
constexpr auto kProjectFolderTypeSystem = "system";
constexpr auto kProjectFolderKindOraAssets = "ora-assets";
constexpr auto kProjectFolderKindOraScenes = "ora-scenes";

QString projectFolderTypeOra()
{
    return QString::fromLatin1(kProjectFolderTypeOra);
}

QString projectFolderTypeManual()
{
    return QString::fromLatin1(kProjectFolderTypeManual);
}

QString projectFolderTypeSystem()
{
    return QString::fromLatin1(kProjectFolderTypeSystem);
}

QString oraImportsRootPath()
{
    return QDir(AppSupport::getAppConfigPath()).filePath(QStringLiteral("OraImports"));
}

bool oraAssetGroupDirForPath(const QString &path,
                             QString *groupDir)
{
    const QDir rootDir(oraImportsRootPath());
    const QString relative = rootDir.relativeFilePath(path);
    if (relative.startsWith(QStringLiteral(".."))) {
        return false;
    }
    const QStringList parts = relative.split(QDir::separator(), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    if (groupDir) {
        *groupDir = rootDir.filePath(parts.first());
    }
    return true;
}

QString oraGroupDisplayName(const QString &groupDir)
{
    QSettings meta(QDir(groupDir).filePath(QStringLiteral(".friction_ora_import.ini")),
                   QSettings::IniFormat);
    const QString displayName = meta.value(QStringLiteral("ora/displayName")).toString().trimmed();
    if (!displayName.isEmpty()) {
        return displayName;
    }
    return QFileInfo(groupDir).fileName();
}

QString oraGroupSourcePath(const QString &groupDir)
{
    QSettings meta(QDir(groupDir).filePath(QStringLiteral(".friction_ora_import.ini")),
                   QSettings::IniFormat);
    return meta.value(QStringLiteral("ora/sourcePath")).toString();
}

QTreeWidgetItem *findProjectFolderItemRecursive(QTreeWidgetItem *item,
                                                const QString &folderId)
{
    if (!item) { return nullptr; }
    if (item->data(0, kProjectFolderIdRole).toString() == folderId) {
        return item;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        if (auto *match = findProjectFolderItemRecursive(item->child(i), folderId)) {
            return match;
        }
    }
    return nullptr;
}

QTreeWidgetItem *findProjectFolderItem(QTreeWidget *tree,
                                       const QString &folderId)
{
    if (!tree) { return nullptr; }
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (auto *item = findProjectFolderItemRecursive(tree->topLevelItem(i), folderId)) {
            return item;
        }
    }
    return nullptr;
}

QTreeWidgetItem *ensureProjectFolderItem(QTreeWidget *tree,
                                         const QString &folderId,
                                         const QString &folderName,
                                         const QString &folderType,
                                         const QString &toolTip)
{
    if (!tree || folderId.isEmpty()) { return nullptr; }
    auto *item = findProjectFolderItem(tree, folderId);
    if (!item) {
        item = new QTreeWidgetItem();
        tree->insertTopLevelItem(0, item);
    }
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (folderType == projectFolderTypeOra()) {
        flags |= Qt::ItemIsDragEnabled;
    } else if (folderType == projectFolderTypeManual()) {
        flags |= Qt::ItemIsEditable;
    }
    item->setFlags(flags);
    item->setText(0, folderName);
    item->setIcon(0, QIcon::fromTheme(QStringLiteral("folder")));
    item->setToolTip(0, toolTip);
    item->setData(0, kProjectFolderIdRole, folderId);
    item->setData(0, kProjectFolderTypeRole, folderType);
    item->setExpanded(false);
    return item;
}

QTreeWidgetItem *ensureProjectFolderChildItem(QTreeWidgetItem *parent,
                                              const QString &folderId,
                                              const QString &folderName,
                                              const QString &folderType,
                                              const QString &folderKind = QString(),
                                              const QString &toolTip = QString())
{
    if (!parent || folderId.isEmpty()) { return nullptr; }
    for (int i = 0; i < parent->childCount(); ++i) {
        auto *child = parent->child(i);
        if (child->data(0, kProjectFolderIdRole).toString() == folderId) {
            child->setText(0, folderName);
            child->setToolTip(0, toolTip);
            child->setData(0, kProjectFolderTypeRole, folderType);
            child->setData(0, kProjectFolderKindRole, folderKind);
            return child;
        }
    }

    auto *child = new QTreeWidgetItem();
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (folderType == projectFolderTypeManual()) {
        flags |= Qt::ItemIsEditable;
    }
    child->setFlags(flags);
    child->setText(0, folderName);
    child->setIcon(0, QIcon::fromTheme(QStringLiteral("folder")));
    child->setToolTip(0, toolTip);
    child->setData(0, kProjectFolderIdRole, folderId);
    child->setData(0, kProjectFolderTypeRole, folderType);
    child->setData(0, kProjectFolderKindRole, folderKind);
    child->setExpanded(true);
    parent->addChild(child);
    return child;
}

QList<int> oraSceneIds(const QString &groupDir)
{
    QList<int> ids;
    QSettings meta(QDir(groupDir).filePath(QStringLiteral(".friction_ora_import.ini")),
                   QSettings::IniFormat);
    const auto values = meta.value(QStringLiteral("ora/sceneIds")).toList();
    for (const auto &value : values) {
        ids.append(value.toInt());
    }
    return ids;
}

int oraRootSceneId(const QString &groupDir)
{
    QSettings meta(QDir(groupDir).filePath(QStringLiteral(".friction_ora_import.ini")),
                   QSettings::IniFormat);
    return meta.value(QStringLiteral("ora/rootSceneId"), -1).toInt();
}

QList<qsptr<Canvas>> oraScenesForGroupDir(const QString &groupDir)
{
    QList<qsptr<Canvas>> scenes;
    if (!Document::sInstance || groupDir.isEmpty()) { return scenes; }

    QSet<int> sceneIds;
    for (const int id : oraSceneIds(groupDir)) {
        sceneIds.insert(id);
    }
    for (const auto &scene : Document::sInstance->fScenes) {
        if (scene && sceneIds.contains(scene->getDocumentId())) {
            scenes.append(scene);
        }
    }
    return scenes;
}

QString oraGroupDirForScene(Canvas *scene)
{
    if (!scene) { return QString(); }
    const QDir rootDir(oraImportsRootPath());
    const auto groups = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const int docId = scene->getDocumentId();
    for (const QString &groupName : groups) {
        const QString groupDir = rootDir.filePath(groupName);
        const auto ids = oraSceneIds(groupDir);
        if (ids.contains(docId)) {
            return groupDir;
        }
    }
    return QString();
}

QTreeWidgetItem *oraPackageItemForGroup(QTreeWidget *tree,
                                        const QString &groupDir)
{
    if (!tree || groupDir.isEmpty()) { return nullptr; }
    auto *packageItem = ensureProjectFolderItem(tree,
                                                groupDir,
                                                oraGroupDisplayName(groupDir),
                                                projectFolderTypeOra(),
                                                oraGroupSourcePath(groupDir));
    packageItem->setData(0, Qt::UserRole + 1, groupDir);
    packageItem->setExpanded(false);
    return packageItem;
}

QTreeWidgetItem *oraScenesFolderItem(QTreeWidget *tree,
                                     const QString &groupDir)
{
    auto *packageItem = oraPackageItemForGroup(tree, groupDir);
    if (!packageItem) { return nullptr; }
    return ensureProjectFolderChildItem(packageItem,
                                        groupDir + QStringLiteral("::comps"),
                                        QObject::tr("Compositions"),
                                        projectFolderTypeSystem(),
                                        QString::fromLatin1(kProjectFolderKindOraScenes));
}

QTreeWidgetItem *oraAssetsFolderItem(QTreeWidget *tree,
                                     const QString &groupDir)
{
    auto *packageItem = oraPackageItemForGroup(tree, groupDir);
    if (!packageItem) { return nullptr; }
    return ensureProjectFolderChildItem(packageItem,
                                        groupDir + QStringLiteral("::assets"),
                                        QObject::tr("Assets"),
                                        projectFolderTypeSystem(),
                                        QString::fromLatin1(kProjectFolderKindOraAssets));
}

void updateProjectFolderItemSummary(QTreeWidgetItem *group)
{
    if (!group) { return; }
    const QString type = group->data(0, kProjectFolderTypeRole).toString();
    const QString kind = group->data(0, kProjectFolderKindRole).toString();
    group->setText(1, type == projectFolderTypeOra()
                        ? QObject::tr("ORA Package")
                        : kind == QString::fromLatin1(kProjectFolderKindOraScenes)
                              ? QObject::tr("Compositions")
                              : kind == QString::fromLatin1(kProjectFolderKindOraAssets)
                                    ? QObject::tr("Assets")
                                    : type == projectFolderTypeSystem()
                                          ? QObject::tr("Folder")
                                          : QObject::tr("Folder"));
    group->setText(2, QObject::tr("%1 items").arg(group->childCount()));
    group->setText(3, QString());
}

void collectProjectItemContents(QTreeWidgetItem *item,
                                QList<qsptr<Canvas>> &scenesToRemove,
                                QList<FileCacheHandler*> &cachesToDetach)
{
    if (!item) { return; }
    const quintptr scenePtr = item->data(0, kScenePointerRole).value<quintptr>();
    if (scenePtr != 0) {
        if (auto *scene = reinterpret_cast<Canvas*>(scenePtr)) {
            scenesToRemove.append(scene->ref<Canvas>());
        }
    }
    if (auto *assetItem = dynamic_cast<AssetsWidgetItem*>(item)) {
        if (auto *cache = assetItem->getCache()) {
            cachesToDetach.append(cache);
        }
    }
    for (int i = 0; i < item->childCount(); ++i) {
        collectProjectItemContents(item->child(i), scenesToRemove, cachesToDetach);
    }
}

QTreeWidgetItem *findSceneItemRecursive(QTreeWidgetItem *item,
                                        quintptr scenePtr)
{
    if (!item) { return nullptr; }
    if (item->data(0, kScenePointerRole).value<quintptr>() == scenePtr) {
        return item;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        if (auto *match = findSceneItemRecursive(item->child(i), scenePtr)) {
            return match;
        }
    }
    return nullptr;
}

AssetsWidgetItem *findCacheItemRecursive(QTreeWidgetItem *item,
                                         FileCacheHandler *handler)
{
    if (!item) { return nullptr; }
    if (auto *assetItem = dynamic_cast<AssetsWidgetItem*>(item)) {
        if (assetItem->getCache() == handler) {
            return assetItem;
        }
    }
    for (int i = 0; i < item->childCount(); ++i) {
        if (auto *match = findCacheItemRecursive(item->child(i), handler)) {
            return match;
        }
    }
    return nullptr;
}

void pruneEmptyFolderBranch(QTreeWidget *tree,
                            QTreeWidgetItem *item)
{
    while (item) {
        if (item->childCount() > 0) { return; }
        const QString folderId = item->data(0, kProjectFolderIdRole).toString();
        const QString folderType = item->data(0, kProjectFolderTypeRole).toString();
        if (folderId.isEmpty()) { return; }
        if (folderType == projectFolderTypeManual()) { return; }
        auto *parent = item->parent();
        if (parent) {
            parent->removeChild(item);
            delete item;
            updateProjectFolderItemSummary(parent);
            item = parent;
            continue;
        }
        const int index = tree ? tree->indexOfTopLevelItem(item) : -1;
        if (index >= 0 && tree) {
            delete tree->takeTopLevelItem(index);
        }
        return;
    }
}

bool applyFilterRecursive(QTreeWidgetItem *item,
                          const QString &needle)
{
    if (!item) { return false; }
    bool matchSelf = needle.isEmpty() ||
                     item->text(0).contains(needle, Qt::CaseInsensitive) ||
                     item->text(1).contains(needle, Qt::CaseInsensitive) ||
                     item->toolTip(0).contains(needle, Qt::CaseInsensitive);

    bool matchChild = false;
    for (int i = 0; i < item->childCount(); ++i) {
        matchChild |= applyFilterRecursive(item->child(i), needle);
    }

    const bool visible = matchSelf || matchChild;
    item->setHidden(!visible);
    if (!needle.isEmpty() && matchChild) {
        item->setExpanded(true);
    }
    return visible;
}

}

AssetsTreeWidget::AssetsTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setAcceptDrops(true);
    setItemsExpandable(true);
    setRootIsDecorated(true);
    setSortingEnabled(false);
    setDragEnabled(true);
    setWordWrap(true);
    setFrameShape(QFrame::NoFrame);
    setColumnCount(4);
    setHeaderLabels(QStringList() << tr("Name")
                                  << tr("Type")
                                  << tr("Size")
                                  << tr("Used"));
    setHeaderHidden(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    eSizesUI::widget.add(this, [this](const int size) {
        setIconSize(QSize(size, size));
    });
}

void AssetsTreeWidget::applyFilterText(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < topLevelItemCount(); ++i) {
        applyFilterRecursive(topLevelItem(i), needle);
    }
}

QMimeData *AssetsTreeWidget::mimeData(const QList<QTreeWidgetItem*> items) const
{
    if (items.size() != 1 || !items.constFirst()) {
        return QTreeWidget::mimeData(items);
    }

    auto *item = items.constFirst();
    const QString folderId = item->data(0, kProjectFolderIdRole).toString();
    if (!folderId.isEmpty()) {
        const quintptr rootScenePtr =
            item->data(0, kProjectFolderRootSceneRole).value<quintptr>();
        if (rootScenePtr != 0) {
            auto *mime = new QMimeData();
            QByteArray payload;
            QDataStream stream(&payload, QIODevice::WriteOnly);
            stream << static_cast<quint64>(rootScenePtr);
            mime->setData(kSceneMimeType, payload);
            return mime;
        }

        const QString folderType = item->data(0, kProjectFolderTypeRole).toString();
        if (folderType == projectFolderTypeOra()) {
            const QString sourcePath = oraGroupSourcePath(folderId);
            if (!sourcePath.isEmpty() && QFileInfo::exists(sourcePath)) {
                auto *mime = new QMimeData();
                mime->setUrls({QUrl::fromLocalFile(sourcePath)});
                return mime;
            }
        }
    }

    const quintptr scenePtr = item->data(0, kScenePointerRole).value<quintptr>();
    if (scenePtr != 0) {
        auto *mime = new QMimeData();
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << static_cast<quint64>(scenePtr);
        mime->setData(kSceneMimeType, payload);
        return mime;
    }

    const QString oraGroupDir = item->data(0, Qt::UserRole + 1).toString();
    if (oraGroupDir.isEmpty()) {
        return QTreeWidget::mimeData(items);
    }

    const QString sourcePath = oraGroupSourcePath(oraGroupDir);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        return QTreeWidget::mimeData(items);
    }

    auto *mime = new QMimeData();
    mime->setUrls({QUrl::fromLocalFile(sourcePath)});
    return mime;
}

void AssetsTreeWidget::addAssets(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            const QString urlStr = url.toLocalFile();
            const QFileInfo fInfo(urlStr);
            const QString ext = fInfo.suffix();
            const auto filesHandler = FilesHandler::sInstance;
            if (fInfo.isDir()) {
                filesHandler->getFileHandler<ImageSequenceFileHandler>(urlStr);
            } else if (isSoundExt(ext)) {
                filesHandler->getFileHandler<SoundFileHandler>(urlStr);
            } else if (isImageExt(ext) || isLayersExt(ext)) {
                filesHandler->getFileHandler<ImageFileHandler>(urlStr);
            } else if (isVideoExt(ext)) {
                filesHandler->getFileHandler<VideoFileHandler>(urlStr);
            }
        }
    }
    Document::sInstance->actionFinished();
}

void AssetsTreeWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urlList = event->mimeData()->urls();
        for (const QUrl &url : urlList) {
            if (url.isLocalFile()) {
                const QString urlStr = url.toLocalFile();
                const QFileInfo fInfo(urlStr);
                const QString ext = fInfo.suffix();
                const auto filesHandler = FilesHandler::sInstance;
                if (fInfo.isDir()) {
                    filesHandler->getFileHandler<ImageSequenceFileHandler>(urlStr);
                } else if (isSoundExt(ext)) {
                    filesHandler->getFileHandler<SoundFileHandler>(urlStr);
                } else if (isImageExt(ext) || isLayersExt(ext)) {
                    filesHandler->getFileHandler<ImageFileHandler>(urlStr);
                } else if (isVideoExt(ext)) {
                    filesHandler->getFileHandler<VideoFileHandler>(urlStr);
                }
            }
        }
        event->acceptProposedAction();
    }
    Document::sInstance->actionFinished();
}

void AssetsTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat(kSceneMimeType)) {
        event->acceptProposedAction();
    }
}

void AssetsTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

AssetsWidgetItem::AssetsWidgetItem(QTreeWidget *parent,
                                   FileCacheHandler *cache)
    : QTreeWidgetItem(parent)
    , mCache(cache)
{
    if (mCache) {
        setCache(mCache->path(), mCache->fileMissing());
        QObject::connect(mCache, &FileCacheHandler::pathChanged,
                         mCache, [this](const QString &newPath) {
            setCache(newPath, mCache->fileMissing());
        });
    }
}

AssetsWidgetItem::AssetsWidgetItem(QTreeWidgetItem *parent,
                                   FileCacheHandler *cache)
    : QTreeWidgetItem(parent)
    , mCache(cache)
{
    if (mCache) {
        setCache(mCache->path(), mCache->fileMissing());
        QObject::connect(mCache, &FileCacheHandler::pathChanged,
                         mCache, [this](const QString &newPath) {
            setCache(newPath, mCache->fileMissing());
        });
    }
}

FileCacheHandler *AssetsWidgetItem::getCache()
{
    return mCache;
}

void AssetsWidgetItem::setCache(const QString &path,
                                bool missing)
{
    auto fileType = [](const QString &ext) {
        if (isVideoExt(ext)) { return QObject::tr("Video"); }
        if (isSoundExt(ext)) { return QObject::tr("Audio"); }
        if (isImageExt(ext)) { return QObject::tr("Image"); }
        if (isLayersExt(ext)) { return QObject::tr("Layered"); }
        if (isEvExt(ext)) { return QObject::tr("Project"); }
        return QObject::tr("Media");
    };

    QFileInfo info(path);
    setData(0, Qt::UserRole, path);
    setText(0, QString("%1.%2").arg(info.baseName(),
                                    info.completeSuffix()));
    setText(1, fileType(info.suffix()));
    setText(2, info.exists() && info.isFile()
                   ? QLocale().formattedDataSize(info.size())
                   : QString());
    setText(3, mCache ? QString::number(mCache->refCount()) : QStringLiteral("0"));
    setIcon(0, missing ? QIcon::fromTheme("error") : QIcon::fromTheme(AppSupport::getFileIcon(path)));
    setToolTip(0, missing ? QObject::tr("Media offline") : path);
}

AssetsWidget::AssetsWidget(QWidget *parent)
    : QWidget(parent)
    , mTree(nullptr)
{
    setAcceptDrops(true);
    setPalette(ThemeSupport::getDarkPalette());
    setAutoFillBackground(true);
    setContentsMargins(0, 10, 0, 0);
    QVBoxLayout *mLayout = new QVBoxLayout(this);
    mLayout->setMargin(0);
    mLayout->setSpacing(4);

    mPreviewBar = new QFrame(this);
    mPreviewBar->setFrameShape(QFrame::StyledPanel);
    mPreviewBar->setFixedHeight(64);
    mPreviewBar->setVisible(false);
    auto *previewLayout = new QHBoxLayout(mPreviewBar);
    previewLayout->setContentsMargins(6, 4, 6, 4);
    previewLayout->setSpacing(8);

    mPreviewThumb = new QLabel();
    mPreviewThumb->setFixedSize(48, 48);
    mPreviewThumb->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(mPreviewThumb);

    auto *previewTextLayout = new QVBoxLayout();
    previewTextLayout->setSpacing(2);
    mPreviewName = new QLabel();
    QFont boldFont = mPreviewName->font();
    boldFont.setBold(true);
    mPreviewName->setFont(boldFont);
    mPreviewName->setWordWrap(true);
    previewTextLayout->addWidget(mPreviewName);

    mPreviewInfo = new QLabel();
    mPreviewInfo->setWordWrap(true);
    previewTextLayout->addWidget(mPreviewInfo);

    previewLayout->addLayout(previewTextLayout, 1);
    mLayout->addWidget(mPreviewBar);

    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);

    mSearch = new QLineEdit(this);
    mSearch->setObjectName(QStringLiteral("SearchLine"));
    mSearch->setPlaceholderText(tr("Search Project"));
    mSearch->setClearButtonEnabled(true);
    topRow->addWidget(mSearch, 1);

    mNewFolderButton = new QPushButton(tr("New Folder"), this);
    mNewFolderButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    topRow->addWidget(mNewFolderButton);

    mLayout->addLayout(topRow);

    mTree = new AssetsTreeWidget(this);
    mLayout->addWidget(mTree);

    connect(mSearch, &QLineEdit::textChanged,
            mTree, &AssetsTreeWidget::applyFilterText);
    connect(mNewFolderButton, &QPushButton::clicked,
            this, &AssetsWidget::createFolder);

    connect(mTree, &QTreeWidget::customContextMenuRequested,
            this, &AssetsWidget::showContextMenu);
    connect(mTree, &QTreeWidget::currentItemChanged,
            this, &AssetsWidget::updatePreview);
    connect(mTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int) {
        if (!item) { return; }
        const quintptr scenePtr = item->data(0, kScenePointerRole).value<quintptr>();
        if (scenePtr == 0) { return; }
        if (auto *scene = reinterpret_cast<Canvas*>(scenePtr)) {
            emit sceneOpenRequested(scene);
        }
    });

    connect(FilesHandler::sInstance, &FilesHandler::addedCacheHandler,
            this, &AssetsWidget::addCacheHandler);
    connect(FilesHandler::sInstance, &FilesHandler::removedCacheHandler,
            this, &AssetsWidget::removeCacheHandler);

    if (Document::sInstance) {
        for (const auto &scene : Document::sInstance->fScenes) {
            addScene(scene.get());
        }
        connect(Document::sInstance, &Document::sceneCreated,
                this, &AssetsWidget::addScene);
        connect(Document::sInstance, qOverload<Canvas*>(&Document::sceneRemoved),
                this, &AssetsWidget::removeScene);
    }
}

// Forward declarations
static QTreeWidgetItem *ensureFootageFolder(QTreeWidget *tree);
static QTreeWidgetItem *ensureCompositionsFolder(QTreeWidget *tree);

void AssetsWidget::updatePreview(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (!current) {
        mPreviewBar->setVisible(false);
        return;
    }

    mPreviewBar->setVisible(true);
    const QString name = current->text(0);
    mPreviewName->setText(name);

    QStringList infoParts;
    const QString type = current->text(1);
    const QString size = current->text(2);
    if (!type.isEmpty()) infoParts << type;
    if (!size.isEmpty()) infoParts << size;

    const auto assetItem = dynamic_cast<AssetsWidgetItem*>(current);
    if (assetItem && assetItem->getCache()) {
        auto *cache = assetItem->getCache();
        const auto imgSeqHandler = dynamic_cast<ImageSequenceFileHandler*>(cache);
        if (imgSeqHandler) infoParts << tr("Image Sequence");
        const auto svgHandler = dynamic_cast<SvgFileCacheHandler*>(cache);
        if (svgHandler) infoParts << tr("Vector Graphic");
        const auto soundHandler = dynamic_cast<SoundFileHandler*>(cache);
        if (soundHandler) infoParts << tr("Audio");
    }

    const quintptr scenePtr = current->data(0, kScenePointerRole).value<quintptr>();
    if (scenePtr != 0) {
        auto *scene = reinterpret_cast<Canvas*>(scenePtr);
        if (scene) {
            const int w = scene->getCanvasWidth();
            const int h = scene->getCanvasHeight();
            if (w > 0) infoParts << QStringLiteral("%1 × %2 px").arg(w).arg(h);
        }
    }

    const QString folderType = current->data(0, kProjectFolderTypeRole).toString();
    if (!folderType.isEmpty()) {
        const int childCount = current->childCount();
        if (childCount > 0) infoParts << tr("%n item(s)", nullptr, childCount);
    }

    mPreviewInfo->setText(infoParts.join(QStringLiteral("  ·  ")));

    const QIcon icon = current->icon(0);
    if (!icon.isNull()) {
        mPreviewThumb->setPixmap(icon.pixmap(48, 48));
    } else {
        mPreviewThumb->clear();
    }
}

void AssetsWidget::addScene(Canvas *scene)
{
    if (!scene || !mTree) { return; }
    const quintptr scenePtr = reinterpret_cast<quintptr>(scene);
    const QString oraGroupDir = oraGroupDirForScene(scene);
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        auto *existingItem = findSceneItemRecursive(mTree->topLevelItem(i), scenePtr);
        if (!existingItem) { continue; }
        if (!oraGroupDir.isEmpty()) {
            auto *packageItem = oraPackageItemForGroup(mTree, oraGroupDir);
            auto *folderItem = oraScenesFolderItem(mTree, oraGroupDir);
            if (folderItem && existingItem->parent() != folderItem) {
                auto *oldParent = existingItem->parent();
                if (oldParent) {
                    oldParent->takeChild(oldParent->indexOfChild(existingItem));
                } else {
                    const int topIndex = mTree->indexOfTopLevelItem(existingItem);
                    if (topIndex >= 0) { mTree->takeTopLevelItem(topIndex); }
                }
                folderItem->insertChild(0, existingItem);
                updateProjectFolderItemSummary(folderItem);
                if (oldParent) {
                    updateProjectFolderItemSummary(oldParent);
                    pruneEmptyFolderBranch(mTree, oldParent);
                }
                updateProjectFolderItemSummary(packageItem);
                packageItem->setExpanded(true);
            }
            if (packageItem && scene->getDocumentId() == oraRootSceneId(oraGroupDir)) {
                packageItem->setData(0, kProjectFolderRootSceneRole,
                                    QVariant::fromValue<quintptr>(scenePtr));
            }
        }
        return;
    }

    auto *item = new QTreeWidgetItem();
    item->setText(0, scene->prp_getName());
    item->setText(1, tr("Composition"));
    item->setIcon(0, QIcon::fromTheme(QStringLiteral("sequence")));
    item->setData(0, kScenePointerRole, QVariant::fromValue<quintptr>(
                      reinterpret_cast<quintptr>(scene)));
    item->setFlags(item->flags() | Qt::ItemIsEnabled |
                   Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);

    if (!oraGroupDir.isEmpty()) {
        auto *packageItem = oraPackageItemForGroup(mTree, oraGroupDir);
        auto *folderItem = oraScenesFolderItem(mTree, oraGroupDir);
        if (!folderItem) {
            delete item;
            return;
        }
        if (packageItem && scene->getDocumentId() == oraRootSceneId(oraGroupDir)) {
            packageItem->setData(0, kProjectFolderRootSceneRole,
                                QVariant::fromValue<quintptr>(
                                    reinterpret_cast<quintptr>(scene)));
        }
        folderItem->insertChild(0, item);
        updateProjectFolderItemSummary(folderItem);
        updateProjectFolderItemSummary(packageItem);
        packageItem->setExpanded(true);
    } else {
        auto *compsFolder = ensureCompositionsFolder(mTree);
        compsFolder->insertChild(0, item);
        compsFolder->setExpanded(true);
        compsFolder->setText(1, tr("Compositions"));
    }

    connect(scene, &Canvas::prp_nameChanged, this, [item](const QString &name) {
        item->setText(0, name);
    });
}

void AssetsWidget::removeScene(Canvas *scene)
{
    if (!scene || !mTree) { return; }
    const quintptr scenePtr = reinterpret_cast<quintptr>(scene);
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        auto *item = findSceneItemRecursive(mTree->topLevelItem(i), scenePtr);
        if (!item) { continue; }
        auto *parent = item->parent();
        if (!parent) {
            delete mTree->takeTopLevelItem(i);
            return;
        }
        QTreeWidgetItem *cursor = parent;
        while (cursor) {
            if (cursor->data(0, kProjectFolderRootSceneRole).value<quintptr>() == scenePtr) {
                cursor->setData(0, kProjectFolderRootSceneRole, QVariant());
            }
            cursor = cursor->parent();
        }
        parent->removeChild(item);
        delete item;
        updateProjectFolderItemSummary(parent);
        pruneEmptyFolderBranch(mTree, parent);
        return;
    }
}

static QTreeWidgetItem *ensureFootageFolder(QTreeWidget *tree)
{
    if (!tree) { return nullptr; }
    static const QString footageId = QStringLiteral("system:footage");
    return ensureProjectFolderItem(tree,
                                   footageId,
                                   QObject::tr("Footage"),
                                   projectFolderTypeSystem(),
                                   QString());
}

static QTreeWidgetItem *ensureCompositionsFolder(QTreeWidget *tree)
{
    if (!tree) { return nullptr; }
    static const QString compsId = QStringLiteral("system:compositions-root");
    return ensureProjectFolderItem(tree,
                                   compsId,
                                   QObject::tr("Compositions"),
                                   projectFolderTypeSystem(),
                                   QString());
}

void AssetsWidget::addCacheHandler(FileCacheHandler *handler)
{
    if (!handler) { return; }
    mCacheList << handler;
    QString oraGroupDir;
    if (oraAssetGroupDirForPath(handler->path(), &oraGroupDir)) {
        auto *packageItem = oraPackageItemForGroup(mTree, oraGroupDir);
        auto *groupItem = oraAssetsFolderItem(mTree, oraGroupDir);
        new AssetsWidgetItem(groupItem, handler);
        updateProjectFolderItemSummary(groupItem);
        updateProjectFolderItemSummary(packageItem);
        packageItem->setExpanded(true);
        return;
    }

    auto *footageFolder = ensureFootageFolder(mTree);
    new AssetsWidgetItem(footageFolder, handler);
    footageFolder->setExpanded(false);
}

void AssetsWidget::showContextMenu(const QPoint &pos)
{
    auto *treeItem = mTree->itemAt(pos);
    if (!treeItem) {
        QMenu menu(this);
        auto *newFolderAction = menu.addAction(QIcon::fromTheme(QStringLiteral("folder-new")),
                                               tr("New Folder"));
        if (menu.exec(mTree->mapToGlobal(pos)) == newFolderAction) {
            createFolder();
        }
        return;
    }

    auto *assetItem = dynamic_cast<AssetsWidgetItem*>(treeItem);
    const QString folderId = treeItem->data(0, kProjectFolderIdRole).toString();
    const QString folderType = treeItem->data(0, kProjectFolderTypeRole).toString();
    const QString oraGroupDir = treeItem->data(0, Qt::UserRole + 1).toString();
    const quintptr scenePtr = treeItem->data(0, kScenePointerRole).value<quintptr>();
    const bool isSceneItem = scenePtr != 0;
    const bool isFolder = !folderId.isEmpty();
    const bool isOraGroup = isFolder && folderType == projectFolderTypeOra();
    const bool isManualFolder = isFolder && folderType == projectFolderTypeManual();
    const bool isSystemFolder = isFolder && folderType == projectFolderTypeSystem();
    if (!assetItem && !isFolder && !isSceneItem) { return; }

    QMenu menu(this);
    QAction *reloadAction = nullptr;
    QAction *replaceAction = nullptr;
    QAction *openFolderAction = nullptr;
    QAction *copyPathAction = nullptr;
    QAction *renameAction = nullptr;
    QAction *removeAction = nullptr;

    if (assetItem && assetItem->getCache()) {
        reloadAction = menu.addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                      tr("Reload"));
        replaceAction = menu.addAction(QIcon::fromTheme(QStringLiteral("document-open")),
                                       tr("Replace"));
    } else if (isOraGroup) {
        reloadAction = menu.addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                      tr("Reload Group"));
    } else if (isManualFolder) {
        renameAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                                      tr("Rename Folder"));
    }

    if (!isSceneItem && !isSystemFolder) {
        menu.addSeparator();
        openFolderAction = menu.addAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                                          tr("Open Containing Folder"));
        copyPathAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                                        tr("Copy Path"));
    }
    if (!isSystemFolder) {
        menu.addSeparator();
        removeAction = menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")),
                                      isSceneItem ? tr("Delete Composition")
                                                  : isOraGroup ? tr("Remove Package from Project")
                                                  : isManualFolder ? tr("Delete Folder")
                                                                   : tr("Remove from Project"));
    } else {
        menu.addSeparator();
        removeAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-clear")),
                                      tr("Clear Folder"));
    }

    const auto selectedAct = menu.exec(mTree->mapToGlobal(pos));
    if (!selectedAct) { return; }

    if (selectedAct == reloadAction) {
        if (assetItem && assetItem->getCache()) {
            assetItem->getCache()->reloadAction();
        } else {
            for (int i = 0; i < treeItem->childCount(); ++i) {
                if (auto *child = dynamic_cast<AssetsWidgetItem*>(treeItem->child(i))) {
                    if (auto *cache = child->getCache()) {
                        cache->reloadAction();
                    }
                }
            }
        }
        Document::sInstance->actionFinished();
        return;
    }

    if (selectedAct == replaceAction && assetItem && assetItem->getCache()) {
        assetItem->getCache()->replace();
        Document::sInstance->actionFinished();
        return;
    }

    if (selectedAct == renameAction && isManualFolder) {
        mTree->editItem(treeItem, 0);
        return;
    }

    if (selectedAct == openFolderAction) {
        const QString path = isFolder ? (isOraGroup ? oraGroupSourcePath(oraGroupDir)
                                                    : QString())
                                        : assetItem->getCache()
                                              ? assetItem->getCache()->path()
                                              : QString();
        openContainingFolder(path);
        return;
    }

    if (selectedAct == copyPathAction) {
        const QString path = isFolder ? (isOraGroup ? oraGroupSourcePath(oraGroupDir)
                                                    : QString())
                                        : assetItem->getCache()
                                              ? assetItem->getCache()->path()
                                              : QString();
        copyPathToClipboard(path);
        return;
    }

    if (selectedAct == removeAction) {
        removeItemFromProject(treeItem);
        Document::sInstance->actionFinished();
    }
}

void AssetsWidget::openContainingFolder(const QString &path) const
{
    if (path.isEmpty()) { return; }
    const QFileInfo info(path);
    const QString folderPath = info.isDir() ? info.absoluteFilePath()
                                            : info.absolutePath();
    if (folderPath.isEmpty()) { return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

void AssetsWidget::copyPathToClipboard(const QString &path) const
{
    if (path.isEmpty()) { return; }
    if (auto *clipboard = QApplication::clipboard()) {
        clipboard->setText(path);
    }
}

void AssetsWidget::removeItemFromProject(QTreeWidgetItem *item)
{
    if (!item) { return; }

    const QString folderId = item->data(0, kProjectFolderIdRole).toString();
    const QString folderType = item->data(0, kProjectFolderTypeRole).toString();
    if (!folderId.isEmpty()) {
        QList<qsptr<Canvas>> scenesToRemove;
        QList<FileCacheHandler*> cachesToDetach;
        collectProjectItemContents(item, scenesToRemove, cachesToDetach);
        if (folderType == projectFolderTypeOra()) {
            for (const auto &scene : oraScenesForGroupDir(folderId)) {
                if (!scenesToRemove.contains(scene)) {
                    scenesToRemove.append(scene);
                }
            }
        }

        while (item->childCount() > 0) {
            delete item->takeChild(0);
        }
        QTreeWidgetItem *parentItem = item->parent();
        if (parentItem) {
            parentItem->takeChild(parentItem->indexOfChild(item));
        } else {
            const int index = mTree->indexOfTopLevelItem(item);
            if (index >= 0) mTree->takeTopLevelItem(index);
        }
        delete item;

        if (FilesHandler::sInstance) {
            for (auto *cache : cachesToDetach) {
                FilesHandler::sInstance->detachFileHandler(cache);
            }
        }
        if (Document::sInstance) {
            for (const auto &scene : scenesToRemove) {
                Document::sInstance->removeScene(scene);
            }
        }
        return;
    }

    const quintptr scenePtr = item->data(0, kScenePointerRole).value<quintptr>();
    if (scenePtr != 0 && Document::sInstance) {
        if (auto *scene = reinterpret_cast<Canvas*>(scenePtr)) {
            Document::sInstance->removeScene(scene->ref<Canvas>());
        }
        return;
    }

    if (!FilesHandler::sInstance) { return; }

    if (auto *assetItem = dynamic_cast<AssetsWidgetItem*>(item)) {
        if (auto *cache = assetItem->getCache()) {
            FilesHandler::sInstance->detachFileHandler(cache);
        }
        return;
    }

    QList<FileCacheHandler*> cachesToDetach;
    for (int i = item->childCount() - 1; i >= 0; --i) {
        if (auto *child = dynamic_cast<AssetsWidgetItem*>(item->child(i))) {
            if (auto *cache = child->getCache()) {
                cachesToDetach.append(cache);
            }
        }
    }

    for (auto *cache : cachesToDetach) {
        FilesHandler::sInstance->detachFileHandler(cache);
    }
}

void AssetsWidget::removeCacheHandler(FileCacheHandler *handler)
{
    if (!handler) { return; }
    for (int i = 0; i < mTree->topLevelItemCount(); i++) {
        QTreeWidgetItem *top = mTree->topLevelItem(i);
        if (auto *item = findCacheItemRecursive(top, handler)) {
            if (!item->parent()) {
                delete mTree->takeTopLevelItem(i);
                i = mTree->topLevelItemCount();
                break;
            }
            auto *parent = item->parent();
            parent->removeChild(item);
            delete item;
            updateProjectFolderItemSummary(parent);
            pruneEmptyFolderBranch(mTree, parent);
            i = mTree->topLevelItemCount();
            break;
        }
    }
    for (int i = 0; i < mCacheList.count(); i++) {
        const auto& abs = mCacheList.at(i);
        if (abs == handler) {
            mCacheList.removeAt(i);
            break;
        }
    }
}

void AssetsWidget::addAssets(const QList<QUrl> &urls)
{
    mTree->addAssets(urls);
}

void AssetsWidget::createFolder()
{
    if (!mTree) { return; }
    const QString folderId = QStringLiteral("manual:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    auto *item = ensureProjectFolderItem(mTree,
                                         folderId,
                                         tr("New Folder"),
                                         projectFolderTypeManual(),
                                         QString());
    updateProjectFolderItemSummary(item);
    mTree->setCurrentItem(item);
    mTree->editItem(item, 0);
}
