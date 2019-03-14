#ifndef t25_tile_h
#define t25_tile_h

#include "Arduino.h"
#include <Adafruit_GFX.h>
#include <Adafruit_DotStarMatrix.h>
#include <Adafruit_DotStar.h>

struct POS {
  int8_t x;
  int8_t y;
};

/* Tile structure
 * active = is this tile currently active
 * addr   = address of the current tile
 * posX   = position of tile in the X direction
 * posY   = position of tile in the Y direction
 * ports  = state of the directional pins
 * previousPorts = state of the directional pins in the previous instance 
*/

struct TILE {
  bool  active;
  int   addr;
  POS   pos;
  int   ports;
  int   previousPorts;
};

class Tile {
  public:
    Tile(uint8_t addr);
    ~Tile();

    char msgBuffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    void setCursor(int8_t x, int8_t y);
    void setOperationMode(const uint8_t mode);
    uint8_t getOperationMode();
    struct TILE getData();

    struct TILE findNeighborTiles();
    void debugWithMatrix(const uint8_t x, const uint8_t y, const uint8_t color);
   
    void updateTileDisplay(const uint8_t i, char dataOut[]);

  protected:
    uint8_t operationMode;

    Adafruit_DotStarMatrix *matrix;

    struct TILE data;
    struct POS cursor;

    void displayChar(char dataOut[]);
    void i2cDirectionTest(const uint16_t color)
};

#endif
