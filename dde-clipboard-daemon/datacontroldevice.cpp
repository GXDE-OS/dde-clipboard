#include "datacontroldevice.h"
#include "datacontroloffer.h"
#include "datacontrolsource.h"
#include "data-control-client-protocol.h"

namespace KWayland
{
namespace Client
{

DataControlDeviceV1::DataControlDeviceV1(zwlr_data_control_device_v1 *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{
    static zwlr_data_control_device_v1_listener s_listener = {
        .data_offer = DataControlDeviceV1::listener_data_offer,
        .selection = DataControlDeviceV1::listener_selection,
        .finished = DataControlDeviceV1::listener_finished
    };
    if (m_device) {
        zwlr_data_control_device_v1_add_listener(m_device, &s_listener, this);
    }
}

DataControlDeviceV1::~DataControlDeviceV1()
{
}

void DataControlDeviceV1::setSelection(quint32 serial, DataControlSourceV1 *source)
{
    if (m_device && source) {
        zwlr_data_control_device_v1_set_selection(m_device, source->raw());
    }
}

void DataControlDeviceV1::destroy()
{
    if (m_device) {
        zwlr_data_control_device_v1_destroy(m_device);
        m_device = nullptr;
    }
}

void DataControlDeviceV1::listener_data_offer(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *id)
{
    Q_UNUSED(device);
    auto self = static_cast<DataControlDeviceV1 *>(data);
    auto offer = new DataControlOfferV1(id, self);
    Q_EMIT self->dataOffered(offer);
}

void DataControlDeviceV1::listener_selection(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *id)
{
    Q_UNUSED(device);
    auto self = static_cast<DataControlDeviceV1 *>(data);
    if (!id) {
        Q_EMIT self->selectionCleared();
    } else {
        auto offer = new DataControlOfferV1(id, self);
        Q_EMIT self->dataOffered(offer);
    }
}

void DataControlDeviceV1::listener_finished(void *data, zwlr_data_control_device_v1 *device)
{
    Q_UNUSED(data);
    Q_UNUSED(device);
}

}
}
