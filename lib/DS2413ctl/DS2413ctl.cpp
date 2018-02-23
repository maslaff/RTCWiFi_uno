#include <Arduino.h>
#include <DS2413ctl.h>

DS2413ctl::DS2413ctl(OneWire *_onewire) { _ow = _onewire; }

void DS2413ctl::begin(void) {
  swchAdr devAdr;
  _ow->reset_search();
  devices = 0; // Reset the number of devices when we enumerate wire devices
  while (_ow->search(devAdr))
    if (validAddress(devAdr))
      devices++;
}

// returns the number of devices found on the bus
uint8_t DS2413ctl::getDeviceCount(void) { return devices; }

// returns true if address is valid
bool DS2413ctl::validAddress(const uint8_t *devAdr) {
  return (devAdr[0] == DS2413_FAMILY_CODE && _ow->crc8(devAdr, 7) == devAdr[7]);
}

// finds an address at a given index on the bus
// returns true if the device was found
bool DS2413ctl::getAddress(uint8_t *adrArr, uint8_t index) {
  uint8_t depth = 0;
  _ow->reset_search();
  while (_ow->search(adrArr)) {
    if (validAddress(adrArr)) {
      if (depth == index)
        return true;
      depth++;
    }
  }
  for (uint8_t i = 0; i < 8; i++)
    adrArr[i] = 0;
  return false;
}

// -= Method <printBytes> - used in ^setup^ for a outputing device addresses to
// Serial _
void DS2413ctl::printAdr(uint8_t *addr) {
  for (uint8_t i = 0; i < 8; i++) {
    Serial.print(addr[i] >> 4, HEX);
    Serial.print(addr[i] & 0x0f, HEX);
    Serial.print(" ");
  }
}

// -= Method <read> - return data received from a DS2413 _
uint8_t DS2413ctl::read(uint8_t *devAdr) {
  _ow->reset();
  _ow->select(devAdr);
  _ow->write(DS2413_ACCESS_READ);
  return _ow->read();
}

// -= Method <spio> - read
uint8_t DS2413ctl::spio(uint8_t dta) {
  bool ok = (~dta & 0x0F) == (dta >> 4);
  if (ok) {
    states = ((~dta >> 1) & 0x01) | ((~dta >> 2) & 0x02);
    Serial.print('S');
    Serial.print(states >> 1, BIN);
    Serial.println(states & 0x01, BIN);
    return states;
  } else {
    Serial.println("SEcrc");
    return 0xFF;
  }
}

// -=DS2413 Write new PIO value _
bool DS2413ctl::write(uint8_t *devAdr, uint8_t state) {
  bool ok = false;
  uint8_t ste = ~state | 0xFC;

  _ow->reset();
  _ow->select(devAdr);
  _ow->write(DS2413_ACCESS_WRITE);
  _ow->write(ste);
  _ow->write(~ste); /* Invert data and resend     */
  if (_ow->read() == DS2413_ACK_SUCCESS)
    ok = spio(_ow->read()) == state;
  _ow->reset();
  return ok;
}

uint8_t DS2413ctl::printStates(uint8_t *devAdr) { return spio(read(devAdr)); }
void DS2413ctl::swch(uint8_t *devAdr, uint8_t chnl) {
  write(devAdr, states ^ chnl);
}
void DS2413ctl::swOn(uint8_t *devAdr, uint8_t chnl) {
  write(devAdr, states | chnl);
}
void DS2413ctl::swAlOn(uint8_t *devAdr) { write(devAdr, 0x03); }
void DS2413ctl::swOff(uint8_t *devAdr, uint8_t chnl) {
  write(devAdr, states & ~chnl);
}
void DS2413ctl::swAlOff(uint8_t *devAdr) { write(devAdr, 0x00); }
void DS2413ctl::swchTo(uint8_t *devAdr, uint8_t chnl, bool cond) {
  cond ? swOn(devAdr, chnl) : swOff(devAdr, chnl);
}
