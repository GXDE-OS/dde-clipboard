#ifndef DATACONTROLDEVICEMANAGER_H
#define DATACONTROLDEVICEMANAGER_H

#include <QObject>

struct zwlr_data_control_manager_v1;
struct wl_seat;

namespace KWayland
{
namespace Client
{
class EventQueue;
class DataControlSourceV1;
class DataControlDeviceV1;

class DataControlDeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DataControlDeviceManager(QObject *parent = nullptr);
    ~DataControlDeviceManager();

    void setup(zwlr_data_control_manager_v1 *manager);
    void release();
    bool isValid() const;

    DataControlDeviceV1 *getDataDevice(wl_seat *seat, QObject *parent = nullptr);
    DataControlSourceV1 *createDataSource(QObject *parent = nullptr);

Q_SIGNALS:
    void removed();

private:
    zwlr_data_control_manager_v1 *m_manager;
};

}
}

#endif
