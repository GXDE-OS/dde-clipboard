#include "datacontroldevicemanager.h"
#include "datacontroldevice.h"
#include "datacontrolsource.h"
#include "data-control-client-protocol.h"
#include <wayland-client.h>

namespace KWayland
{
namespace Client
{

DataControlDeviceManager::DataControlDeviceManager(QObject *parent)
    : QObject(parent)
    , m_manager(nullptr)
{
}

DataControlDeviceManager::~DataControlDeviceManager()
{
}

void DataControlDeviceManager::setup(zwlr_data_control_manager_v1 *manager)
{
    m_manager = manager;
}

void DataControlDeviceManager::release()
{
    if (m_manager) {
        zwlr_data_control_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
}

bool DataControlDeviceManager::isValid() const
{
    return m_manager != nullptr;
}

DataControlDeviceV1 *DataControlDeviceManager::getDataDevice(wl_seat *seat, QObject *parent)
{
    if (!m_manager || !seat) return nullptr;
    auto device = zwlr_data_control_manager_v1_get_data_device(m_manager, seat);
    return new DataControlDeviceV1(device, parent);
}

DataControlSourceV1 *DataControlDeviceManager::createDataSource(QObject *parent)
{
    if (!m_manager) return nullptr;
    auto source = zwlr_data_control_manager_v1_create_data_source(m_manager);
    return new DataControlSourceV1(source, parent);
}

}
}
