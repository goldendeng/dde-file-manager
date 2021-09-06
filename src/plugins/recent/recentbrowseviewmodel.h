/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     yanghao<yanghao@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
 *             hujianzhong<hujianzhong@uniontech.com>
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
#ifndef RECENTBROWSEVIEWMODEL_H
#define RECENTBROWSEVIEWMODEL_H

#include "dfm-base/base/abstractfileinfo.h"
#include "dfm-base/widgets/dfmfileview/fileviewmodel.h"

#include <QAbstractItemView>

DFMBASE_USE_NAMESPACE

class RecentBrowseViewModel : public FileViewModel
{
public:
    explicit RecentBrowseViewModel(QAbstractItemView *parent = nullptr);

    virtual ~RecentBrowseViewModel() override;

    AbstractFileInfoPointer fileInfo(const QModelIndex &index);

    virtual QModelIndex parent(const QModelIndex &child) const override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};

#endif // RECENTBROWSEVIEWMODEL_H