// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waylandcopyclient.h"
#include "readpipedatatask.h"
#include "datacontroldevicemanager.h"
#include "datacontroldevice.h"
#include "datacontrolsource.h"
#include "datacontroloffer.h"

#include <QEventLoop>
#include <QMimeData>
#include <QImageReader>
#include <QtConcurrent>
#include <QImageWriter>
#include <QMutexLocker>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>

#include <wayland-client.h>
#include "data-control-client-protocol.h"

#include <unistd.h>

static const QString ApplicationXQtImageLiteral QStringLiteral("application/x-qt-image");

static QStringList imageMimeFormats(const QList<QByteArray> &imageFormats)
{
    QStringList formats;
    formats.reserve(imageFormats.size());
    for (const auto &format : imageFormats)
        formats.append(QLatin1String("image/") + QLatin1String(format.toLower()));

    // put png at the front because it is best
    int pngIndex = formats.indexOf(QLatin1String("image/png"));
    if (pngIndex != -1 && pngIndex != 0)
        formats.move(pngIndex, 0);

    return formats;
}

static inline QStringList imageReadMimeFormats()
{
    return imageMimeFormats(QImageReader::supportedImageFormats());
}

static QByteArray getByteArray(QMimeData *mimeData, const QString &mimeType)
{
    QByteArray content;
    if (mimeType == QLatin1String("text/plain")) {
        content = mimeData->text().toUtf8();
    } else if (mimeData->hasImage()
               && (mimeType == QLatin1String("application/x-qt-image")
                   || mimeType.startsWith(QLatin1String("image/")))) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull()) {
            QBuffer buf;
            buf.open(QIODevice::ReadWrite);
            QByteArray fmt = "BMP";
            if (mimeType.startsWith(QLatin1String("image/"))) {
                QByteArray imgFmt = mimeType.mid(6).toUpper().toLatin1();
                if (QImageWriter::supportedImageFormats().contains(imgFmt))
                    fmt = imgFmt;
            }
            QImageWriter wr(&buf, fmt);
            wr.write(image);
            content = buf.buffer();
        }
    } else if (mimeType == QLatin1String("application/x-color")) {
        content = qvariant_cast<QColor>(mimeData->colorData()).name().toLatin1();
    } else if (mimeType == QLatin1String("text/uri-list")) {
        QList<QUrl> urls = mimeData->urls();
        for (int i = 0; i < urls.count(); ++i) {
            content.append(urls.at(i).toEncoded());
            content.append('\n');
        }
    } else {
        content = mimeData->data(mimeType);
    }
    return content;
}

DMimeData::DMimeData()
{

}

DMimeData::~DMimeData()
{

}

QVariant DMimeData::retrieveData(const QString &mimeType, QMetaType preferredType) const
{
    QVariant data = QMimeData::retrieveData(mimeType, preferredType);
    if (mimeType == QLatin1String("application/x-qt-image")) {
        if (data.isNull() || (data.userType() == QMetaType::QByteArray && data.toByteArray().isEmpty())) {
            // try to find an image
            QStringList imageFormats = imageReadMimeFormats();
            for (int i = 0; i < imageFormats.size(); ++i) {
                data = QMimeData::retrieveData(imageFormats.at(i), preferredType);
                if (data.isNull() || (data.userType() == QMetaType::QByteArray && data.toByteArray().isEmpty()))
                    continue;
                break;
            }
        }
        int typeId = preferredType.id();
        // we wanted some image type, but all we got was a byte array. Convert it to an image.
        if (data.userType() == QMetaType::QByteArray
            && (typeId == QMetaType::QImage || typeId == QMetaType::QPixmap || typeId == QMetaType::QBitmap))
            data = QImage::fromData(data.toByteArray());
    } else if (mimeType == QLatin1String("application/x-color") && data.userType() == QMetaType::QByteArray) {
        QColor c;
        QByteArray ba = data.toByteArray();
        if (ba.size() == 8) {
            ushort * colBuf = (ushort *)ba.data();
            c.setRgbF(qreal(colBuf[0]) / qreal(0xFFFF),
                      qreal(colBuf[1]) / qreal(0xFFFF),
                      qreal(colBuf[2]) / qreal(0xFFFF),
                      qreal(colBuf[3]) / qreal(0xFFFF));
            data = c;
        } else {
            qWarning() << "Qt: Invalid color format";
        }
    } else {
        data = QMimeData::retrieveData(mimeType, preferredType);
    }
    return data;
}

WaylandCopyClient::WaylandCopyClient(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
    , m_eventQueue(nullptr)
    , m_dataControlDeviceManager(nullptr)
    , m_dataControlDevice(nullptr)
    , m_copyControlSource(nullptr)
    , m_mimeData(new DMimeData())
    , m_seat(nullptr)
    , m_currentOffer(nullptr)
    , m_curOffer(0)
{

}

WaylandCopyClient::~WaylandCopyClient()
{
    m_connectionThread->quit();
    m_connectionThread->wait();

    if (m_currentOffer) {
        delete m_currentOffer;
        m_currentOffer = nullptr;
    }

    if (m_dataControlDevice) {
        m_dataControlDevice->destroy();
        delete m_dataControlDevice;
        m_dataControlDevice = nullptr;
    }

    if (m_dataControlDeviceManager) {
        m_dataControlDeviceManager->release();
        delete m_dataControlDeviceManager;
        m_dataControlDeviceManager = nullptr;
    }

    m_connectionThreadObject->deleteLater();

    if (m_mimeData)
        m_mimeData->deleteLater();
}

void WaylandCopyClient::init()
{
    connect(m_connectionThreadObject, &ConnectionThread::connected, this, [this] {
        qInfo() << "WaylandCopyClient: Connection established";

        m_eventQueue = new EventQueue(this);
        m_eventQueue->setup(m_connectionThreadObject);

        wl_display *display = m_connectionThreadObject->display();
        wl_registry *registry = wl_display_get_registry(display);

        static const wl_registry_listener s_registryListener = {
            .global = [](void *data, wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
                auto self = static_cast<WaylandCopyClient *>(data);
                if (qstrcmp(interface, "wl_seat") == 0) {
                    self->m_seat = static_cast<wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, qMin(version, 7u)));
                    qInfo() << "WaylandCopyClient: wl_seat found";

                    if (self->m_dataControlDeviceManager && !self->m_dataControlDevice) {
                        self->m_dataControlDevice = self->m_dataControlDeviceManager->getDataDevice(self->m_seat, nullptr);
                        if (self->m_dataControlDevice) {
                            qInfo() << "WaylandCopyClient: DataControlDevice created after wl_seat";
                            self->connectDevice(self->m_dataControlDevice);
                        }
                    }
                } else if (qstrcmp(interface, "zwlr_data_control_manager_v1") == 0) {
                    auto manager = reinterpret_cast<zwlr_data_control_manager_v1 *>(
                        wl_registry_bind(reg, name, &zwlr_data_control_manager_v1_interface, qMin(version, 2u)));
                    self->m_dataControlDeviceManager = new DataControlDeviceManager(nullptr);
                    self->m_dataControlDeviceManager->setup(manager);
                    qInfo() << "WaylandCopyClient: DataControlManager found";

                    if (self->m_seat && !self->m_dataControlDevice) {
                        self->m_dataControlDevice = self->m_dataControlDeviceManager->getDataDevice(self->m_seat, nullptr);
                        if (self->m_dataControlDevice) {
                            qInfo() << "WaylandCopyClient: DataControlDevice created after manager";
                            self->connectDevice(self->m_dataControlDevice);
                        }
                    }
                }
            },
            .global_remove = [](void *, wl_registry *, uint32_t) {}
        };
        wl_registry_add_listener(registry, &s_registryListener, this);

        wl_display_roundtrip(display);

        if (m_dataControlDeviceManager && m_seat && !m_dataControlDevice) {
            qInfo() << "WaylandCopyClient: Creating DataControlDevice after roundtrip";
            m_dataControlDevice = m_dataControlDeviceManager->getDataDevice(m_seat, nullptr);
            if (m_dataControlDevice) {
                connectDevice(m_dataControlDevice);
            }
        }

        if (!m_dataControlDevice) {
            qWarning() << "WaylandCopyClient: Failed to create DataControlDevice";
        } else {
            qInfo() << "WaylandCopyClient: Initialization complete";
        }
    }, Qt::QueuedConnection );
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();
    m_connectionThreadObject->initConnection();
    connect(this, &WaylandCopyClient::dataCopied, this, &WaylandCopyClient::onDataCopied);
}

void WaylandCopyClient::connectDevice(DataControlDeviceV1 *device)
{
    connect(device, &DataControlDeviceV1::selectionCleared, this, [this] {
        m_copyControlSource = nullptr;
    }, Qt::QueuedConnection);

    connect(device, &DataControlDeviceV1::dataOffered, this, [this](DataControlOfferV1 *offer) {
        if (m_currentOffer) {
            m_currentOffer->deleteLater();
        }
        m_currentOffer = offer;
        onDataOffered(offer);
    }, Qt::QueuedConnection);
}

void WaylandCopyClient::onDataOffered(KWayland::Client::DataControlOfferV1* offer)
{
    if (!offer || !offer->isValid())
        return;

    QList<QString> mimeTypeList = filterMimeType(offer->offeredMimeTypes());

    if (mimeTypeList.isEmpty())
        return;

    m_curMimeTypes = mimeTypeList;

    m_curOffer = qint64(offer);

    if (!m_mimeData)
        m_mimeData = new DMimeData();
    m_mimeData->clear();

    // try to stop the previous pipeline
    Q_EMIT dataOfferedNew();
    execTask(mimeTypeList, offer);
}

// NOTE: no thread, this should be block
void WaylandCopyClient::execTask(const QStringList &mimeTypes, DataControlOfferV1 *offer)
{
    for (const QString &mimeType : mimeTypes) {
        ReadPipeDataTask *task = new ReadPipeDataTask(m_connectionThreadObject, offer, mimeType, this);
        connect(this, &WaylandCopyClient::dataOfferedNew, task, &ReadPipeDataTask::forceClosePipeLine);
        connect(task, &ReadPipeDataTask::dataReady, this, &WaylandCopyClient::taskDataReady);

        task->run();
    }
}

void WaylandCopyClient::taskDataReady(qint64 offer, const QString &mimeType, const QByteArray &data)
{
    if (offer != m_curOffer) {
        return;
    }

    m_curMimeTypes.removeOne(mimeType);

    m_mimeData->setData(mimeType, data);
    if (m_curMimeTypes.isEmpty()) {
        m_curOffer = 0;

        Q_EMIT dataChanged();
    }
}

const QMimeData* WaylandCopyClient::mimeData()
{
    return m_mimeData;
}

// NOTE: copy entrance
void WaylandCopyClient::setMimeData(QMimeData *mimeData)
{
    if (m_mimeData)
        m_mimeData->deleteLater();

    m_mimeData = mimeData;

    Q_EMIT dataCopied();
    Q_EMIT dataChanged();
}

void WaylandCopyClient::onDataCopied()
{
    sendOffer();
}

void WaylandCopyClient::sendOffer()
{
    m_copyControlSource = m_dataControlDeviceManager->createDataSource(this);
    if (!m_copyControlSource)
        return;

    m_dataControlDevice->setSelection(0, m_copyControlSource);
    if (m_mimeData->formats().isEmpty()) {
        return;
    }
    for (const QString &format : m_mimeData->formats()) {
        // 如果是application/x-qt-image类型则需要提供image的全部类型, 比如image/png
        if (ApplicationXQtImageLiteral == format) {
            QStringList imageFormats = imageReadMimeFormats();
            for (int i = 0; i < imageFormats.size(); ++i) {
                m_copyControlSource->offer(imageFormats.at(i));
            }
            continue;
        }
        m_copyControlSource->offer(format);
    }

    connect(m_copyControlSource, &DataControlSourceV1::sendDataRequested, this, &WaylandCopyClient::onSendDataRequest);
    m_connectionThreadObject->flush();
}

void WaylandCopyClient::onSendDataRequest(const QString &mimeType, qint32 fd) const
{
    QFile f;
    if (f.open(fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
        const QByteArray &ba = getByteArray(m_mimeData, mimeType);
        f.write(ba);
        f.close();
    }
}

QStringList WaylandCopyClient::filterMimeType(const QStringList &mimeTypeList)
{
    QStringList tmpList;
    for (const QString &mimeType : mimeTypeList) {
        // 根据窗管的要求，不读取纯大写、和不含'/'的字段，因为源窗口可能没有写入这些字段的数据，导致获取数据的线程一直等待。
        if ((mimeType.contains("/") && mimeType.toUpper() != mimeType)
            || mimeType == "FROM_DEEPIN_CLIPBOARD_MANAGER"
            || mimeType == "TIMESTAMP") {
            tmpList.append(mimeType);
        }
    }

    return tmpList;
}
