#ifndef DATACONTROLSOURCE_H
#define DATACONTROLSOURCE_H

#include <QObject>
#include <QString>

struct zwlr_data_control_source_v1;

namespace KWayland
{
namespace Client
{

class DataControlSourceV1 : public QObject
{
    Q_OBJECT
public:
    explicit DataControlSourceV1(zwlr_data_control_source_v1 *source, QObject *parent = nullptr);
    ~DataControlSourceV1();

    void offer(const QString &mimeType);
    zwlr_data_control_source_v1 *raw() const { return m_source; }

Q_SIGNALS:
    void sendDataRequested(const QString &mimeType, qint32 fd);

private:
    static void listener_send(void *data, zwlr_data_control_source_v1 *source, const char *mimeType, int32_t fd);

    zwlr_data_control_source_v1 *m_source;
};

}
}

#endif
