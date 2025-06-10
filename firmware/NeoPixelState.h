#include "config.h"

class NeoPixelState {
public:
  uint32_t _pixels[NUMSTEPS];
  bool _hasChanged = false;

  void clear(){
    for(auto i = 0; i < NUMSTEPS; i++){
      _pixels[i] = 0;
    }
    _hasChanged = true;
  }
  void setColor(int index, uint32_t color) {
    if (color == _pixels[index]) {
      return;
    }
    _pixels[index] = color;
    _hasChanged = true;
  }
};
