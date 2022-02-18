#include "infiniti_g37.hpp"

#define DEBUG false


InfinitiG37::~InfinitiG37()
{
    if (this->climate)
        delete this->climate;
    if (this->vehicle)
        delete this->vehicle;
}

bool InfinitiG37::init(ICANBus* canbus){
    this->duelClimate=false;
    if (this->arbiter) {
        this->aa_handler = this->arbiter->android_auto().handler;
        this->climate = new Climate(*this->arbiter);
        this->climate->max_fan_speed(7);
        this->climate->setObjectName("Climate");
        this->vehicle = new Vehicle(*this->arbiter);
        this->vehicle->setObjectName("Infiniti G37");
        this->vehicle->pressure_init("psi", 30);
        this->vehicle->disable_sensors();
        this->vehicle->rotate(270);

        if(DEBUG)
            this->debug = new DebugWindow(*this->arbiter);

        canbus->registerFrameHandler(0x60D, [this](QByteArray payload){this->monitorHeadlightStatus(payload);});
        canbus->registerFrameHandler(0x54B, [this](QByteArray payload){this->updateClimateDisplay(payload);});
        canbus->registerFrameHandler(0x542, [this](QByteArray payload){this->updateTemperatureDisplay(payload);});
        canbus->registerFrameHandler(0x551, [this](QByteArray payload){this->engineUpdate(payload);});
        canbus->registerFrameHandler(0x385, [this](QByteArray payload){this->tpmsUpdate(payload);});
        canbus->registerFrameHandler(0x354, [this](QByteArray payload){this->brakePedalUpdate(payload);});
        canbus->registerFrameHandler(0x002, [this](QByteArray payload){this->steeringWheelUpdate(payload);});
        G37_LOG(info)<<"loaded successfully";
        return true;
    }
    else{
        G37_LOG(error)<<"Failed to get arbiter";
        return false;
    }
    

}

QList<QWidget *> InfinitiG37::widgets()
{
    QList<QWidget *> tabs;
    tabs.append(this->vehicle);
    tabs.append(this->climate);
    if(DEBUG)
        tabs.append(this->debug);
    return tabs;
}


// TPMS
// 385
// THIRD BYTE:
//  Tire Pressure (PSI) * 4
// FOURTH BYTE:
//  Tire Pressure (PSI) * 4
// FIFTH BYTE:
//  Tire Pressure (PSI) * 4
// SIXTH BYTE:
//  Tire Pressure (PSI) * 4
// SEVENTH BYTE:
// |Tire 1 Pressure Valid|Tire 2 Pressure Valid|Tire 3 Pressure Valid|Tire 4 Pressure Valid|unknown|unknown|unknown|unknown

// OTHERS UNKNOWN

void InfinitiG37::tpmsUpdate(QByteArray payload){
    if(DEBUG){
        this->debug->tpmsOne->setText(QString::number((uint8_t)payload.at(2)/4));
        this->debug->tpmsTwo->setText(QString::number((uint8_t)payload.at(3)/4));
        this->debug->tpmsThree->setText(QString::number((uint8_t)payload.at(4)/4));
        this->debug->tpmsFour->setText(QString::number((uint8_t)payload.at(5)/4));
    }
    uint8_t newRrPressure = (uint8_t)payload.at(4)/4;
    uint8_t newRlPressure = (uint8_t)payload.at(5)/4;
    uint8_t newFrPressure = (uint8_t)payload.at(2)/4;
    uint8_t newFlPressure = (uint8_t)payload.at(3)/4;

    this->vehicle->pressure(Position::BACK_RIGHT,newRrPressure);
    this->vehicle->pressure(Position::BACK_LEFT, newRlPressure);
    this->vehicle->pressure(Position::FRONT_RIGHT,newFrPressure);
    this->vehicle->pressure(Position::FRONT_LEFT, newFlPressure);

}

//551
//(A, B, C, D, E, F, G, H)
// D - Bitfield.

//     0xA0 / 160 - Engine off, "On"
//     0x20 / 32 - Engine turning on (500ms)
//     0x00 / 0 - Engine turning on (2500ms)
//     0x80 / 128 - Engine running
//     0x00 / 0 - Engine shutting down (2500ms)
//     0x20 / 32 - Engine off


void InfinitiG37::engineUpdate(QByteArray payload){
    if((payload.at(3) == 0x80)) engineRunning = true;
    else
    {
        if(engineRunning)
            this->aa_handler->injectButtonPress(aasdk::proto::enums::ButtonCode::PAUSE);
        engineRunning = false;
    }
}


//002

void InfinitiG37::steeringWheelUpdate(QByteArray payload){
    uint16_t rawAngle = payload.at(1);
    rawAngle = rawAngle<<8;
    rawAngle |= payload.at(0);
    int degAngle = 0;
    if(rawAngle>32767) degAngle = -((65535-rawAngle)/10);
    else degAngle = rawAngle/10;
    degAngle = degAngle/16.4;
    this->vehicle->wheel_steer(degAngle);
    G37_LOG(info)<<"raw: "<<rawAngle<<" deg "<<degAngle;
}

//354
//(A, B, C, D, E, F, G, H)
// G - brake pedal
//     4 - off
//     20 - pressed a bit

void InfinitiG37::brakePedalUpdate(QByteArray payload){
    bool brakePedalUpdate = false;
    if((payload.at(6) == 20)) brakePedalUpdate = true;
    // if(brakePedalUpdate != this->brakePedal){
        // this->brakePedal = brakePedalUpdate;
        this->vehicle->taillights(brakePedalUpdate);
    // }
   
}

// HEADLIGHTS AND DOORS
// 60D
// FIRST BYTE:
// |unknown|RR_DOOR|RL_DOOR|FR_DOOR|FL_DOOR|SIDE_LIGHTS|HEADLIGHTS|unknown|
// SECOND BYTE:
// |unknown|unknown|unknown|unknown|unknown|left turn signal light|right turn signal light|FOGLIGHTS|
// OTHERS UNKNOWN

void InfinitiG37::monitorHeadlightStatus(QByteArray payload){
    if((payload.at(0)>>1) & 1){
        //headlights are ON - turn to dark mode
        if(this->arbiter->theme().mode == Session::Theme::Light){
            this->arbiter->set_mode(Session::Theme::Dark);
        this->vehicle->headlights(true);
        }
    }
    else{
        //headlights are off or not fully on (i.e. sidelights only) - make sure is light mode
        if(this->arbiter->theme().mode == Session::Theme::Dark){
            this->arbiter->set_mode(Session::Theme::Light);
        this->vehicle->headlights(false);
        }
    }
    bool rrDoorUpdate = (payload.at(0) >> 6) & 1;
    bool rlDoorUpdate = (payload.at(0) >> 5) & 1;
    bool frDoorUpdate = (payload.at(0) >> 4) & 1;
    bool flDoorUpdate = (payload.at(0) >> 3) & 1;
    this->vehicle->door(Position::BACK_RIGHT, rrDoorUpdate);
    this->vehicle->door(Position::BACK_LEFT, rlDoorUpdate);
    this->vehicle->door(Position::FRONT_RIGHT, frDoorUpdate);
    this->vehicle->door(Position::FRONT_LEFT, flDoorUpdate);

    bool rTurnUpdate = (payload.at(1)>>6) & 1;
    bool lTurnUpdate = (payload.at(1)>>5) & 1;
    this->vehicle->indicators(Position::LEFT, lTurnUpdate);
    this->vehicle->indicators(Position::RIGHT, rTurnUpdate);
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
        if(hvacOff){
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
    if(climate->left_temp()!=(unsigned char)payload.at(1))
        climate->left_temp((unsigned char)payload.at(1));
    if(duelClimate){
        if(climate->right_temp()!=(unsigned char)payload.at(2)){
            climate->right_temp((unsigned char)payload.at(2));
        }
    }else{
        if(climate->right_temp()!=(unsigned char)payload.at(1))
            climate->right_temp((unsigned char)payload.at(1));
    }
}




DebugWindow::DebugWindow(Arbiter &arbiter, QWidget *parent) : QWidget(parent)
{
    this->setObjectName("Debug");


    QLabel* textOne = new QLabel("Front Right PSI", this);
    QLabel* textTwo = new QLabel("Front Left PSI", this);
    QLabel* textThree = new QLabel("Rear Right PSI", this);
    QLabel* textFour = new QLabel("Rear Left PSI", this);

    tpmsOne = new QLabel("--", this);
    tpmsTwo = new QLabel("--", this);
    tpmsThree = new QLabel("--", this);
    tpmsFour = new QLabel("--", this);



    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(textOne);
    layout->addWidget(tpmsOne);
    layout->addWidget(Session::Forge::br(false));

    layout->addWidget(textTwo);
    layout->addWidget(tpmsTwo);
    layout->addWidget(Session::Forge::br(false));

    layout->addWidget(textThree);
    layout->addWidget(tpmsThree);
    layout->addWidget(Session::Forge::br(false));

    layout->addWidget(textFour); 
    layout->addWidget(tpmsFour);
    layout->addWidget(Session::Forge::br(false));




}
