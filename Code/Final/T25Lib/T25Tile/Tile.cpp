#include "Arduino.h"
#include "Tile.h"
#include "Constants.h"
#include "PinConfig.h"
#include "Colors.h"
// #include <Wire.h>

Tile::Tile(uint8_t addr) {
  // initialize operationMode
  operationMode = SCROLL_MODE;

  //SET ADDRESS
  data.addr = addr;

  // SET FRAME RATE
  frameRate = 1;

  // SENSOR SETUP
  sensorRow = 0;
  sensorCol = 0;
  for (uint16_t i = 0; i < MATRIX_WIDTH*MATRIX_HEIGHT; ++i) {
    sensorData[i] = 0;
  }

  //initialize the matrix
  matrix = new Adafruit_DotStarMatrix(
    MATRIX_WIDTH, 
    MATRIX_HEIGHT, 
    TILES_X, 
    TILES_Y,
    MATRIX_DATA_PIN, 
    MATRIX_CLK_PIN, 
    DS_MATRIX_BOTTOM     + DS_MATRIX_LEFT +
    DS_MATRIX_COLUMNS + DS_MATRIX_ZIGZAG + DS_TILE_PROGRESSIVE,
    DOTSTAR_BGR
  );
}

void Tile::beginTile() {
  cursor.x = 0;
  cursor.y = 0;

  // DotStar Setup
  matrix->begin(); // Initialize pins for output
  matrix->setBrightness(64); // Set max brightness (out of 255) 
  matrix->setTextWrap(false);
  matrix->setTextColor(colors[0]);
  matrix->show();  // Turn all LEDs off ASAP

  // Directional Pin Setup
  pinMode(PIN_DIR_U, INPUT_PULLDOWN);
  pinMode(PIN_DIR_D, INPUT_PULLDOWN);
  pinMode(PIN_DIR_L, INPUT_PULLDOWN);
  pinMode(PIN_DIR_R, INPUT_PULLDOWN);
  // pinMode(PA2, OUTPUT);
}

/*
setCursor - sets the cursor relative to the top left of the tile matrix
  Inputs:
    x - the horizontal offset from the left edge of the tile matrix
    y - the veritcal offset from the top edge of the tile matrix
*/
void Tile::setCursor(int8_t x, int8_t y) {
  cursor.x = x;
  cursor.y = y;
}

/*
setOperationMode - sets the cursor relative to the top left of the tile matrix
  Inputs:
    mode - the horizontal offset from the left edge of the tile matrix
  Outputs:
    void
*/
void Tile::setOperationMode(const uint8_t mode) {
  operationMode = mode;
}

/*
getOperationMode - returns the current operation mode
  Inputs:
    void
  Outputs:
    operationMode - the currently set operationMode
*/
uint8_t Tile::getOperationMode() {
  return operationMode;
}

/*
getData - retrieves the Tile's status, address, position, and neighborTiles
  Outputs:
    TILE - a struct describing the tile data
*/
struct TILE Tile::getData() {
  return data;
}

/*
updateTileDisplay - updates the display based on current operation mode
*/
void Tile::updateTileDisplay(const POS &outPos, char dataOut[]) {
    switch(operationMode) {
      case (SCROLL_MODE):
        displayChar(outPos, dataOut);
        break;
      case (GESTURE_MODE):
        // TBD
        break;
      case (DIRECTION_TEST):
        // i2cDirectionTest(colors[data.color]);
        break;
    }
}

/*
displayChar - Shows the visible portion of characters on the matrix
  Inputs:
    dataOut - char array of characters to be displayed
  Outputs:
    void
*/
void Tile::displayChar(const POS &pos, char dataOut[]){
  matrix->fillScreen(0);
  matrix->setCursor(pos.x, pos.y);
  for(int i = 0; i < MAX_DISPLAY_CHARS; ++i){ //For 4x4 should be 2
    matrix->print(dataOut[i]);
  }
  matrix->show();
}

/*
i2cDirectionTest - test to show direction of tiles added
  Inputs:

*/
void Tile::i2cDirectionTest(const uint16_t color) {
    matrix->fillScreen(0);
    if((data.ports & CNCT_U) == CNCT_U){
      matrix->fillRect(1, 3, 2, 1, color);
    }
    if((data.ports & CNCT_D) == CNCT_D){
      matrix->fillRect(1, 0, 2, 1, color);
    }
    if((data.ports & CNCT_L) == CNCT_L){
      matrix->fillRect(0, 1, 1, 2, color);
    }
    if((data.ports & CNCT_R) == CNCT_R){
      matrix->fillRect(3, 1, 1, 2, color);
    }
    matrix->fillRect(1, 1, 2, 2, colors[WHITE]);
    matrix->show();
}

/*
findNeighborTiles - checks each port of the tile for a neighbor and updates the Tile's data
  Outputs:
    struct TILE - data including updated ports of the current tile and previous ports of current tile
*/
// void getOccupiedDirections() {  
struct TILE Tile::findNeighborTiles() {  
  // remember previous ports
  data.previousPorts = data.ports;

  // get current ports
  data.ports = 0b0000;
  if(digitalRead(PIN_DIR_U)){
    data.ports = data.ports | CNCT_U;
  }
  if(digitalRead(PIN_DIR_D)){
    data.ports = data.ports | CNCT_D;
  }
  if(digitalRead(PIN_DIR_L)){
    data.ports = data.ports | CNCT_L;
  }
  if(digitalRead(PIN_DIR_R)){
    data.ports = data.ports | CNCT_R;
  }

  return data;
}

/*
debugWithMatrix - Displays single pixel on tile matrix corresponding to some error code
  Inputs:
    x - horizontal offset from left edge of tile matrix
    y - veritcal offset from top edge of tile matrix
    color - predefined color code value
  Outputs:
    void
*/
void Tile::debugWithMatrix(const uint8_t x, const uint8_t y, const uint8_t color) {
  matrix->fillScreen(0);
  matrix->fillRect(x, 1, y, 1, colors[color]);
  matrix->show();
  delay(250);
}

/*
changeColor - a method to change the color of the matrix
  Inputs:
    colors - an array containing the values for red, green, blue from 0-255
  Outputs:
    void
*/
void Tile::changeColor(uint8_t colors[]) {
    matrix->setCursor(0, 0);
    matrix->setTextColor(makeColor(colors[0], colors[1], colors[2]));
    matrix->show();
}

/*
changeColor - a method to change the color of the matrix
  Inputs:
    color - a 2byte color
  Outputs:
    void
*/
void Tile::changeColor(uint16_t color) {
    matrix->setCursor(0, 0);
    matrix->setTextColor(color);
    matrix->show();
}

/*
printSensorData - outputs the current sensor data to serial
  Inputs:
    void
  Outputs:
    void
*/
void Tile::printSensorData() {
    Serial.write(27);       // ESC command
  Serial.print("[2J");    // clear screen command
  Serial.write(27);
  Serial.print("[H");     // cursor to home command

  for (uint8_t i = 0; i < MATRIX_WIDTH; ++i) {
    for(uint8_t j = 0; j < MATRIX_HEIGHT; ++j) {
      Serial.print(sensorData[i + j*(MATRIX_HEIGHT)]);
      if(j < MATRIX_HEIGHT - 1)
        Serial.print(" | ");
      else
        Serial.println();
    }
  }
  Serial.println();
}

/*
readSensorData - 
*/
void Tile::readSensorData() {
  if (sensorID != prevSensorID) {
    // read sensor data, pin map should be 0-7 for A0-A7 so we use sensorCol

    sensorData[sensorCol + sensorRow*MATRIX_WIDTH] = analogRead(sensorCol);

    // turn on next emitter
    ++sensorCol;
    if (sensorCol > MATRIX_HEIGHT - 1) {
      sensorCol = 0;
         ++sensorRow;
      if (sensorRow > MATRIX_WIDTH - 1) {
          sensorRow = 0;
      }
    }

    for (uint8_t i = 0; i < 3; ++i) {
      if ((sensorRow >> i) & 1) {
        gpio_write_bit(GPIOB, MUX_ROW_SELECT[i], LOW);
      } else {
        gpio_write_bit(GPIOB, MUX_ROW_SELECT[i], HIGH);
      }
      
      if ((sensorCol >> i) & 1) {
        gpio_write_bit(GPIOB, MUX_COL_SELECT[i], LOW);
      } else {
        gpio_write_bit(GPIOB, MUX_COL_SELECT[i], HIGH);
      }
    }

    if (DEBUG) {
      printSensorData();
    }
  }
}

/*

*/
void Tile::ISR_sensorRead() {
  prevSensorID = sensorID;
  ++sensorID;
  if (sensorID > MATRIX_WIDTH * MATRIX_HEIGHT) {
    sensorID = 0;
  }
}