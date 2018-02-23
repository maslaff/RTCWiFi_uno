#ifndef DS2413ctl_h
#define DS2413ctl_h

#include <Arduino.h>
#include <OneWire.h>

#define DS2413_FAMILY_CODE 0x3A

#define DS2413_ACCESS_READ 0xF5
#define DS2413_ACCESS_WRITE 0x5A
#define DS2413_ACK_SUCCESS 0xAA
#define DS2413_ACK_ERROR 0xFF

typedef uint8_t swchAdr[8];

class DS2413ctl {
public:
  DS2413ctl(OneWire *);
  void begin(void);
  uint8_t getDeviceCount(void);
  bool getAddress(uint8_t *adrArr, uint8_t index);
  void printAdr(uint8_t *addr);
  uint8_t spio(uint8_t dta);
  uint8_t printStates(uint8_t *devAdr);
  void swch(uint8_t *devAdr, uint8_t chnl);
  void swOn(uint8_t *devAdr, uint8_t chnl);
  void swAlOn(uint8_t *devAdr);
  void swOff(uint8_t *devAdr, uint8_t chnl);
  void swAlOff(uint8_t *devAdr);
  void swchTo(uint8_t *devAdr, uint8_t chnl, bool cond);
  uint8_t devices;
  uint8_t states;

private:
  bool validAddress(const uint8_t *devAdr);
  bool write(uint8_t *devAdr, uint8_t state);
  uint8_t read(uint8_t *devAdr);
  OneWire *_ow;
};

#endif
