#ifndef DATACONTROLOFFER_H
#define DATACONTROLOFFER_H

#include <QObject>
#include <QStringList>

struct zwlr_data_control_offer_v1;

namespace KWayland
{
namespace Client
{

class DataControlOfferV1 : public QObject
{
    Q_OBJECT
public:
    explicit DataControlOfferV1(zwlr_data_control_offer_v1 *offer, QObject *parent = nullptr);
    ~DataControlOfferV1();

    bool isValid() const;
    QStringList offeredMimeTypes() const;
    void receive(const QString &mimeType, int32_t fd);

Q_SIGNALS:
    void mimeTypeOffered(const QString &mimeType);

private:
    static void listener_offer(void *data, zwlr_data_control_offer_v1 *offer, const char *mimeType);

    zwlr_data_control_offer_v1 *m_offer;
    QStringList m_mimeTypes;
};

}
}

#endif
