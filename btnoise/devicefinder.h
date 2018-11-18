/***************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DEVICEFINDER_H
#define DEVICEFINDER_H

#include "app-global.h"
#include "bluetoothbaseclass.h"

#include <QTimer>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothServiceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothServiceInfo>
#include <QBluetoothSocket>
#include <QBluetoothAddress>
#include <QVariant>
#include <QSettings>

class DeviceInfo;

class DeviceFinder: public BluetoothBaseClass
{
    Q_OBJECT

    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QVariant devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QVariant speakerDevices READ speakerDevices NOTIFY speakerDevicesChanged)
    Q_PROPERTY(QVariant volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(QVariant playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(QVariant playerConfigured READ playerConfigured NOTIFY playerConfiguredChanged)
    Q_PROPERTY(QVariant playerConnected READ playerConnected NOTIFY playerConnectedChanged)
    Q_PROPERTY(QVariant speakerConfigured READ speakerConfigured NOTIFY speakerConfiguredChanged)
    Q_PROPERTY(QVariant speakerConnected READ playing NOTIFY speakerConnectedChanged)

public:
    DeviceFinder(QSettings *settings, QObject *parent = nullptr);
    ~DeviceFinder();

    bool scanning() const;
    QVariant devices();
    QVariant volume();
    QVariant playing();
    QVariant playerConfigured();
    QVariant speakerConfigured();
    QVariant playerConnected();
    QVariant speakerConnected();
    QVariant speakerDevices();

public slots:
    void startSearch();
    void connectToService(const QString &address);
    void startSpeakerSearch();
    void connectToSpeaker(const QString &address);
    void disconnectAllSpeakers();
    void play();
    void stop();
    void setVolume(unsigned int vol);
    void sendVolCmd();
    void ensureConnected();
private slots:
    void addDevice(const QBluetoothDeviceInfo&);
    void serviceDiscovered(const QBluetoothServiceInfo&);
    void scanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void scanFinished();

signals:
    void scanningChanged();
    void devicesChanged();
    void speakerDevicesChanged();
    void volumeChanged();
    void playingChanged();
    void playerConfiguredChanged();
    void playerConnectedChanged();
    void speakerConfiguredChanged();
    void speakerConnectedChanged();

private:
    QSettings *m_settings;
    QBluetoothLocalDevice m_localDevice;

    QBluetoothSocket socket;

    QBluetoothDeviceDiscoveryAgent m_deviceDiscoveryAgent;
    QBluetoothServiceDiscoveryAgent m_serviceDiscoveryAgent;
    QList<QObject*> m_devices;
    QList<QObject*> m_speakerDevices;
    QTimer m_volControlTimer;
    QTimer m_connWatchdogTimer;

    unsigned int m_volume = 0;
    bool m_playing = false;
    bool m_playerConnected = false;
    bool m_speakerConnected = false;

    void sendCmd(const std::vector<std::string> &cmdv);
    void readServer();
    std::vector<std::string> parseCmd(const std::string &cmd);
    void handlePlayerConnection();
};

#endif // DEVICEFINDER_H
