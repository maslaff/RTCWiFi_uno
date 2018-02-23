#include <Arduino.h>
#include <DS2413ctl.h>
#include <DallasTemperature.h>
#include <EEPROMex.h>
#include <OneWire.h>
#include <RtcDS3231.h>
#include <Stream.h>
#include <Wire.h>

OneWire ow(8);
DallasTemperature th(&ow);
DS2413ctl sw(&ow);
RtcDS3231<TwoWire> Rtc(Wire);

#define ADTLOAD_AFTER_S 10
#define ADTLOAD_BEFOR_S 5

#define MAX_NUM_SWITCH 8
#define MAX_NUM_THERM 8

#define PIN_OUT_LOAD 7
#define PIN_OUT_INDI 13

bool comm = false;
bool intFlg;
uint8_t nsw;
uint8_t nth;
int eeAddrAlarm;

const char *STATES[2] = {"of", "on"};
enum lightStateEnum { nig, day };
enum stateEnum { off, onn };
enum typechr { lit, dig, ld, non };

struct timeStruct {
  uint8_t h;
  uint8_t m;
};

struct alarmStruct {
  timeStruct of;
  timeStruct on;
};
alarmStruct alarm;

struct adtL {
  uint8_t after;
  uint8_t before;
};
adtL adtLoad = {ADTLOAD_AFTER_S, ADTLOAD_BEFOR_S};

struct therm {
  DeviceAddress adr;
  float tmp;
};
therm therms[MAX_NUM_THERM];

struct key {
  swchAdr adr;
  bool state;
};
key keys[MAX_NUM_SWITCH];

void Rtc_Setup();
void Sw_Setup();
void Th_Setup();
void Comm_Setup();
void AlarmSet(timeStruct &);
bool AlarmReSet();

void onNewSecond();
void onSerial();
void onMainAlarm();

void dt2str(String &, const RtcDateTime &);
void owAdr2str(String &, uint8_t *);
String strSwAddrs();
String strSwStates();
String strThAddrs();
String strThTemps();
String strDT();
String strAlarm();
bool searchIn(String &, uint8_t);
bool getTimeFromStr(String &, uint8_t &, uint8_t &);
bool getAlarmsFromStr(String &, alarmStruct &);

#define countof(a) (sizeof(a) / sizeof(a[0]))

void InterruptServiceRoutine() { intFlg = true; }

void setup() {
  Serial.begin(115200);
  pinMode(PIN_OUT_LOAD, OUTPUT);
  pinMode(PIN_OUT_INDI, OUTPUT);

  EEPROM.setMemPool(32, EEPROMSizeUno);
  EEPROM.setMaxAllowedWrites(20);
  eeAddrAlarm = EEPROM.getAddress(sizeof(alarmStruct));
  EEPROM.readBlock(eeAddrAlarm, alarm);

  Sw_Setup();
  Th_Setup();

  pinMode(3, INPUT);
  Rtc_Setup();
  digitalWrite(PIN_OUT_LOAD, AlarmReSet());
  Rtc.LatchAlarmsTriggeredFlags();
  intFlg = false;
  attachInterrupt(1, InterruptServiceRoutine, FALLING);
}

void loop() {
  if (intFlg) {
    intFlg = false;
    DS3231AlarmFlag flag = Rtc.LatchAlarmsTriggeredFlags();
    if (flag & DS3231AlarmFlag_Alarm1) { onNewSecond(); }
    if (flag & DS3231AlarmFlag_Alarm2) { onMainAlarm(); }
  }
  if (Serial.available()) onSerial();
}

void onSerial() {
  if (!comm) {
    Comm_Setup();
    return;
  }
  char buf[255];
  Serial.readBytesUntil('\n', buf, 255);
  String instring = String(buf);
  instring.trim();
  searchIn(instring, lit);

  switch (instring[0]) {
    case 'g':
      switch (instring[1]) {
        case 'c': Serial.print(strDT()); break;
        case 'a': Serial.print(strAlarm()); break;
        case 't':
          if (instring[2] == 'a') {
            Serial.print(strThAddrs());
            break;
          }
          Serial.print(strThTemps());
          break;
        case 's':
          if (instring[2] == 'a') {
            Serial.print(strSwAddrs());
            break;
          }
          Serial.print(strSwStates());
          break;
      }
      break;

    case 's': {
      char ch = instring[1];
      searchIn(instring, ld);
      switch (ch) {
        case 'a': {
          getAlarmsFromStr(instring, alarm);
          digitalWrite(PIN_OUT_LOAD, AlarmReSet());
          EEPROM.updateBlock(eeAddrAlarm, alarm);
          Serial.println(strAlarm());
          break;
        }
        case 'c': {
          uint8_t _h, _m;
          RtcDateTime now = Rtc.GetDateTime();
          getTimeFromStr(instring, _h, _m);
          RtcDateTime newTime =
              RtcDateTime(now.Year(), now.Month(), now.Day(), _h, _m, 0);
          Rtc.SetDateTime(newTime);
          digitalWrite(PIN_OUT_LOAD, AlarmReSet());
          Serial.println(strDT());
          break;
        }
        case 't': break;
      }
      break;
    }
    default: break;
  }
}

void onNewSecond() {
  if (!comm) { digitalWrite(PIN_OUT_INDI, !digitalRead(PIN_OUT_INDI)); }
  for (size_t i = 0; i < nth; i++) {
    therms[i].tmp = th.getTempC(therms[i].adr);
  }
  th.requestTemperatures();
}

void onMainAlarm() {
  bool nfs = AlarmReSet();
  // sw.swchTo(keys[0].adr, 0, nfs);
  digitalWrite(PIN_OUT_LOAD, nfs);
  Serial.println(nfs ? "D" : "N");
}

String strSwStates() {
  String nminstr = String("ss ");
  if (nsw) {
    for (size_t i = 0; i < nsw; i++) {
      nminstr += keys[i].state;
      nminstr += ';';
    }
  } else
    nminstr += "-";
  return nminstr + '\n';
}

String strThTemps() {
  String nminstr = String("tt ");
  if (nth) {
    for (size_t i = 0; i < nth; i++) {
      nminstr += therms[i].tmp;
      nminstr += ';';
    }
  } else
    nminstr += '-';
  return nminstr + '\n';
}

String strSwAddrs() {
  String comstr = String("sa\n");
  if (nsw) {
    for (size_t i = 0; i < nsw; i++) {
      owAdr2str(comstr, keys[i].adr);
      comstr += '\n';
    }
  } else
    comstr += "none\n";
  return comstr;
}

String strThAddrs() {
  String comstr = String("ta\n");
  if (nth) {
    for (size_t i = 0; i < nth; i++) {
      owAdr2str(comstr, therms[i].adr);
      comstr += '\n';
    }
  } else
    comstr += "none\n";
  return comstr;
}

String strAlarm() {
  char datestring[20];
  snprintf_P(datestring, countof(datestring), PSTR("%02u:%02u/%02u:%02u"),
             alarm.on.h, alarm.on.m, alarm.of.h, alarm.of.m);
  return String(datestring) + '\n';
}

String strDT() {
  String dt = String();
  dt2str(dt, Rtc.GetDateTime());
  return dt + '\n';
}

String strReport() {
  String comstr = String("");
  comstr += strSwAddrs();
  comstr += strThAddrs();
  comstr += strDT();
  comstr += strAlarm();
  comstr += adtLoad.after;
  comstr += ' ';
  comstr += adtLoad.before;
  comstr += '\n';
  return comstr;
}

void Rtc_Setup() {
  Rtc.Begin();
  RtcDateTime now = Rtc.GetDateTime();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid()) Rtc.SetDateTime(compiled);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
  if (now < compiled) Rtc.SetDateTime(compiled);

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmBoth);

  DS3231AlarmOne alarm1(0, 0, 0, 0, DS3231AlarmOneControl_OncePerSecond);
  Rtc.SetAlarmOne(alarm1);
}

void Sw_Setup() {
  sw.begin();
  nsw = sw.getDeviceCount();
  if (nsw) {
    if (nsw > MAX_NUM_SWITCH) nsw = MAX_NUM_SWITCH;
    for (size_t i = 0; i < nsw; i++) { sw.getAddress(keys[i].adr, i); }
  }
}

void Th_Setup() {
  th.begin();
  nth = th.getDeviceCount();
  if (nth) {
    if (nth > MAX_NUM_THERM) nth = MAX_NUM_THERM;
    for (size_t i = 0; i < nth; i++) {
      th.getAddress(therms[i].adr, i);
      th.setResolution(therms[i].adr, 12);
    }
  }
  th.setResolution(9);
  th.setWaitForConversion(false);
  th.requestTemperatures();
}

void Comm_Setup() {
  comm = false;
  if (!Serial.available()) return;
  Serial.setTimeout(3000);

  if (Serial.find((char *)"#esp")) {
    Serial.println("#uno");
    if (!Serial.find((char *)"ready")) return;

    Serial.print(strReport());
    Serial.println("end");

    if (Serial.find((char *)"ok")) comm = true;
    Serial.setTimeout(1000);
    digitalWrite(PIN_OUT_INDI, LOW);
  }  // else Serial.println("Not Setup");
}

void AlarmSet(timeStruct &t) {
  DS3231AlarmTwo lastAlarm = Rtc.GetAlarmTwo();
  if (lastAlarm.Hour() == t.h && lastAlarm.Minute() == t.m) return;
  DS3231AlarmTwo alarm_set(0, t.h, t.m,
                           DS3231AlarmTwoControl_HoursMinutesMatch);
  Rtc.SetAlarmTwo(alarm_set);
}

bool AlarmReSet() {
  uint8_t _h, _m, _anh, _anm, _afh, _afm;
  bool inv, flg;
  RtcDateTime now = Rtc.GetDateTime();
  _h = now.Hour();
  _m = now.Minute();
  inv = alarm.on.h > alarm.of.h;
  _anh = inv ? alarm.of.h : alarm.on.h;
  _anm = inv ? alarm.of.m : alarm.on.m;
  _afh = inv ? alarm.on.h : alarm.of.h;
  _afm = inv ? alarm.on.m : alarm.of.m;

  if (_h > _anh && _h < _afh) {
    flg = day;
  } else if (_h == _anh && _anh != _afh) {
    flg = (_m >= _anm) ? day : nig;
  } else if (_h == _afh) {
    flg = (_m < _afm) ? day : nig;
  } else {
    flg = nig;
  }

  if (inv) flg = !flg;

  AlarmSet(flg ? alarm.of : alarm.on);
  return flg;
}

bool getTimeFromStr(String &data, uint8_t &h, uint8_t &m) {
  int spr = data.indexOf(':');
  if (spr < 0) return false;
  uint8_t _h = (data.substring(spr - 2, spr)).toInt();
  uint8_t _m = (data.substring(spr + 1, spr + 3)).toInt();
  if (_h > 23 || _m > 59) return false;
  h = _h;
  m = _m;
  return true;
}

bool getAlarmsFromStr(String &data, alarmStruct &ala) {
  if (!getTimeFromStr(data, ala.on.h, ala.on.m)) return false;
  int spr = data.indexOf('/');
  if (spr < 0) return false;
  data = data.substring(spr + 1);
  if (!getTimeFromStr(data, ala.of.h, ala.of.m)) return false;
  return true;
}

void dt2str(String &tgstr, const RtcDateTime &dt) {
  char datestring[20];
  snprintf_P(datestring, countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"), dt.Day(), dt.Month(),
             dt.Year(), dt.Hour(), dt.Minute(), dt.Second());
  tgstr += datestring;
  //    Serial.print(datestring);
}

// void printOWAdr(uint8_t* addr) {
//   for (uint8_t n = 0; n < 8; n++) {
//     Serial.print(addr[n] >> 4, HEX);
//     Serial.print(addr[n] & 0x0f, HEX);
//   }
// }

void owAdr2str(String &trg, uint8_t *addr) {
  for (uint8_t n = 0; n < 8; n++) {
    trg += (addr[n] >> 4);
    trg += (addr[n] & 0x0f);
  }
}

bool searchIn(String &str, uint8_t sub) {
  uint8_t cur = 0;
  switch (sub) {
    case lit:
      while (!isAlpha(str[cur])) {
        cur++;
        if (cur > 230) return false;
      }
      break;

    case dig:
      while (!isDigit(str[cur])) {
        cur++;
        if (cur > 230) return false;
      }
      break;

    case ld:
      while (!isAlphaNumeric(str[cur])) {
        cur++;
        if (cur > 230) return false;
      }
      break;

    case non: break;

    default: return false;
  }
  str = str.substring(cur);
  return true;
}
