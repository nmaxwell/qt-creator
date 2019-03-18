/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include <QObject>
#include <QGradient>

class GradientPresetItem
{
    Q_GADGET

    Q_PROPERTY(QGradient::Preset preset READ preset FINAL)
    Q_PROPERTY(QList<qreal> stopsPosList READ stopsPosList FINAL)
    Q_PROPERTY(QList<QString> stopsColorList READ stopsColorList FINAL)
    Q_PROPERTY(int stopListSize READ stopListSize FINAL)
    Q_PROPERTY(QString presetName READ presetName FINAL)
    Q_PROPERTY(int presetID READ presetID FINAL)

public:
    explicit GradientPresetItem();
    explicit GradientPresetItem(const QGradient &value, const QString &name = QString());
    explicit GradientPresetItem(const QGradient::Preset number);
    ~GradientPresetItem();

    enum Property {
        objectNameRole = 0,
        presetRole = 1,
        stopsPosListRole = 2,
        stopsColorListRole = 3,
        stopListSizeRole = 4,
        presetNameRole = 5,
        presetIDRole = 6
    };

    QVariant getProperty(Property id) const;

    QGradient gradientVal() const;
    QGradient::Preset preset() const;

    void setGradient(const QGradient &value);
    void setGradient(const QGradient::Preset value);

    QList<qreal> stopsPosList() const;
    QList<QString> stopsColorList() const;
    int stopListSize() const;

    void setPresetName(const QString &value);
    QString presetName() const;
    int presetID() const;

    static QString getNameByPreset(QGradient::Preset value);

    friend QDebug &operator<<(QDebug &stream, const GradientPresetItem &gradient);

    friend QDataStream &operator<<(QDataStream &stream, const GradientPresetItem &gradient);
    friend QDataStream &operator>>(QDataStream &stream, GradientPresetItem &gradient);

private:
    QGradient m_gradientVal;
    QGradient::Preset m_gradientID;
    QString m_presetName;
};

Q_DECLARE_METATYPE(GradientPresetItem)
