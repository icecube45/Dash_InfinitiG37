#include "infiniti_g37.hpp"

Theme* InfinitiG37::theme = nullptr;
Climate* InfinitiG37::climate = nullptr;
bool InfinitiG37::duelClimate = false;


bool InfinitiG37::init(ICANBus* canbus){
    this->duelClimate=false;
    this->theme = Theme::get_instance();
    this->climate = new Climate();
    this->climate->max_fan_speed(7);
    this->climate->setObjectName("Climate");
    std::function<void(QByteArray)> headlightCallback = InfinitiG37::monitorHeadlightStatus;
    canbus->registerFrameHandler(0x60D, headlightCallback);
    std::function<void(QByteArray)> climateControlCallback = InfinitiG37::updateClimateDisplay;
    canbus->registerFrameHandler(0x54B, climateControlCallback);
    std::function<void(QByteArray)> temperatureControlCallback = InfinitiG37::updateTemperatureDisplay;
    canbus->registerFrameHandler(0x542, temperatureControlCallback);
    

    G37_LOG(info)<<"loaded successfully";
    return true;
}

QList<QWidget *> InfinitiG37::widgets()
{
    QList<QWidget *> tabs;
    tabs.append(this->climate);
    return tabs;
}

// HEADLIGHTS AND DOORS
// 60D
// FIRST BYTE:
// |unknown|RR_DOOR|RL_DOOR|FR_DOOR|FL_DOOR|SIDE_LIGHTS|HEADLIGHTS|unknown|
// SECOND BYTE:
// |unknown|unknown|unknown|unknown|unknown|unknown|unknown|FOGLIGHTS|
// OTHERS UNKNOWN

void InfinitiG37::monitorHeadlightStatus(QByteArray payload){
    if((payload.at(0)>>1) & 1){
        //headlights are ON - turn to dark mode
        if(theme->get_mode() != true){
            theme->set_mode(true);
            theme->update();
        }
    }
    else{
        //headlights are off or not fully on (i.e. sidelights only) - make sure is light mode
        if(theme->get_mode() != false){
            theme->set_mode(false);
            theme->update();
        }
    }
}

// HVAC
// 54B

// FIRST BYTE 
// |unknown|unknown|unknown|unknown|unknown|unknown|unknown|HVAC_OFF|
// SECOND BYTE
// |unknown|unknown|unknown|unknown|unknown|unknown|unknown|unknown|
// THIRD BYTE - MODE
// |unknown|unknown|MODE|MODE|MODE|unknown|unknown|unknown|
//   mode:
//    defrost+leg
//       1 0 0
//    head
//       0 0 1
//    head+feet
//       0 1 0
//    feet
//       0 1 1
//    defrost
//       1 0 1
// FOURTH BYTE
// |unknown|DUEL_CLIMATE_ON|DUEL_CLIMATE_ON|unknown|unknown|unknown|RECIRCULATE_OFF|RECIRCULATE_ON|
// Note both duel climate on bytes toggle to 1 when duel climate is on
// FIFTH BYTE - FAN LEVEL
// |unknown|unknown|FAN_1|FAN_2|FAN_3|unknown|unknown|unknown|
// FAN_1, FAN_2, FAN_3 scale linearly fan 0 (off) -> 7
//
// ALL OTHERS UNKNOWN

bool oldStatus = true;

void InfinitiG37::updateClimateDisplay(QByteArray payload){
    duelClimate = (payload.at(3)>>5) & 1;
    bool hvacOff = payload.at(0) & 1;
    if(hvacOff != oldStatus){
        oldStatus = hvacOff;
        if(hvacOFF){
            climate->airflow(Airflow::OFF);
            climate->fan_speed(0);
            G37_LOG(info)<<"Climate is off";
            return;
        }
    }
    uint8_t airflow = (payload.at(2) >> 3) & 0b111;
    uint8_t dash_airflow = 0;
    switch(airflow){
        case(1):
            dash_airflow = Airflow::BODY;
            break;
        case(2):
            dash_airflow = Airflow::BODY | Airflow::FEET;
            break;
        case(3):
            dash_airflow = Airflow::FEET;
            break;
        case(4):
            dash_airflow = Airflow::DEFROST | Airflow::FEET;
            break;
        case(5):
            dash_airflow = Airflow::DEFROST;
            break;
    }
    if(climate->airflow()!=dash_airflow)
        climate->airflow(dash_airflow);
    uint8_t fanLevel = (payload.at(4)>>3) & 0b111;
    if(climate->fan_speed()!=fanLevel)
        climate->fan_speed(fanLevel);
}

// Climate
// 542

// FIRST BYTE:
// unknown
// SECOND BYTE:
// entire byte is driver side temperature, 60F->90F
// THIRD BYTE:
// entire byte is passenger side temperature, 60F->90F
// note that this byte is only updated when duel climate is on. When duel climate is off, SECOND BYTE contains accurate passenger temperature.

void InfinitiG37::updateTemperatureDisplay(QByteArray payload){
    if(climate->driver_temp()!=(unsigned char)payload.at(1))
        climate->driver_temp((unsigned char)payload.at(1));
    if(duelClimate){
        if(climate->passenger_temp()!=(unsigned char)payload.at(2)){
            climate->passenger_temp((unsigned char)payload.at(2));
        }
    }else{
        if(climate->passenger_temp()!=(unsigned char)payload.at(1))
            climate->passenger_temp((unsigned char)payload.at(1));
    }
}

