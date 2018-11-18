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

#include <sstream>

#include "devicefinder.h"
#include "deviceinfo.h"

static const QLatin1String BT_SERVER_UUID("3bb45162-cecf-4bcb-be9f-026ec7ab38be");

DeviceFinder::DeviceFinder(QSettings *settings, QObject *parent):
    BluetoothBaseClass(parent),
    m_settings(settings),
    m_localDevice(parent),
    socket(QBluetoothServiceInfo::RfcommProtocol),
    m_deviceDiscoveryAgent(this),
  m_serviceDiscoveryAgent(this)
{
    connect(&m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &DeviceFinder::addDevice);
    connect(&m_deviceDiscoveryAgent, static_cast<void (QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error),
            this, &DeviceFinder::scanError);

    connect(&m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, &DeviceFinder::scanFinished);
    connect(&m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled, this, &DeviceFinder::scanFinished);

    connect(&m_serviceDiscoveryAgent, &QBluetoothServiceDiscoveryAgent::serviceDiscovered, this, &DeviceFinder::serviceDiscovered);

    connect(&socket, &QBluetoothSocket::readyRead, this, &DeviceFinder::readServer);
    connect(&socket, &QBluetoothSocket::connected, this, &DeviceFinder::handlePlayerConnection);
    connect(&socket, &QBluetoothSocket::disconnected, [this]() {
        qInfo() << "disconnected from service";
        m_playerConnected = false;
        emit playerConnectedChanged();
    });

    connect(&m_volControlTimer, &QTimer::timeout, this, &DeviceFinder::sendVolCmd);
    connect(&m_connWatchdogTimer, &QTimer::timeout, this, &DeviceFinder::ensureConnected);

    m_connWatchdogTimer.setInterval(5000);
    m_connWatchdogTimer.start();

    if (m_settings->contains("player.address")) {
        qInfo() << "adding saved player to list"
                << m_settings->value("player.address").toString()
                << m_settings->value("player.name").toString();
        m_devices.append(new DeviceInfo(m_settings->value("player.address").toString(), m_settings->value("player.name").toString()));
    }

    if (m_settings->contains("speaker.address")) {
        qInfo() << "adding saved speaker to list"
                << m_settings->value("speaker.address").toString()
                << m_settings->value("speaker.name").toString();
        m_speakerDevices.append(new DeviceInfo(m_settings->value("speaker.address").toString(), m_settings->value("speaker.name").toString()));
    }
}

DeviceFinder::~DeviceFinder()
{
    qDeleteAll(m_devices);
    m_devices.clear();

    qDeleteAll(m_speakerDevices);
    m_speakerDevices.clear();
}

void DeviceFinder::startSearch()
{
    clearMessages();

    emit devicesChanged();

    qInfo() << "starting service scan";
    m_serviceDiscoveryAgent.setUuidFilter(QBluetoothUuid(BT_SERVER_UUID));
    m_serviceDiscoveryAgent.start(QBluetoothServiceDiscoveryAgent::FullDiscovery);

    emit scanningChanged();
}

void DeviceFinder::addDevice(const QBluetoothDeviceInfo &device)
{
    qInfo() << "found device" << device.address().toString();
    m_devices.append(new DeviceInfo(device));
//    setInfo(tr("Device found. Scanning more..."));
    emit devicesChanged();
}

void DeviceFinder::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    qInfo() << "service discovered"
            << service.device().name()
            << service.device().address().toString();

    for (const auto &device : m_devices) {
        if (QString::compare(static_cast<DeviceInfo *>(device)->getAddress(), service.device().address().toString()) == 0) {
            return;
        }
    }

    m_devices.append(new DeviceInfo(service));
    emit devicesChanged();
}

void DeviceFinder::scanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError) {
        setError(tr("The Bluetooth adaptor is powered off."));
    } else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError) {
        setError(tr("Writing or reading from the device resulted in an error."));
    } else {
        setError(tr("An unknown error has occurred."));
    }
}

void DeviceFinder::scanFinished()
{
//    if (m_devices.size() == 0) {
//        setError(tr("No devices found."));
//    } else {
//        setInfo(tr("Scanning done."));
//    }

    emit scanningChanged();
    emit devicesChanged();
}

std::vector<std::string> DeviceFinder::parseCmd(const std::string &cmd)
{
  std::string delim = ",";
  std::vector<std::string> cmdv;

  unsigned long start = 0U;
  unsigned long end = cmd.find(delim);

  while (end != std::string::npos) {
    cmdv.emplace_back(cmd.substr(start, end - start));
    start = end + delim.length();
    end = cmd.find(delim, start);
  }

  cmdv.emplace_back(cmd.substr(start, end));

  return cmdv;
}

void DeviceFinder::sendCmd(const std::vector<std::string> &cmdv)
{
    std::stringstream cmd_ss("");

    for (unsigned int i = 0; i < cmdv.size(); i++) {
        socket.write(cmdv[i].c_str());

        if (i < cmdv.size() - 1) {
            socket.write(",");
        }
    }

    socket.write("\n");
}

void DeviceFinder::readServer() {
    qInfo() << "ready to read from server";

    while (socket.canReadLine()) {
        QByteArray lineData = socket.readLine();
        QString line = QString::fromUtf8(lineData.constData(), lineData.length());
        auto cmdv = parseCmd(line.trimmed().toStdString());

        for (const auto &v : cmdv) {
            qInfo() << "CMD" << v.c_str();
        }


        std::map<std::string, std::function<void()>> cmd_dispatch;

        cmd_dispatch.emplace("BT_DEVICE", [this, &cmdv]() {
            for (const auto &device : m_speakerDevices) {
                if (static_cast<DeviceInfo *>(device)->getAddress().toStdString() == cmdv[1]) {
                    qInfo() << "speaker device already known"
                            << static_cast<DeviceInfo *>(device)->getAddress();
                    return;
                }
            }
            qInfo() << "discovered speaker"
                    << QString::fromStdString(cmdv[1])
                    << QString::fromStdString(cmdv[2]);

            m_speakerDevices.append(new DeviceInfo(QString::fromStdString(cmdv[1]),
                                                   QString::fromStdString(cmdv[2])));
            emit speakerDevicesChanged();
        });

        cmd_dispatch.emplace("CONNECTED_SPEAKER", [this, &cmdv]() {
            qInfo() << "player reported speaker connected"
                    << cmdv[1].c_str();
            m_speakerConnected = true;
            emit speakerConnectedChanged();
        });

        cmd_dispatch.emplace("DISCONNECTED_SPEAKER", [this]() {
            qInfo() << "player reported speaker disconnected";
            m_speakerConnected = false;
            emit speakerConnectedChanged();
        });

        cmd_dispatch.emplace("VOL", [this, &cmdv]() {
            qInfo() << "player reported volume"
                    << cmdv[1].c_str();
            m_volume = static_cast<unsigned int>(std::atoi(cmdv[1].c_str()));
            emit volumeChanged();
        });

        cmd_dispatch.emplace("PLAYING", [this]() {
            qInfo() << "player reported playing";
            m_playing = true;
            emit playingChanged();
        });

        cmd_dispatch.emplace("STOPPED", [this]() {
            qInfo() << "player reported stopped";
            m_playing = false;
            emit playingChanged();
        });

        auto cmd_it = cmd_dispatch.find(cmdv[0]);

        if (cmd_it != cmd_dispatch.end()) {
          cmd_dispatch[cmd_it->first]();
        } else {
          qInfo() << "unrecognized command";
        }
    }
}

void DeviceFinder::handlePlayerConnection()
{
    qInfo() << "connected to service";
    m_playerConnected = true;
    emit playerConnectedChanged();
}

void DeviceFinder::connectToService(const QString &address)
{
    m_deviceDiscoveryAgent.stop();

    DeviceInfo *currentDevice = nullptr;
    for (int i = 0; i < m_devices.size(); i++) {
        if (static_cast<DeviceInfo *>(m_devices.at(i))->getAddress() == address ) {
            currentDevice = static_cast<DeviceInfo *>(m_devices.at(i));
            break;
        }
    }

    if (currentDevice) {
        qInfo() << "connect player device"
                << currentDevice->getAddress();
        m_settings->setValue("player.address", currentDevice->getAddress());
        m_settings->setValue("player.name", currentDevice->getName());
        if (socket.state() != QBluetoothSocket::UnconnectedState) {
            m_playerConnected = false;
            emit playerConnectedChanged();
            socket.close();
        }
        ensureConnected();

    }

    clearMessages();
}

void DeviceFinder::ensureConnected() {
    if (socket.state() == QBluetoothSocket::UnconnectedState) {
        socket.connectToService(QBluetoothAddress(m_settings->value("player.address").toString()), QBluetoothUuid(BT_SERVER_UUID));
    }
}

void DeviceFinder::startSpeakerSearch()
{
    qInfo() << "sending SCAN command";

    sendCmd({"SCAN"});
}

void DeviceFinder::connectToSpeaker(const QString &address)
{

    DeviceInfo *currentDevice = nullptr;
    for (int i = 0; i < m_speakerDevices.size(); i++) {
        if (static_cast<DeviceInfo *>(m_speakerDevices.at(i))->getAddress() == address ) {
            currentDevice = static_cast<DeviceInfo *>(m_speakerDevices.at(i));
            break;
        }
    }

    m_settings->setValue("speaker.address", currentDevice->getAddress());
    m_settings->setValue("speaker.name", currentDevice->getName());

    qInfo() << "sending request to connect to speaker"
            << address;
    sendCmd({"CONNECT", address.toStdString()});
}

void DeviceFinder::disconnectAllSpeakers()
{
    qInfo() << "sending request to remove all speakers";
    sendCmd({"UNPAIR_SPEAKER"});
}

void DeviceFinder::play()
{
    qInfo() << "sending request to play";
    sendCmd({"PLAY"});
    m_playing = true;
    emit playingChanged();
}

void DeviceFinder::stop()
{
    qInfo() << "sending request to stop";
    sendCmd({"STOP"});
    m_playing = false;
    emit playingChanged();
}

void DeviceFinder::setVolume(unsigned int vol)
{
    qInfo() << "sending request to set volume";
    m_volControlTimer.stop();
    m_volControlTimer.setInterval(500);
    m_volControlTimer.setSingleShot(true);
    m_volControlTimer.start();
    m_volume = vol;
    emit volumeChanged();
}

void DeviceFinder::sendVolCmd() {
    qInfo() << "sending request to set volume";
    sendCmd({"SET_VOL", QString::number(m_volume).toStdString()});
}

bool DeviceFinder::scanning() const
{
    return m_deviceDiscoveryAgent.isActive();
}

QVariant DeviceFinder::devices()
{
    return QVariant::fromValue(m_devices);
}

QVariant DeviceFinder::volume()
{
    return QVariant::fromValue(m_volume);
}

QVariant DeviceFinder::playing()
{
    return QVariant::fromValue(m_playing);
}

QVariant DeviceFinder::playerConfigured()
{
    if (m_settings->contains("player.address")) {
        return QVariant::fromValue(true);
    } else {
        return QVariant::fromValue(false);
    }
}

QVariant DeviceFinder::speakerConfigured()
{
    if (m_settings->contains("speaker.address")) {
        return QVariant::fromValue(true);
    } else {
        return QVariant::fromValue(false);
    }
}

QVariant DeviceFinder::playerConnected()
{
    return QVariant::fromValue(m_playerConnected);
}

QVariant DeviceFinder::speakerConnected()
{
    return QVariant::fromValue(m_speakerConnected);
}

QVariant DeviceFinder::speakerDevices()
{
    return QVariant::fromValue(m_speakerDevices);
}
