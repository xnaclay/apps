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

#include "devicefinder.h"
#include "deviceinfo.h"

static const QLatin1String BT_SERVER_UUID("3bb45162-cecf-4bcb-be9f-026ec7ab38be");

std::vector<std::string> parse_cmd(const std::string &cmd) {
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
    qDeleteAll(m_devices);
    m_devices.clear();

    emit devicesChanged();

//    m_deviceDiscoveryAgent.start();
    qInfo() << "starting service scan";
    m_serviceDiscoveryAgent.setUuidFilter(QBluetoothUuid(BT_SERVER_UUID));
    m_serviceDiscoveryAgent.start(QBluetoothServiceDiscoveryAgent::FullDiscovery);

    emit scanningChanged();
    setInfo(tr("Scanning for devices..."));
}

void DeviceFinder::addDevice(const QBluetoothDeviceInfo &device)
{
    qInfo() << "found device" << device.address().toString();
    m_devices.append(new DeviceInfo(device));
    setInfo(tr("Device found. Scanning more..."));
    emit devicesChanged();
}

void DeviceFinder::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    qInfo() << "service discovered"
            << service.device().name()
            << service.device().address().toString();
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
    if (m_devices.size() == 0) {
        setError(tr("No devices found."));
    } else {
        setInfo(tr("Scanning done."));
    }

    emit scanningChanged();
    emit devicesChanged();
}

void DeviceFinder::connectToService(const QString &address)
{
    m_deviceDiscoveryAgent.stop();

    DeviceInfo *currentDevice = nullptr;
    for (int i = 0; i < m_devices.size(); i++) {
        if (((DeviceInfo*)m_devices.at(i))->getAddress() == address ) {
            currentDevice = (DeviceInfo*)m_devices.at(i);
            break;
        }
    }

    if (currentDevice) {
        qInfo() << "connect player device"
                << currentDevice->getAddress();
        m_settings->setValue("player.address", currentDevice->getAddress());
        m_settings->setValue("player.name", currentDevice->getName());
        //PROOF OF CONCEPT
        socket.connectToService(QBluetoothAddress(currentDevice->getAddress()),
                                QBluetoothUuid(BT_SERVER_UUID));

        connect(&socket, &QBluetoothSocket::readyRead, [this]() {
            qInfo() << "ready to read from server";

            while (socket.canReadLine()) {
                QByteArray lineData = socket.readLine();
                QString line = QString::fromUtf8(lineData.constData(), lineData.length());
                qInfo() << "read line"
                        << line;
                auto cmdv = parse_cmd(line.toStdString());

                if (cmdv[0] == "BT_DEVICE" && cmdv.size() == 3) {
                    m_speakerDevices.append(new DeviceInfo(QString::fromStdString(cmdv[1]),
                                                           QString::fromStdString(cmdv[2])));
                    emit speakerDevicesChanged();
                }
            }
        });
        connect(&socket, &QBluetoothSocket::connected, [this]() {
            qInfo() << "connected to service";

            qInfo() << "sending SCAN command";

            socket.write("SCAN\n");
        });
        connect(&socket, &QBluetoothSocket::disconnected, []() {
            qInfo() << "disconnected from service";
        });
    }

    clearMessages();
}

bool DeviceFinder::scanning() const
{
    return m_deviceDiscoveryAgent.isActive();
}

QVariant DeviceFinder::devices()
{
    return QVariant::fromValue(m_devices);
}

QVariant DeviceFinder::speakerDevices()
{
    return QVariant::fromValue(m_speakerDevices);
}
