/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liuzhangjian<liuzhangjian@uniontech.com>
 *
 * Maintainer: liuzhangjian<liuzhangjian@uniontech.com>
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
#include "searcheventreceiver.h"
#include "searcheventcaller.h"
#include "utils/searchhelper.h"

#include "services/filemanager/windows/windowsservice.h"
#include "services/filemanager/search/searchservice.h"

DSB_FM_USE_NAMESPACE
DPSEARCH_BEGIN_NAMESPACE

namespace GlobalPrivate {
static SearchService *searchServ { nullptr };
}   // namespace GlobalPrivate

dfmplugin_search::SearchEventReceiver *dfmplugin_search::SearchEventReceiver::instance()
{
    static SearchEventReceiver ins;
    return &ins;
}

void SearchEventReceiver::handleSearch(quint64 winId, const QString &keyword)
{
    auto &ctx = dpfInstance.serviceContext();
    auto windowService = ctx.service<WindowsService>(WindowsService::name());
    auto window = windowService->findWindowById(winId);
    Q_ASSERT(window);

    auto url = window->currentUrl();
    QUrl searchUrl;
    if (SearchHelper::isSearchFile(url)) {
        searchUrl = SearchHelper::setSearchKeyword(url, keyword);
    } else {
        searchUrl = SearchHelper::fromSearchFile(url, keyword, QString::number(winId), QUrl());
    }

    SearchEventCaller::sendDoSearch(winId, searchUrl);
}

void SearchEventReceiver::handleStopSearch(quint64 winId)
{
    if (!GlobalPrivate::searchServ) {
        auto &ctx = dpfInstance.serviceContext();
        GlobalPrivate::searchServ = ctx.service<SearchService>(SearchService::name());

        if (!GlobalPrivate::searchServ) {
            qCritical() << "get SearchService failed!";
            abort();
        }
    }

    GlobalPrivate::searchServ->stop(QString::number(winId));
}

SearchEventReceiver::SearchEventReceiver(QObject *parent)
    : QObject(parent)
{
}

DPSEARCH_END_NAMESPACE