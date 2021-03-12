/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     gongheng<gongheng@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             gongheng<gongheng@uniontech.com>
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
*/

#ifndef DFMCRUMBPLUGIN_H
#define DFMCRUMBPLUGIN_H

#include <QObject>

#include "dfmglobal.h"

DFM_BEGIN_NAMESPACE
#define DFMCrumbFactoryInterface_iid "com.deepin.filemanager.DFMCrumbFactoryInterface_iid"

class DFMCrumbInterface;
class DFMCrumbPlugin : public QObject
{
    Q_OBJECT
public:
    explicit DFMCrumbPlugin(QObject *parent = 0);
    ~DFMCrumbPlugin();

    virtual DFMCrumbInterface *create(const QString &key) = 0;
};
DFM_END_NAMESPACE

#endif // DFMCRUMBPLUGIN_H