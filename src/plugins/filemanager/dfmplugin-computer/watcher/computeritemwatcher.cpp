/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     xushitong<xushitong@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             zhangsheng<zhangsheng@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "computeritemwatcher.h"
#include "controller/computercontroller.h"
#include "utils/computerutils.h"
#include "utils/stashmountsutils.h"
#include "fileentity/appentryfileentity.h"
#include "fileentity/stashedprotocolentryfileentity.h"

#include "dfm-base/dbusservice/global_server_defines.h"
#include "dfm-base/dbusservice/dbus_interface/devicemanagerdbus_interface.h"
#include "dfm-base/utils/devicemanager.h"
#include "dfm-base/file/entry/entryfileinfo.h"
#include "dfm-base/file/local/localfilewatcher.h"
#include "dfm-base/base/schemefactory.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/base/standardpaths.h"
#include "dfm-base/base/device/devicecontroller.h"
#include "dfm-base/dbusservice/global_server_defines.h"

#include <QDebug>
#include <QApplication>
#include <QWindow>

DFMBASE_USE_NAMESPACE
DPCOMPUTER_BEGIN_NAMESPACE
using namespace GlobalServerDefines;

/*!
 * \class ComputerItemWatcher
 * \brief watches the change of computer item
 */
ComputerItemWatcher *ComputerItemWatcher::instance()
{
    static ComputerItemWatcher watcher;
    return &watcher;
}

ComputerItemWatcher::ComputerItemWatcher(QObject *parent)
    : QObject(parent)
{
    initAppWatcher();
    initConn();
}

ComputerItemWatcher::~ComputerItemWatcher()
{
}

ComputerDataList ComputerItemWatcher::items()
{
    ComputerDataList ret;

    ret.append(getUserDirItems());

    // these are all in Disk group
    bool hasInsertNewDisk = false;
    ret.push_back(getGroup(kGroupDisks));
    int diskStartPos = ret.count();

    ret.append(getBlockDeviceItems(hasInsertNewDisk));
    ComputerDataList protocolDevices = getProtocolDeviceItems(hasInsertNewDisk);
    ret.append(protocolDevices);
    ret.append(getStashedProtocolItems(hasInsertNewDisk, protocolDevices));
    ret.append(getAppEntryItems(hasInsertNewDisk));

    std::sort(ret.begin() + diskStartPos, ret.end(), ComputerItemWatcher::typeCompare);

    if (!hasInsertNewDisk)
        ret.pop_back();

    return ret;
}

ComputerDataList ComputerItemWatcher::getInitedItems()
{
    return initedDatas;
}

bool ComputerItemWatcher::typeCompare(const ComputerItemData &a, const ComputerItemData &b)
{
    if (a.info && b.info) {
        if (a.info->order() == b.info->order())   // then sort by name
            return a.info->displayName() < b.info->displayName();
        else
            return a.info->order() < b.info->order();
    }
    return false;
}

void ComputerItemWatcher::initConn()
{
    connect(this, &ComputerItemWatcher::itemRemoved, this, &ComputerItemWatcher::removeSidebarItem);

    connect(appEntryWatcher.data(), &LocalFileWatcher::subfileCreated, this, [this](const QUrl &url) {
        auto appUrl = ComputerUtils::makeAppEntryUrl(url.path());
        if (!appUrl.isValid())
            return;
        this->onDeviceAdded(appUrl, false);
    });
    connect(appEntryWatcher.data(), &LocalFileWatcher::fileDeleted, this, [this](const QUrl &url) {
        auto appUrl = ComputerUtils::makeAppEntryUrl(url.path());
        if (!appUrl.isValid())
            return;
        Q_EMIT this->itemRemoved(appUrl);
    });

    connect(Application::instance(), &Application::genericAttributeChanged, this, &ComputerItemWatcher::onAppAttributeChanged);

    if (DeviceManagerInstance.isServiceDBusRunning())
        initDeviceServiceDBusConn();
    else
        initDeviceServiceConn();

    connect(&DeviceManagerInstance, &DeviceManager::serviceRegistered, this, &ComputerItemWatcher::onDeviceServiceDBusRegistered);
    connect(&DeviceManagerInstance, &DeviceManager::serviceUnregistered, this, &ComputerItemWatcher::onDeviceServiceDBusUnregistered);
}

void ComputerItemWatcher::initDeviceServiceDBusConn()
{
    qInfo() << "connecting DBus signals...";

    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceAdded, this, &ComputerItemWatcher::onBlockDeviceAdded);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceRemoved, this, &ComputerItemWatcher::onBlockDeviceRemoved);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceMounted, this, &ComputerItemWatcher::onBlockDeviceMounted);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceUnmounted, this, &ComputerItemWatcher::onUpdateBlockItem);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceLocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceUnlocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDevicePropertyChanged, this, &ComputerItemWatcher::onDevicePropertyChangedQDBusVar);

    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::ProtocolDeviceMounted, this, &ComputerItemWatcher::onProtocolDeviceMounted);
    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::ProtocolDeviceUnmounted, this, &ComputerItemWatcher::onProtocolDeviceUnmounted);

    connect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::SizeUsedChanged, this, &ComputerItemWatcher::onDeviceSizeChanged);
}

void ComputerItemWatcher::disconnDeviceServiceDBus()
{
    qInfo() << "disconnecting DBus signals...";

    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceAdded, this, &ComputerItemWatcher::onBlockDeviceAdded);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceRemoved, this, &ComputerItemWatcher::onBlockDeviceRemoved);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceMounted, this, &ComputerItemWatcher::onBlockDeviceMounted);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceUnmounted, this, &ComputerItemWatcher::onUpdateBlockItem);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceLocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDeviceUnlocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::BlockDevicePropertyChanged, this, &ComputerItemWatcher::onDevicePropertyChangedQDBusVar);

    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::ProtocolDeviceMounted, this, &ComputerItemWatcher::onProtocolDeviceMounted);
    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::ProtocolDeviceUnmounted, this, &ComputerItemWatcher::onProtocolDeviceUnmounted);

    disconnect(DeviceManagerInstance.getDeviceInterface(), &DeviceManagerInterface::SizeUsedChanged, this, &ComputerItemWatcher::onDeviceSizeChanged);
}

void ComputerItemWatcher::initDeviceServiceConn()
{
    qInfo() << "connecting service signals...";

    DSC_USE_NAMESPACE;
    auto deviceServ = ComputerUtils::deviceServIns();
    deviceServ->startMonitor();

    connect(deviceServ, &DeviceController::blockDevAdded, this, &ComputerItemWatcher::onBlockDeviceAdded);
    connect(deviceServ, &DeviceController::blockDevRemoved, this, &ComputerItemWatcher::onBlockDeviceRemoved);
    connect(deviceServ, &DeviceController::blockDevMounted, this, &ComputerItemWatcher::onBlockDeviceMounted);
    connect(deviceServ, &DeviceController::blockDevUnmounted, this, &ComputerItemWatcher::onUpdateBlockItem);
    connect(deviceServ, &DeviceController::blockDevLocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    connect(deviceServ, &DeviceController::blockDevUnlocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    connect(deviceServ, &DeviceController::blockDevicePropertyChanged, this, &ComputerItemWatcher::onDevicePropertyChangedQVar);

    connect(deviceServ, &DeviceController::protocolDevMounted, this, &ComputerItemWatcher::onProtocolDeviceMounted);
    connect(deviceServ, &DeviceController::protocolDevUnmounted, this, &ComputerItemWatcher::onProtocolDeviceUnmounted);

    connect(deviceServ, &DeviceController::deviceSizeUsedChanged, this, &ComputerItemWatcher::onDeviceSizeChanged);
}

void ComputerItemWatcher::disconnDeviceService()
{
    qInfo() << "disconnecting service signals...";

    DSC_USE_NAMESPACE;
    auto deviceServ = ComputerUtils::deviceServIns();
    deviceServ->stopMonitor();

    disconnect(deviceServ, &DeviceController::blockDevAdded, this, &ComputerItemWatcher::onBlockDeviceAdded);
    disconnect(deviceServ, &DeviceController::blockDevRemoved, this, &ComputerItemWatcher::onBlockDeviceRemoved);
    disconnect(deviceServ, &DeviceController::blockDevMounted, this, &ComputerItemWatcher::onBlockDeviceMounted);
    disconnect(deviceServ, &DeviceController::blockDevUnmounted, this, &ComputerItemWatcher::onUpdateBlockItem);
    disconnect(deviceServ, &DeviceController::blockDevLocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    disconnect(deviceServ, &DeviceController::blockDevUnlocked, this, &ComputerItemWatcher::onUpdateBlockItem);
    disconnect(deviceServ, &DeviceController::blockDevicePropertyChanged, this, &ComputerItemWatcher::onDevicePropertyChangedQVar);

    disconnect(deviceServ, &DeviceController::protocolDevMounted, this, &ComputerItemWatcher::onProtocolDeviceMounted);
    disconnect(deviceServ, &DeviceController::protocolDevUnmounted, this, &ComputerItemWatcher::onProtocolDeviceUnmounted);

    disconnect(deviceServ, &DeviceController::deviceSizeUsedChanged, this, &ComputerItemWatcher::onDeviceSizeChanged);
}

void ComputerItemWatcher::initAppWatcher()
{
    QUrl extensionUrl;
    extensionUrl.setScheme(SchemeTypes::kFile);
    extensionUrl.setPath(StandardPaths::location(StandardPaths::kExtensionsAppEntryPath));
    appEntryWatcher.reset(new LocalFileWatcher(extensionUrl, this));
    appEntryWatcher->startWatcher();
}

ComputerDataList ComputerItemWatcher::getUserDirItems()
{
    ComputerDataList ret;
    bool userDirAdded = false;
    ret.push_back(getGroup(kGroupDirs));

    static const QStringList udirs = { "desktop", "videos", "music", "pictures", "documents", "downloads" };
    for (auto dir : udirs) {
        QUrl url;
        url.setScheme(dfmbase::SchemeTypes::kEntry);
        url.setPath(QString("%1.%2").arg(dir).arg(SuffixInfo::kUserDir));
        //        auto info = InfoFactory::create<EntryFileInfo>(url);
        DFMEntryFileInfoPointer info(new EntryFileInfo(url));
        if (!info->exists()) continue;

        ComputerItemData data;
        data.url = url;
        data.shape = ComputerItemData::kSmallItem;
        data.info = info;
        ret.push_back(data);
        userDirAdded = true;
    }
    if (!userDirAdded)
        ret.pop_back();
    return ret;
}

ComputerDataList ComputerItemWatcher::getBlockDeviceItems(bool &hasNewItem)
{
    ComputerDataList ret;
    QStringList devs;
    if (DeviceManagerInstance.isServiceDBusRunning())
        devs = DeviceManagerInstance.invokeBlockDevicesIdList({});
    else
        devs = ComputerUtils::deviceServIns()->blockDevicesIdList({});

    for (const auto &dev : devs) {
        auto devUrl = ComputerUtils::makeBlockDevUrl(dev);
        //        auto info = InfoFactory::create<EntryFileInfo>(devUrl);
        DFMEntryFileInfoPointer info(new EntryFileInfo(devUrl));
        if (!info->exists())
            continue;

        ComputerItemData data;
        data.url = devUrl;
        data.shape = ComputerItemData::kLargeItem;
        data.info = info;
        ret.push_back(data);
        hasNewItem = true;

        addSidebarItem(info);
    }

    return ret;
}

ComputerDataList ComputerItemWatcher::getProtocolDeviceItems(bool &hasNewItem)
{
    ComputerDataList ret;
    QStringList devs;
    if (DeviceManagerInstance.isServiceDBusRunning())
        devs = DeviceManagerInstance.invokeProtolcolDevicesIdList({});
    else
        devs = ComputerUtils::deviceServIns()->protocolDevicesIdList();

    for (const auto &dev : devs) {
        auto devUrl = ComputerUtils::makeProtocolDevUrl(dev);
        //        auto info = InfoFactory::create<EntryFileInfo>(devUrl);
        DFMEntryFileInfoPointer info(new EntryFileInfo(devUrl));
        if (!info->exists())
            continue;

        ComputerItemData data;
        data.url = devUrl;
        data.shape = ComputerItemData::kLargeItem;
        data.info = info;
        ret.push_back(data);
        hasNewItem = true;

        addSidebarItem(info);
    }

    return ret;
}

ComputerDataList ComputerItemWatcher::getStashedProtocolItems(bool &hasNewItem, const ComputerDataList &protocolDevs)
{
    auto hasProtocolDev = [](const QUrl &url, const ComputerDataList &container) {
        for (auto dev : container) {
            if (dev.url == url)
                return true;
        }
        return false;
    };
    ComputerDataList ret;

    const QMap<QString, QString> &&stashedMounts = StashMountsUtils::stashedMounts();
    for (auto iter = stashedMounts.cbegin(); iter != stashedMounts.cend(); ++iter) {
        QUrl protocolUrl = ComputerUtils::makeProtocolDevUrl(iter.key());
        if (hasProtocolDev(protocolUrl, ret) || hasProtocolDev(protocolUrl, protocolDevs))
            continue;

        QUrl stashedUrl = ComputerUtils::makeStashedProtocolDevUrl(iter.key());
        DFMEntryFileInfoPointer info(new EntryFileInfo(stashedUrl));
        if (!info->exists())
            continue;

        ComputerItemData data;
        data.url = stashedUrl;
        data.shape = ComputerItemData::kLargeItem;
        data.info = info;
        ret.push_back(data);

        addSidebarItem(info);

        hasNewItem = true;
    }

    return ret;
}

ComputerDataList ComputerItemWatcher::getAppEntryItems(bool &hasNewItem)
{
    static const QString appEntryPath = StandardPaths::location(StandardPaths::kExtensionsAppEntryPath);
    QDir appEntryDir(appEntryPath);
    if (!appEntryDir.exists())
        return {};

    ComputerDataList ret;

    auto entries = appEntryDir.entryList(QDir::Files);
    QStringList cmds;   // for de-duplication
    for (auto entry : entries) {
        auto entryUrl = ComputerUtils::makeAppEntryUrl(QString("%1/%2").arg(appEntryPath).arg(entry));
        if (!entryUrl.isValid())
            continue;

        DFMEntryFileInfoPointer info(new EntryFileInfo(entryUrl));
        if (!info->exists()) {
            qInfo() << "the appentry is in extension folder but not exist: " << info->url();
            continue;
        }
        QString cmd = info->extraProperty(ExtraPropertyName::kExecuteCommand).toString();
        if (cmds.contains(cmd))
            continue;
        cmds.append(cmd);

        ComputerItemData data;
        data.url = entryUrl;
        data.shape = ComputerItemData::kLargeItem;
        data.info = info;
        ret.push_back(data);
        hasNewItem = true;
    }

    return ret;
}

/*!
 * \brief ComputerItemWatcher::getGroup, create a group item
 * \param type
 * \return
 */
ComputerItemData ComputerItemWatcher::getGroup(ComputerItemWatcher::GroupType type)
{
    ComputerItemData splitter;
    splitter.shape = ComputerItemData::kSplitterItem;
    switch (type) {
    case kGroupDirs:
        splitter.groupName = tr("My Directories");
        break;
    case kGroupDisks:
        splitter.groupName = tr("Disks");
        break;
    }
    return splitter;
}

void ComputerItemWatcher::addSidebarItem(DFMEntryFileInfoPointer info)
{
    // additem to sidebar
    bool removable = info->extraProperty(DeviceProperty::kRemovable).toBool() || info->suffix() == SuffixInfo::kProtocol;
    if (ComputerUtils::shouldSystemPartitionHide() && info->suffix() == SuffixInfo::kBlock && !removable) return;

    DSB_FM_USE_NAMESPACE;
    SideBar::ItemInfo sbItem;
    sbItem.group = SideBar::DefaultGroup::kDevice;
    sbItem.url = info->url();
    sbItem.flag = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (info->renamable())
        sbItem.flag |= Qt::ItemIsEditable;
    if (info->fileIcon().name().startsWith("media"))
        sbItem.iconName = "media-optical-symbolic";
    else
        sbItem.iconName = info->fileIcon().name() + "-symbolic";
    sbItem.text = info->displayName();
    sbItem.removable = removable;

    sbItem.cdCb = [](quint64 winId, const QUrl &url) { ComputerControllerInstance->onOpenItem(winId, url); };
    sbItem.contextMenuCb = [](quint64 winId, const QUrl &url, const QPoint &) { ComputerControllerInstance->onMenuRequest(winId, url, true); };
    sbItem.renameCb = [](quint64 winId, const QUrl &url, const QString &name) { ComputerControllerInstance->doRename(winId, url, name); };
    sbItem.findMeCb = [](const QUrl &itemUrl, const QUrl &targetUrl) {
        DFMEntryFileInfoPointer info(new EntryFileInfo(itemUrl));   // TODO(xust) BUG HERE: if enter an unmounted device, the mntUrl is invalid.
        auto mntUrl = info->targetUrl();
        return mntUrl.scheme() == targetUrl.scheme() && mntUrl.path() == targetUrl.path();
    };
    ComputerUtils::sbServIns()->addItem(sbItem);
}

void ComputerItemWatcher::removeSidebarItem(const QUrl &url)
{
    ComputerUtils::sbServIns()->removeItem(url);
}

void ComputerItemWatcher::updateSidebarItem(const QUrl &url, const QString &newName, bool editable)
{
    ComputerUtils::sbServIns()->updateItem(url, newName, editable);
}

void ComputerItemWatcher::addDevice(const QString &groupName, const QUrl &url)
{
    addGroup(groupName);
    onDeviceAdded(url, false);
}

void ComputerItemWatcher::removeDevice(const QUrl &url)
{
    Q_EMIT itemRemoved(url);
}

void ComputerItemWatcher::startQueryItems()
{
    // if computer view is not init view, no receiver to receive the signal, cause when cd to computer view, shows empty.
    // on initialize computer view/model, get the cached items in construction.
    initedDatas = items();
    Q_EMIT itemQueryFinished(initedDatas);
}

/*!
 * \brief ComputerItemWatcher::addGroup, add and emit itemAdded signal
 * \param name
 */
void ComputerItemWatcher::addGroup(const QString &name)
{
    ComputerItemData data;
    data.shape = ComputerItemData::kSplitterItem;
    data.groupName = name;
    emit itemAdded(data);
}

void ComputerItemWatcher::onDeviceAdded(const QUrl &devUrl, bool needSidebarItem)
{
    DFMEntryFileInfoPointer info(new EntryFileInfo(devUrl));
    if (!info->exists()) return;

    ComputerItemData data;
    data.url = devUrl;
    data.shape = ComputerItemData::kLargeItem;
    data.info = info;
    Q_EMIT this->itemAdded(data);

    if (info->suffix() == SuffixInfo::kProtocol) {
        QString id = ComputerUtils::getProtocolDevIdByUrl(info->url());
        if (id.startsWith("smb")) {
            StashMountsUtils::stashMount(info->url(), info->displayName());
            Q_EMIT this->itemRemoved(ComputerUtils::makeStashedProtocolDevUrl(id));
        }
    }

    if (needSidebarItem)
        addSidebarItem(info);
}

void ComputerItemWatcher::onDevicePropertyChangedQVar(const QString &id, const QString &propertyName, const QVariant &var)
{
    onDevicePropertyChangedQDBusVar(id, propertyName, QDBusVariant(var));
}

void ComputerItemWatcher::onDevicePropertyChangedQDBusVar(const QString &id, const QString &propertyName, const QDBusVariant &var)
{
    if (id.startsWith(DeviceId::kBlockDeviceIdPrefix)) {
        auto url = ComputerUtils::makeBlockDevUrl(id);
        // if `hintIgnore` changed to TRUE, then remove the display in view, else add it.
        if (propertyName == DeviceProperty::kHintIgnore) {
            if (var.variant().toBool())
                Q_EMIT itemRemoved(url);
            else
                onDeviceAdded(url);
        } else {
            auto &&devUrl = ComputerUtils::makeBlockDevUrl(id);
            if (propertyName == DeviceProperty::kOptical)
                Q_EMIT itemUpdated(devUrl);
            Q_EMIT itemPropertyChanged(devUrl, propertyName, var.variant());
        }
    }
}

void ComputerItemWatcher::onAppAttributeChanged(Application::GenericAttribute ga, const QVariant &value)
{
    if (ga == Application::GenericAttribute::kShowFileSystemTagOnDiskIcon) {
        Q_EMIT hideFileSystemTag(!value.toBool());
    } else if (ga == Application::GenericAttribute::kHiddenSystemPartition) {
        bool hide = value.toBool();
        Q_EMIT hideNativeDisks(hide);
        for (const auto &data : initedDatas) {
            if (data.info && !data.info->extraProperty(DeviceProperty::kRemovable).toBool() && data.info->suffix() == SuffixInfo::kBlock) {
                if (hide)
                    removeSidebarItem(data.url);
                else
                    addSidebarItem(data.info);
            }
        }
    } else if (ga == Application::GenericAttribute::kAlwaysShowOfflineRemoteConnections) {
        if (!value.toBool()) {
            QStringList mounts = StashMountsUtils::stashedMounts().keys();
            for (const auto &mountUrl : mounts) {
                QUrl stashedUrl = ComputerUtils::makeStashedProtocolDevUrl(mountUrl);
                Q_EMIT itemRemoved(stashedUrl);
                removeSidebarItem(stashedUrl);
            }
            StashMountsUtils::clearStashedMounts();
        } else {
            StashMountsUtils::stashMountedMounts();
        }
    }
}

void ComputerItemWatcher::onBlockDeviceAdded(const QString &id)
{
    QUrl url = ComputerUtils::makeBlockDevUrl(id);
    onDeviceAdded(url);
}

void ComputerItemWatcher::onBlockDeviceRemoved(const QString &id)
{
    auto &&devUrl = ComputerUtils::makeBlockDevUrl(id);
    Q_EMIT itemRemoved(devUrl);
}

void ComputerItemWatcher::onUpdateBlockItem(const QString &id)
{
    QUrl &&devUrl = ComputerUtils::makeBlockDevUrl(id);
    Q_EMIT this->itemUpdated(devUrl);
}

void ComputerItemWatcher::onProtocolDeviceMounted(const QString &id, const QString &mntPath)
{
    auto url = ComputerUtils::makeProtocolDevUrl(id);
    this->onDeviceAdded(url);
}

void ComputerItemWatcher::onProtocolDeviceUnmounted(const QString &id)
{
    auto datas = DeviceManagerInstance.isServiceDBusRunning()
            ? DeviceManagerInstance.invokeQueryProtocolDeviceInfo(id)
            : ComputerUtils::deviceServIns()->protocolDeviceInfo(id);
    auto &&devUrl = ComputerUtils::makeProtocolDevUrl(id);
    if (datas.value(GlobalServerDefines::DeviceProperty::kId).toString().isEmpty()) {   // device have been removed
        Q_EMIT this->itemRemoved(devUrl);
        if (id.startsWith("smb"))
            onDeviceAdded(ComputerUtils::makeStashedProtocolDevUrl(id));
    } else {
        Q_EMIT this->itemUpdated(devUrl);
    }
}

void ComputerItemWatcher::onDeviceSizeChanged(const QString &id, qlonglong total, qlonglong free)
{
    QUrl devUrl = id.startsWith(DeviceId::kBlockDeviceIdPrefix) ? ComputerUtils::makeBlockDevUrl(id) : ComputerUtils::makeProtocolDevUrl(id);
    Q_EMIT this->itemSizeChanged(devUrl, total, free);
}

void ComputerItemWatcher::onDeviceServiceDBusRegistered()
{
    qInfo() << "DBus is registered, switch to DBus signals";
    startQueryItems();
    initDeviceServiceDBusConn();
    disconnDeviceService();
}

void ComputerItemWatcher::onDeviceServiceDBusUnregistered()
{
    qInfo() << "DBus is unregistered, switch to service signals";
    initDeviceServiceConn();
    disconnDeviceServiceDBus();
}

void ComputerItemWatcher::onBlockDeviceMounted(const QString &id, const QString &mntPath)
{
    Q_UNUSED(mntPath);
    auto datas = DeviceManagerInstance.isServiceDBusRunning()
            ? DeviceManagerInstance.invokeQueryBlockDeviceInfo(id)
            : ComputerUtils::deviceServIns()->blockDeviceInfo(id);
    auto shellDevId = datas.value(GlobalServerDefines::DeviceProperty::kCryptoBackingDevice).toString();
    onUpdateBlockItem(shellDevId.length() > 1 ? shellDevId : id);
}

DPCOMPUTER_END_NAMESPACE