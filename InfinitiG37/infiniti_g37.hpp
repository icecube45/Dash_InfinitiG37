#include <QString>
#include <iostream>
#include <stdlib.h>
#include <QByteArray>
#include <boost/log/trivial.hpp>


#include "plugins/vehicle_plugin.hpp"
#include "app/widgets/climate.hpp"
#include "app/arbiter.hpp"
#include "openauto/Service/InputService.hpp"


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
        bool init(ICANBus* canbus) override;

    private:
        QList<QWidget *> widgets() override;

        bool duelClimate;

        void monitorHeadlightStatus(QByteArray payload);
        void updateClimateDisplay(QByteArray payload);
        void updateTemperatureDisplay(QByteArray payload);
        void engineUpdate(QByteArray payload);
        void tpmsUpdate(QByteArray payload);



        Climate *climate;
        DebugWindow *debug;
        bool engineRunning = false;
};
