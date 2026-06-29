#include "datacontrolsource.h"
#include "data-control-client-protocol.h"

namespace KWayland
{
namespace Client
{

DataControlSourceV1::DataControlSourceV1(zwlr_data_control_source_v1 *source, QObject *parent)
    : QObject(parent)
    , m_source(source)
{
    static zwlr_data_control_source_v1_listener s_listener = {
        .send = DataControlSourceV1::listener_send
    };
    if (m_source) {
        zwlr_data_control_source_v1_add_listener(m_source, &s_listener, this);
    }
}

DataControlSourceV1::~DataControlSourceV1()
{
    if (m_source) {
        zwlr_data_control_source_v1_destroy(m_source);
    }
}

void DataControlSourceV1::offer(const QString &mimeType)
{
    if (m_source) {
        zwlr_data_control_source_v1_offer(m_source, mimeType.toUtf8().constData());
    }
}

void DataControlSourceV1::listener_send(void *data, zwlr_data_control_source_v1 *source, const char *mimeType, int32_t fd)
{
    Q_UNUSED(source);
    auto self = static_cast<DataControlSourceV1 *>(data);
    Q_EMIT self->sendDataRequested(QString::fromUtf8(mimeType), fd);
}

}
}
