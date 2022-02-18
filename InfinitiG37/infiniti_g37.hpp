#include <QString>
#include <iostream>
#include <stdlib.h>
#include <QByteArray>
#include <boost/log/trivial.hpp>


#include "plugins/vehicle_plugin.hpp"
#include "app/widgets/climate.hpp"
#include "app/widgets/vehicle.hpp"
#include "app/arbiter.hpp"
#include "openauto/Service/InputService.hpp"
#include "AAHandler.hpp"


#define G37_LOG(severity) BOOST_LOG_TRIVIAL(severity) << "[G37VehiclePlugin] "


class DebugWindow : public QWidget {
    Q_OBJECT

    public:
        DebugWindow(Arbiter &arbiter, QWidget *parent = nullptr);
        QLabel* tpmsOne;
        QLabel* tpmsTwo;
        QLabel* tpmsThree;
        QLabel* tpmsFour;

};

class InfinitiG37 : public QObject, VehiclePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID VehiclePlugin_iid)
    Q_INTERFACES(VehiclePlugin)

    public:
        InfinitiG37() {};
        ~InfinitiG37();
        bool init(ICANBus* canbus) override;
        QList<QWidget *> widgets() override;

    private:

        bool duelClimate;

        bool frDoor = false;
        bool flDoor = false;
        bool rrDoor = false;
        bool rlDoor = false;

        bool lTurn = false;
        bool rTurn = false;

        bool brakePedal = false;

        uint8_t frPressure = 0;
        uint8_t flPressure = 0;
        uint8_t rrPressure = 0;
        uint8_t rlPressure = 0;

        void monitorHeadlightStatus(QByteArray payload);
        void updateClimateDisplay(QByteArray payload);
        void updateTemperatureDisplay(QByteArray payload);
        void engineUpdate(QByteArray payload);
        void tpmsUpdate(QByteArray payload);
        void brakePedalUpdate(QByteArray payload);
        void steeringWheelUpdate(QByteArray payload);


        Climate *climate;
        Vehicle *vehicle;
        AAHandler *aa_handler;
        DebugWindow *debug;
        bool engineRunning = false;
};