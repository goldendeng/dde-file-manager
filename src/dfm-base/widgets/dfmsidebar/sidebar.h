/*
 * Copyright (C) 2019 Deepin Technology Co., Ltd.
 *
 * Author:     Gary Wang <wzc782970009@gmail.com>
 *
 * Maintainer: Gary Wang <wangzichong@deepin.com>
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

#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "dfm-base/dfm_base_global.h"
#include "dfm-base/base/abstractfileinfo.h"
#include "dfm-base/dfm_base_global.h"

#include <QObject>
#include <QFuture>
#include <QUrl>
#include <QWidget>
#include <QMenu>
#include <QAbstractItemView>

DFMBASE_BEGIN_NAMESPACE
class SideBarView;
class SideBarModel;
class SideBarItem;
class SideBar : public QWidget
{
    Q_OBJECT
public:
    enum GroupName {
        Unknown,
        StandardPath
    };Q_ENUM(GroupName)

    explicit SideBar(QWidget *parent = nullptr);
    virtual ~SideBar() override;
    QAbstractItemView *view(); // return m_sidebarView

    QRect groupGeometry(const QString &groupName);
    void scrollToGroup(const QString &groupName);

    void setCurrentUrl(const QUrl &url);

    int addItem(SideBarItem *item);
    bool removeItem(const QUrl &url, const QString &group);
    int findItem(const SideBarItem *item) const;
    int findItem(const QUrl &url, const QString &group) const;
    int findItem(const QUrl &url, bool fuzzy = false) const;
    int findItem(std::function<bool(const SideBarItem *item)> cb) const; // cb return true to get the index
    int findLastItem(const QString &group, bool sidebarItemOnly = true) const;
    void openItemEditor(int index) const;
    QSet<QString> disableUrlSchemes() const;

    void setContextMenuEnabled(bool enabled);
    void setDisableUrlSchemes(const QSet<QString> &schemes);

    QList<QUrl> savedItemOrder(const QString &groupName) const;
    void saveItemOrder(const QString &groupName) const;

    static QString groupName(GroupName group);
    static GroupName groupFromName(const QString &name);

Q_SIGNALS:
    void disableUrlSchemesChanged();
    void activatedItemUrl(const QUrl &url);
    void activateditemIndex(const QModelIndex &index);

private Q_SLOTS:
    void onItemActivated(const QModelIndex &index);
    void onContextMenuRequested(const QPoint &pos);
    void onRename(const QModelIndex &index,const QString &newName) const ;

private:
    void initUI();
    void initModelData();
    void initConnection();
    void applySidebarColor();
    void updateSeparatorVisibleState();
    void addGroupItems(GroupName groupType);
    void insertItem(int index, SideBarItem *item, const QString &groupName);
    void appendItem(SideBarItem *item, const QString &groupName);
    void appendItemWithOrder(QList<SideBarItem *> &list, const QList<QUrl> &order, const QString &groupName);

    QModelIndex groupModelIndex(const QString &groupName);

    void changeEvent(QEvent *event) override;

    SideBarView *m_sidebarView;
    SideBarModel *m_sidebarModel;
    bool m_contextMenuEnabled = true;
    QList<QUrl> devitems;
    QSet<QString> m_disableUrlSchemes;

#ifdef ENABLE_ASYNCINIT
    QPair<bool,QFuture<void>> m_initDevThread; //初始化initDeviceConnection线程，first为是否强制结束线程
#endif
};

DFMBASE_END_NAMESPACE

#endif // SIDEBAR_H

