#include "Arduino.h"
#include "MasterTile.h"
#include "PinConfig.h"
#include "Constants.h"
#include "MatrixSetup.h"
#include "Colors.h"
#include <Wire.h>

MasterTile::MasterTile(uint8_t addr):Tile(addr) {
  tileID = 0;
  tileCount = 0;

  scrollPos.x = 0;
  scrollPos.y = 0;

  //Initialize the tileOrder
  resetTileOrder();

  // Initialize the tileMap
  tileMap.xMin = TILE_MAP_SIZE/2;
  tileMap.xMax = TILE_MAP_SIZE/2;
  tileMap.yMin = TILE_MAP_SIZE/2;
  tileMap.yMax = TILE_MAP_SIZE/2;
  for(uint8_t i = 0; i < TILE_MAP_SIZE; i++) {
    for(uint8_t j = 0; j < TILE_MAP_SIZE; j++) {
      tileMap.grid[i][j] = 0;
      // set center of map as MASTER_TILE_ID
      if ((i == TILE_MAP_SIZE/2) && (j == TILE_MAP_SIZE/2)) tileMap.grid[i][j] = MASTER_TILE_ID;
    }
  }

  // Internal Device Map - Initial Population
  for(uint8_t i = 1; i < TILE_MAX; ++i){
    tile[i].active  = false;
    tile[i].addr    = addr_lst[i];
    tile[i].pos.x   = 0;
    tile[i].pos.y   = 0;
    tile[i].ports   = 0b00000000;
  }

  // Master Tile Setup
  tile[0].active = true;
  tile[0].addr = addr;
  tile[0].pos.x = 3;
  tile[0].pos.y = 3;

  data = tile[0];

  // Directional Pin Setup
  pinMode(PIN_DIR_U, INPUT_PULLDOWN);
  pinMode(PIN_DIR_D, INPUT_PULLDOWN);
  pinMode(PIN_DIR_L, INPUT_PULLDOWN);
  pinMode(PIN_DIR_R, INPUT_PULLDOWN);
  // pinMode(PA2, OUTPUT);

  // I2C Master Setup
  Wire.begin();
}

void MasterTile::setTileCount(const uint8_t count) {
  tileCount = count;
}

uint8_t MasterTile::getTileCount() {
  return tileCount;
}

uint8_t MasterTile::getOrderedTileID(const uint8_t index) {
  return tileOrder[index];
}

struct POS MasterTile::getScrollPos() {
  return scrollPos;
}
/*
getTile - returns a tile
  Input:
    id - the id of the tile to be returned
*/
struct TILE MasterTile::getTile(const uint8_t id) {
  return tile[id];
}

void MasterTile::resetTileOrder() {
  for(uint8_t i = 0; i < TILE_MAX; ++i) {
    tileOrder[i] = 0;
  }
}

void MasterTile::resetTileMapBounds() {
  tileMap.xMax = TILE_MAP_SIZE/2;
  tileMap.xMin = TILE_MAP_SIZE/2;
  tileMap.yMax = TILE_MAP_SIZE/2;
  tileMap.yMin = TILE_MAP_SIZE/2;
}

/*
getCursor - gets the cursor position for a tile at tileOrder[tileNumber]
  Inputs: the
*/
struct POS MasterTile::getOffsetCursor(uint8_t tileNumber, uint8_t charXIndex) {
  POS offset;
  offset.x = scrollPos.x + (tileNumber * MATRIX_WIDTH) - (charXIndex * CHAR_WIDTH);
  offset.y = scrollPos.y;
  return offset;
}

/*
handleDisplayShape - checks for the addition or removal of Tiles from the display and
  updates the
*/
uint8_t MasterTile::handleDisplayShape() {
    resetTileMapBounds();

    //Determine occupied directions
    findNeighborTiles();
    tile[0] = getData();
    handleAddedOrRemovedTiles();

    return tileCount;
}

/*
handleAddedOrRemovedTiles - Determines if any tiles have been added or removed and updates the tile order.
*/
void MasterTile::handleAddedOrRemovedTiles () {
    tileCount = 1;
    uint8_t tilesUpdated = false;

    // Loop to check if the currently existing tiles
    // still exist, if not clear and erase from tileMap
    // tile[0] will be reserved for the master
    for (uint8_t i = 0; i < TILE_MAX; i++) {
      //Deal with slave Tiles
      if ( i != 0 ) {
        uint8_t response = 5;
        if ( tile[i].active ) {
          Wire.beginTransmission(tile[i].addr);
          response = Wire.endTransmission();
        } else {
          tileID = i;
        }

        switch(response) {
          case 0:
          // SUCCESS
            ++tileCount;
            adjustMapBounds(tile[i]);
            break;
          case 1:
          // DATA TOO LONG
            debugWithMatrix(0, 0, RED);
            addressNotFound(tile[i]);
            tilesUpdated = true; // Tile removed
            break;
          case 2:
          // NACK ON TRANSMIT OF ADDRESS
            debugWithMatrix(1, 0, RED);
            addressNotFound(tile[i]);
            tilesUpdated = true; // Tile removed
            break;
          case 3:
          // NACK ON TRANSMIT OF DATA
            debugWithMatrix(1, 0, RED);
            addressNotFound(tile[i]);
            tilesUpdated = true; // Tile removed
            break;
          case 4:
          // OTHER ERROR
            debugWithMatrix(2, 0, RED);
            addressNotFound(tile[i]);
            tilesUpdated = true; // Tile removed
            break;
          default:
          // TILE NOT ACTIVE
            addressNotFound(tile[i]);
            tilesUpdated = true; // Tile removed
        } // END SWITCH
      } // END NOT MASTER IF

      // Check for new tiles
      if(tile[i].ports != tile[i].previousPorts){
        addNewTile(tile[i]);
        tilesUpdated = true; // Tile added
      }
    }// End FOR loop

    if(tilesUpdated){
      configTileOrder();
    }
}

/*
addNewTile - finds the location for the new tile in the tile map
  Inputs:
    &tile - the new tile to be added
*/
void MasterTile::addNewTile(const struct TILE &tile) {
    uint8_t newDirection = tile.ports ^ tile.previousPorts;
    uint8_t x, y;
    switch(newDirection){
      case CNCT_U:
        x = tile.pos.x;
        y = tile.pos.y - 1;
        break;
      case CNCT_D:
        x = tile.pos.x;
        y = tile.pos.y + 1;
        break;
      case CNCT_L:
        x = tile.pos.x - 1;
        y = tile.pos.y;
        break;
      case CNCT_R:
        x = tile.pos.x + 1;
        y = tile.pos.y;
        break;
      default:
        x = 0;
        y = 0;
    }// END SWITCH

    uint8_t response = assignNewAddress(y, x);
    if(response != SUCCESS) {
      debugWithMatrix(response, 0, RED);
    }
}

/*
assignNewAddress - assigns an address to a newly connected Tile
  Inputs:
    y - y index of new tile in tileMap
    x - x index of new tile in tileMap
  Outputs:
    Return value of endTransmission
*/
uint8_t MasterTile::assignNewAddress(const uint8_t x, const uint8_t y) {
  // Check if the default address exist
  uint8_t response = -1;
  Wire.beginTransmission(I2C_DEFAULT);
  response = Wire.endTransmission();
  if (response != SUCCESS) return response;

  Wire.beginTransmission(I2C_DEFAULT);
  Wire.write('A');
  Wire.write(tile[tileID].addr); //Assign the next available address from
  response = Wire.endTransmission();
  if (response != SUCCESS) return response;

  uint8_t waits = 0;
  response = -1;
  while(response != SUCCESS && waits < 20) {
    delay(10);
    Wire.beginTransmission(tile[tileID].addr);
    response = Wire.endTransmission();
    ++waits;
  }

  if (response != SUCCESS) return response;

  tile[tileID].active = true;
  tile[tileID].pos.x = x;
  tile[tileID].pos.y = y;
  tileMap.grid[y][x] = tileID;
  return SUCCESS;
}

/*
transmitI2cCharData - Transmits the Character Data to a slave tile.
  Inputs:
    addr    - address of the slave tile
    pos     - reference position of where character data should start
    color   - RGB value to display text
    data    - 2 byte character array
  Outputs:
    Return value of endTransmission
*/
uint8_t MasterTile::transmitI2cCharData(const uint8_t addr, const struct POS &pos, const uint16_t color, char data[]) {
    Wire.beginTransmission(addr);
    Wire.write('Q'); // New Identifier for sending Character data? using Q arbritrarily
    Wire.write(pos.x);
    Wire.write(pos.y);
    Wire.write(color);
    for(int i = 0; i < MAX_DISPLAY_CHARS; ++i){ //For 4x4 should be 2
        Wire.write(data[i]);
    }
    return Wire.endTransmission();
}

/*
getOutputData - gets the characters that are to be sent to a Tile
  Inputs:
    dataOut     - Character Array with MAX_DISPLAY_CHARS size
    textData    - Text Array with textLength size
    textLength  - The length of the text to be displayed as obtained from Sanket's code
    charIndex   - The index of the character the scroll position is currently at

        scrollPos = 2 and charIndex = 0
           V
    0  0  0| 0  0  .  0  .      .  .  .  .  .  .  .  .
    0  .  0| .  0  .  0  .      .  .  .  .  .  .  .  .
    .  .  0| .  .  .  0  .      .  .  .  .  .  .  .  .
    .  .  0| .  .  .  0  .      .  .  .  .  .  .  .  .
    .  .  0| .  .  .  0  .      .  .  .  .  .  .  .  . => TL
    .  .  0| .  .  .  0  .      .  .  .  .  .  .  .  .
    .  0  0| 0  .  .  0  0      0  0  0  .  .  .  .  .
    .  .  .| .  .  .  .  .      .  .  .  .  .  .  .  .

  Outputs:
    N/A
*/
uint8_t MasterTile::getOutputData(char dataOut[], char textData[], const uint8_t textLength, const uint8_t charIndex){
  for( int i = 0; i < MAX_DISPLAY_CHARS; ++i){
    if ( i == 0 ){
      dataOut[i] = textData[charIndex];
    }else{
      if( charIndex + i + 1 > textLength){
        dataOut[i] = ' '; //Data out of bounds set to empty character
      }else{
        dataOut[i] = textData[charIndex + i];
      }
    }
  }

  return MAX_DISPLAY_CHARS;
}

/*
updateScrollPos - gets the current position in the scrolling string
*/
void MasterTile::updateScrollPos() {
  if (scrollPos.x >= scrollLength) {
    scrollPos.x = 0;
  } else {
    ++scrollPos.x;
  }
}

/*
adjustMapBounds - sets the outer bounds of the tileMap
  Inputs:
    &tile - ref to a tile
*/
void MasterTile::adjustMapBounds(TILE &tile) {
  tile.previousPorts = tile.ports;
  //If available request current port status from slave devices
  Wire.requestFrom(tile.addr, 1);
  tile.ports = Wire.read();
  if (tile.pos.x < tileMap.xMin){
    tileMap.xMin = tile.pos.x;
  }
  if (tile.pos.x > tileMap.xMax){
    tileMap.xMax = tile.pos.x;
  }
  if (tile.pos.y > tileMap.yMax){
    tileMap.yMax = tile.pos.y;
  }
  if (tile.pos.y < tileMap.yMin) {
    tileMap.yMin = tile.pos.y;
  }
}

/*
configTileOrder - sorts tileOrder with tilIDs from left to right then top to bottom

  tileMap.grid    =>    tileOrder
    0 0 0 0 0
    0 0 2 1 0
    0 0 9 3 0    =>    [2, 1, 9, 3]
    0 0 0 0 0
    0 0 0 0 0
*/
void MasterTile::configTileOrder() {
  resetTileOrder();

  uint8_t cnt_order = 0;
  for(uint8_t y = tileMap.yMin; y <= tileMap.yMax; ++y){
    for(uint8_t x = tileMap.xMin; x <= tileMap.xMax; ++x){
      uint8_t currentTileID = tileMap.grid[y][x];

      if (currentTileID == MASTER_TILE_ID){
        tileOrder[cnt_order] = currentTileID;
        ++cnt_order;
      }
      else if (currentTileID != 0 && tile[currentTileID].active){
        tileOrder[cnt_order] = currentTileID;
        ++cnt_order;
      }
    }
  }// End looping through array
}

/*
addressNotFound - resets the tilemap position and data of the missing tile
  Inputs:
    &tile - the tile that is not found
*/
void MasterTile::addressNotFound(struct TILE &tile) {
    tileMap.grid[tile.pos.y][tile.pos.x] = 0;
    tile.pos.x = 0;
    tile.pos.y = 0;
    tile.active = false;
}

/*
transmitToSlave - transmits data to a slave tile depending on operation mode
  Inputs:
    addr  - address of the slave tile to update
    pos   - cursor offset for slave tile
    color - color to send to the tile
    data  - char array of data to be sent to slave
*/
void MasterTile::transmitToSlave(const uint8_t addr, const struct POS &pos, const uint16_t color, char data[]) {
    switch(operationMode) {
      case (SCROLL_MODE):
        transmitI2cCharData(addr, pos, color, data);
        break;
      case (GESTURE_MODE):
        // TBD
        break;
      case (DIRECTION_TEST):
        // struct POS temp;
        // temp.x = 0;
        // temp.y = 0;
        // transmitI2cData(addr, temp, color);
        break;
    }
}

//Currently only a prototype for when Sanket implements his code as well
void MasterTile::setTextData(const char text[], uint16_t size) {
  if (size > MAX_STRING_SIZE) {
    size = MAX_STRING_SIZE - 1;
  }
  for (uint8_t i = 0; i < size; ++i) {
    textData[0] = text[0];
  }
  textData[size] = '\0';
  scrollLength = size * CHAR_WIDTH;
}

