#ifndef DATACONTROLDEVICE_H
#define DATACONTROLDEVICE_H

#include <QObject>

struct zwlr_data_control_device_v1;
struct zwlr_data_control_offer_v1;

namespace KWayland
{
namespace Client
{
class DataControlOfferV1;
class DataControlSourceV1;

class DataControlDeviceV1 : public QObject
{
    Q_OBJECT
public:
    explicit DataControlDeviceV1(zwlr_data_control_device_v1 *device, QObject *parent = nullptr);
    ~DataControlDeviceV1();

    void setSelection(quint32 serial, DataControlSourceV1 *source);
    void destroy();

Q_SIGNALS:
    void dataOffered(DataControlOfferV1 *offer);
    void selectionCleared();

private:
    static void listener_data_offer(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *id);
    static void listener_selection(void *data, zwlr_data_control_device_v1 *device, zwlr_data_control_offer_v1 *id);
    static void listener_finished(void *data, zwlr_data_control_device_v1 *device);

    zwlr_data_control_device_v1 *m_device;
};

}
}

#endif
