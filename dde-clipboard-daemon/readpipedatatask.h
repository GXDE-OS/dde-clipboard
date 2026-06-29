// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef READ_PIPE_DATA_TASK_H
#define READ_PIPE_DATA_TASK_H

#include <QObject>

namespace KWayland
{
    namespace Client
    {
        class ConnectionThread;
        class DataControlOfferV1;
    } //Client
} //KWayland

using namespace KWayland::Client;

class ReadPipeDataTask : public QObject
{
    Q_OBJECT
public:
    explicit ReadPipeDataTask(ConnectionThread *connectionThread, DataControlOfferV1 *offerV1,
                              const QString &mimeType, QObject *parent = nullptr);

    void run();

signals:
    void dataReady(qint64, const QString &, const QByteArray &);
    void forceClosePipeLine();

private:
    bool readData(int fd, QByteArray &data);

private:
    QString m_mimeType;
    bool m_pipeIsForcedClosed;

    DataControlOfferV1 *m_pOffer;
    ConnectionThread *m_pConnectionThread;
};

#endif //READ_PIPE_DATA_TASK_H
