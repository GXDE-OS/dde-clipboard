#include "datacontroloffer.h"
#include "data-control-client-protocol.h"

namespace KWayland
{
namespace Client
{

DataControlOfferV1::DataControlOfferV1(zwlr_data_control_offer_v1 *offer, QObject *parent)
    : QObject(parent)
    , m_offer(offer)
{
    static zwlr_data_control_offer_v1_listener s_listener = {
        .offer = DataControlOfferV1::listener_offer
    };
    if (m_offer) {
        zwlr_data_control_offer_v1_add_listener(m_offer, &s_listener, this);
    }
}

DataControlOfferV1::~DataControlOfferV1()
{
    if (m_offer) {
        zwlr_data_control_offer_v1_destroy(m_offer);
    }
}

bool DataControlOfferV1::isValid() const
{
    return m_offer != nullptr;
}

QStringList DataControlOfferV1::offeredMimeTypes() const
{
    return m_mimeTypes;
}

void DataControlOfferV1::receive(const QString &mimeType, int32_t fd)
{
    if (m_offer) {
        zwlr_data_control_offer_v1_receive(m_offer, mimeType.toUtf8().constData(), fd);
    }
}

void DataControlOfferV1::listener_offer(void *data, zwlr_data_control_offer_v1 *offer, const char *mimeType)
{
    Q_UNUSED(offer);
    auto self = static_cast<DataControlOfferV1 *>(data);
    self->m_mimeTypes.append(QString::fromUtf8(mimeType));
    Q_EMIT self->mimeTypeOffered(QString::fromUtf8(mimeType));
}

}
}
