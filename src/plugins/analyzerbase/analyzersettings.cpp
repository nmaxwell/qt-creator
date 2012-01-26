/**************************************************************************
**
** This file is part of Qt Creator Instrumentation Tools
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Author: Milian Wolff, KDAB (milian.wolff@kdab.com)
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "analyzersettings.h"

#include "analyzermanager.h"
#include "ianalyzertool.h"
#include "analyzerplugin.h"
#include "analyzeroptionspage.h"

#include <coreplugin/icore.h>
#include <utils/qtcassert.h>

#include <QtCore/QSettings>

using namespace Analyzer::Internal;

static const char groupC[] = "Analyzer";
static const char useGlobalC[] = "Analyzer.Project.UseGlobal";

namespace Analyzer {

AnalyzerGlobalSettings *AnalyzerGlobalSettings::m_instance = 0;

AnalyzerSettings::AnalyzerSettings(QObject *parent)
    : QObject(parent)
{
}

QVariantMap AnalyzerSettings::defaults() const
{
    QVariantMap map;
    foreach (AbstractAnalyzerSubConfig *config, subConfigs()) {
        map.unite(config->defaults());
    }
    return map;
}

bool AnalyzerSettings::fromMap(const QVariantMap &map)
{
    return fromMap(map, &m_subConfigs);
}

bool AnalyzerSettings::fromMap(const QVariantMap &map, QList<AbstractAnalyzerSubConfig *> *subConfigs)
{
    bool ret = true;
    foreach (AbstractAnalyzerSubConfig *config, *subConfigs) {
        ret = ret && config->fromMap(map);
    }
    return ret;
}

QVariantMap AnalyzerSettings::toMap() const
{
    return toMap(m_subConfigs);
}

QVariantMap AnalyzerSettings::toMap(const QList<AbstractAnalyzerSubConfig *> &subConfigs) const
{
    QVariantMap map;
    foreach (AbstractAnalyzerSubConfig *config, subConfigs) {
        map.unite(config->toMap());
    }
    return map;
}


AnalyzerGlobalSettings::AnalyzerGlobalSettings(QObject *parent)
: AnalyzerSettings(parent)
{
    QTC_ASSERT(!m_instance, return);
    m_instance = this;
}

AnalyzerGlobalSettings *AnalyzerGlobalSettings::instance()
{
    if (!m_instance)
        m_instance = new AnalyzerGlobalSettings(AnalyzerPlugin::instance());

    return m_instance;
}

AnalyzerGlobalSettings::~AnalyzerGlobalSettings()
{
    m_instance = 0;
    qDeleteAll(m_subConfigs);
}

void AnalyzerGlobalSettings::readSettings()
{
    QSettings *settings = Core::ICore::settings();

    QVariantMap map;

    settings->beginGroup(QLatin1String(groupC));
    // read the values from config, using the keys from the defaults value map
    const QVariantMap def = defaults();
    for (QVariantMap::ConstIterator it = def.constBegin(); it != def.constEnd(); ++it)
        map.insert(it.key(), settings->value(it.key(), it.value()));
    settings->endGroup();

    // apply the values to our member variables
    fromMap(map);
}

void AnalyzerGlobalSettings::writeSettings() const
{
    QSettings *settings = Core::ICore::settings();
    settings->beginGroup(QLatin1String(groupC));
    const QVariantMap map = toMap();
    for (QVariantMap::ConstIterator it = map.begin(); it != map.end(); ++it)
        settings->setValue(it.key(), it.value());
    settings->endGroup();
}

void AnalyzerGlobalSettings::registerTool(IAnalyzerTool *tool)
{
    AbstractAnalyzerSubConfig *config = tool->createGlobalSettings();
    if (config) {
        m_subConfigs.append(config);
        AnalyzerPlugin::instance()->addAutoReleasedObject(new AnalyzerOptionsPage(config));
        readSettings();
    }
}


AnalyzerProjectSettings::AnalyzerProjectSettings(QObject *parent)
    : AnalyzerSettings(parent), m_useGlobalSettings(true)
{
    QList<IAnalyzerTool*> tools = AnalyzerManager::tools();
    // add sub configs
    foreach (IAnalyzerTool *tool, tools) {
        AbstractAnalyzerSubConfig *config = tool->createProjectSettings();
        if (config)
            m_customConfigurations.append(config);
    }

    m_subConfigs = AnalyzerGlobalSettings::instance()->subConfigs();
    resetCustomToGlobalSettings();
}

AnalyzerProjectSettings::~AnalyzerProjectSettings()
{
    qDeleteAll(m_customConfigurations);
}

QString AnalyzerProjectSettings::displayName() const
{
    return tr("Analyzer Settings");
}

bool AnalyzerProjectSettings::fromMap(const QVariantMap &map)
{
    if (!AnalyzerSettings::fromMap(map, &m_customConfigurations))
        return false;
    m_useGlobalSettings = map.value(QLatin1String(useGlobalC), true).toBool();
    return true;
}

QVariantMap AnalyzerProjectSettings::toMap() const
{
    QVariantMap map = AnalyzerSettings::toMap(m_customConfigurations);
    map.insert(QLatin1String(useGlobalC), m_useGlobalSettings);
    return map;
}

void AnalyzerProjectSettings::setUsingGlobalSettings(bool value)
{
    if (value == m_useGlobalSettings)
        return;
    m_useGlobalSettings = value;
    if (m_useGlobalSettings) {
        m_subConfigs = AnalyzerGlobalSettings::instance()->subConfigs();
    } else {
        m_subConfigs = m_customConfigurations;
    }
}

void AnalyzerProjectSettings::resetCustomToGlobalSettings()
{
    AnalyzerGlobalSettings *gs = AnalyzerGlobalSettings::instance();
    AnalyzerSettings::fromMap(gs->toMap(), &m_customConfigurations);
}

} // namespace Analyzer
