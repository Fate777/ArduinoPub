#include <LiquidCrystal.h>
#include <math.h>
//Added 3/7/2026 for SD File I/O
#include <SPI.h>
#include <SD.h>
//Added 3/11/2026 for sound output to DFPlayer Mini
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

/*
  Double Diamond - Arduino Ver.
  Created by Bryan Umaguing
  ET238B Spring 2026 - Jerry Pittmon

  Version Log
  02/26/2026   -   Initial Development
                  - Basic slot functionality
                    - Double diamond symbols & pays, plus more...
                  - Additional features
                    - Add'l credits
                    - Debugging / Development settings
                      - More info (atttendant mode)
                      - Reset credits (show floor mode)
  02/27/2026   -   More features
                  - Progressive (only at max credit)
                    - 3x Double Diamond (Grand)
                    - 2x Double Diamond + 1 Red Seven (Major)
                  - Credit check
                    - Inform player if insufficient credits
                  - Bonus Features
                    - Haywire trigger
                      - Triggers on most wins except 3x top symbol
              -   Code cleanup
                  - Moved display win/loss into function (displayResult)
                  - Fixed bug in spinReels logic
                  - Fixed wild check in spinReels logic
                  - Added function declarations
  03/04/2026  -   Input revision
                  - Changed button references to point to new 4-button keyboard
  VERSION 2   -   Input revision
                  - Pin reassignment due to rearranging of components
              -   More features
                  - Read from SD for player tracking purposes
              -   Code cleanup
                  - Revised progressive gain rate to global const vars
  03/06/2026  -   More features
                  - LHA, aka "Luck has arrived"
  VERSION 3   -   New peripheral added
                  - SD Card reading code for "player tracking" prototype
              -   More Features
                  - Multi-player player-based tracking on a single medium / SDCard (PBT)
  03/07/2026  -   Hardware Migration
                  - Pin reassignment from Arduino Uno to Arduino Mega
  VERSION 4       - I repeat, this code will NOT work with the Arduino Uno. (unless you're down to learn about multiplexers & bitshift registers, turning this microcontroller into fully fleshed out computer...)
              -   Code cleanup
                  - Moved multiple global vars to local
  03/08/2026  -   More Features
                  - Implemented starting functions for SD File I/O from fileIOTemplate project
                    - Read users from user folders & assign to PT variables
                    - Write PT variables back to user file
                    - Helper functions
                  - Initial File I/O into SD for player tracking
  03/11/2026  -   More Features
  VERSION 5       - Initial implementation of sound bank info & sound output to DFPlayer Mini module
  
  ToDo / WIP Features
  - HIGH PRIORITY
    - Figure out physical spinning reel logic (link to existing capstone in lab)
    - Revise reel logic to use virtual -> physical / 6 array setup
    - Figure out file output bug
    - Auto spins 
    - Massive code refactor - Re-sort code & modularize into .h/.cpp libraries
  - NICE TO HAVES
    - Volume control
    - Password check for certain playerTypes
    - Pinball Bonus
    - Top Dollar Bonus
    - Haywire Feature (extra spins on certain wins)
    - Wild+ Extra spin (Everi Patriot Extra Spin style, grant 1 extra spin w/ held wilds if at least 1 double diamond exists)
  - LOW PRIORITY / BRAINSTORMING
    - Player tracking (Joint Accounts)

*/

//Global Vars

// BUTTON INPUT PIN VARS
const int playButtonPin = 40;            // BLUE, the number of the play button pin
const int creditsPlayedButtonPin = 41;   // ORANGE, the number of the credits pin
const int cashOutButtonPin = 42;          // BLUE, # of cash out pin
const int opButtonPin = 43;              // ORANGE, the number of the operator pin

//FOR LCD PIN SETTINGS
const int rs=48;
const int en=49;
const int d4=44;
const int d5=45;
const int d6=46;
const int d7=47;

//LCD Display intialization
LiquidCrystal lcd(rs,en,d4,d5,d6,d7);

//FOR SD CARD
const int chipSelectPin = 53;       //YELLOW

//FOR DFPLAYER MINI
// Use pins 2 and 3 to communicate with DFPlayer Mini
static const uint8_t PIN_MP3_TX = 11; // Connects to module's TX
static const uint8_t PIN_MP3_RX = 10; // Connects to module's RX
SoftwareSerial softwareSerial(PIN_MP3_TX, PIN_MP3_RX);

// Create the Player object
DFRobotDFPlayerMini player;

const int lcdColCount = 16;
const int lcdRowCount = 2;

const int startingCredits = 1000;
const int maxCredits = 3, bonusJackpotDD = 100, bonusJackpot7s = 40, startingProgDD = 2500, startingProg7s = 960;
//Haywire hit rate - think 1 out of...
const int hwCherryChance = 4, hwAnyBarChance = 8, hwSingleBarChance = 8, hwDoubleBarChance = 25, hwTripleBarChance = 50, hw7sChance = 1000000, hwDCChance = 1000;
const int hwCherryMaxAmount = 10, hwAnyBarMaxAmount = 4, hwSingleBarMaxAmount = 3, hwDoubleBarMaxAmount = 2, hwTripleBarMaxAmount = 1, hw7sMaxAmount = 1, hwDCMaxAmount = 10;
const int reelStops = 50;
const int reelSpeed = 750; //BU: Used in spinReels logic
const int startingPlayerID = 1; //Refer to playerList to see who to start out (MUST EXIST IN PLAYER TRACKING SD CARD)
const int startingVolume = 30;   //15 is good!

const int reel1Arr[50] = {0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,
                          0,4,1,1,1,1,1,1,1,1,
                          1,2,2,2,2,2,2,2,2,3,
                          3,3,7,7,7,7,7,8,8,9};
const int reel2Arr[50] = {0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,
                          0,4,1,1,1,1,1,1,1,1,
                          1,1,1,1,1,2,2,3,3,3,
                          7,7,7,7,7,7,7,8,8,9};
const int reel3Arr[50] = {0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,
                          4,4,1,1,1,1,1,1,1,1,
                          1,1,1,1,1,1,1,2,2,2,
                          2,2,2,3,3,3,7,7,8,9};

int displayState = 1, maxDisplayStates = 10; // 1 = Main, 2 = Credits, 3 = Bet

int reel1 = 0, reel2 = 0, reel3 = 0;

int reelSpinState = 1;

// Game state vars:
bool bIdle = 1, bInGame = 0, bTilt = 0;   //I don't know when the tilt flag will be used, but add it in for potential future use
int lastResult = 0;

// LHA Settings
int lhaSpread[100];
//LHA hit rate - think 1 out of...
const int lhaChance = 10;    // s/b 10
const int lha0sAmt = 67;
const int lha1sAmt = 27;
const int lha2sAmt = 5;
const int lha3sAmt = 1;
int lhaReel1 = 0, lhaReel2 = 0, lhaReel3 = 0;
bool bLHACheck = false;

//PT Settings
String playerList[9] = {"Bryan", "Jerry", "Kendrick", "Melissa", "Ryan", "Hunter", "Graham", "Brethnie", "Brian"};
String themeList[6] = {"IGTLegacy", "IGTEnhanced", "TwinWin", "GameKing", "Aristocrat", "Kendrick"};
String passwordList[9] = {"12321", "123", "-1", "-1", "123", "-1", "-1", "-1", "123"};
int creditBank[9] = {};
String filePathSuffix = "\/user.txt";
String fullFilePath;
//PT user attributes
int PID = -1;
String name = "";
int playerCredits = -1;
int playCount = -1;
int winLoss = -1;
int haywireCount = -1;
int lhaCount = -1;
int highScore = -1;
String playerType = "";
String defaultTheme = "IGTLegacy";
//Admin/Instructor/Tech exclusive settings
bool allowDebug = false;
//PT state vars
bool bPasswordCheckState = false;
//PT more info vars
String moreInfoName = "";
int playerPosition = -1;
int themeID = 1;

// Player settings & initial credits
int numCredits = 0;
int creditsPlayed = 1;
double progressiveDDAmt = startingProgDD;
double progressive7sAmt = startingProg7s;
bool bProgressive = true, bHaywire = true, bLHA = true;

//WIP Settings (SAVE FOR FUTURE USE)
//bool bPatriotEX = false, bPinball = false, bTopDollar = false;

int debug = 0;
int resetCreditsCheck = 0;
bool forceHaywire = true;
int forceHaywireLoops = 0;
bool forceLHA = false;
int forceLHACondition = 1;

// int devReel1 = 24, devReel2 = 24, devReel3 = 24;
int devReel1 = 48, devReel2 = 48, devReel3 = 48;

//function declarations
void addButtonDelay();
template <typename T> void printToLCD(int lcdRowPos, int lcdColPos, T outputString);
void setCurrentState(bool& stateType, bool state);
void displayIdleOnLCD(int dState);
void updateCreditsOnLCD(int dState);
void updateBetOnLCD(int dState);
void spinReels(int debug);
int getResult(int reel1, int reel2, int reel3, int creditsPlayedLoc);
void displayResultOnLCD(int resultLoc);
void haywireBonus(int& result, int basePay, int hwChance, int hwMaxAmount);
//Added 3/5/2026
void LHABonus(bool& result, int reelStops);
void forceReelResult();
void initLHAChanceArray();
//Added 3/8/2026: PT Function Declarations
void getPlayerInfo(String path, int from, int to);
void assignPlayerInfoToGlobals(String playerInfo, int position);
String convertPlayerInfoToDataString (int PID, String name, int playerCredits, int playCount, int winLoss, int haywireCount, int lhaCount, int highScore, String playerType, String defaultTheme);
void writePlayerInfo(String path);
void displayWelcomeOnLCD();
//Added 3/11/2026: DFPlayer Mini Functions
int getSoundFile(String eventName);
int getThemeFolder(String themeName);
void playSound(String eventName, int delayTime = 0);


void setup() {
  // put your setup code here, to run once:
  String debugString;
  
  
  Serial.begin(9600);
  Serial.println("Starting Program.");
  
  // BU: DISABLED AS OF 3/3/2026
  // pinMode(playButtonPin, INPUT);
  // pinMode(creditsPlayedButtonPin, INPUT);
  // pinMode(opButtonPin, INPUT);

  // BU (3/3/2026): New button init functions
  pinMode(playButtonPin, INPUT_PULLUP);
  pinMode(creditsPlayedButtonPin, INPUT_PULLUP);
  pinMode(opButtonPin, INPUT_PULLUP);
  pinMode(cashOutButtonPin, INPUT_PULLUP);

  lcd.begin(lcdColCount,lcdRowCount);

  //intialize the RNG seed
  randomSeed(analogRead(0));

  if (bProgressive) {
    maxDisplayStates += 2;
  }

  //initialize LHA Chance Array
  initLHAChanceArray();

  //Initialize program start with my credentials
  // Open the file 'data.txt'. FILE_WRITE allows us to save new data.
  String fileData[10];
  File readFile;
  fullFilePath = playerList[startingPlayerID] + filePathSuffix;
  int lineStart = 1, lineEnd = 10;

  // Start communication with the computer (Serial Monitor)
  Serial.begin(9600);
  
  Serial.print("Starting SD card...");

  // Try to start the SD card module
  if (!SD.begin(chipSelectPin)) {
    Serial.println("SD card failed or not present!");
    return; // Stop the program here if it fails
  }
  Serial.println("SD card connected.");
  
  Serial.println("--- Reading saved file ---");

  // Init serial port for DFPlayer Mini
  softwareSerial.begin(9600);

  Serial.println("Initializing DFPlayer");

  // Start communication with DFPlayer Mini
  if (player.begin(softwareSerial)) {
    Serial.println("DFPlayer initialization - OK");

    // Set volume to maximum (0 to 30).
    player.volume(startingVolume);      //15 is nice
    // Play the "0001.mp3" in the "mp3" folder on the SD card
    // player.playMp3Folder(1);
    // player.playFolder(3, 98); //USE THIS TO PLAY FILES

  } else {
    Serial.println("Connecting to DFPlayer Mini failed! - Player could not begin");
  }
  getPlayerInfo(fullFilePath, 1, 10);
}

void loop() {

  // variables will change:
  int playButtonState;  // variable for reading the playButtonState status
  int creditsPlayedState;  // variable for reading the creditsPlayedState status
  int opButtonState;  // variable for reading the opButtonState status
  int cashOutButtonState;  // variable for reading the opButtonState status

  String spinReelPrefix = "spinReel ", spinReelString = "";

  const double progGainGrand = 4.0, progGainMajor = 2.0;

  // DEV - Debug Options
  const int resetCreditsCount = 15;
  
  int resultLoc = 0;
  debug = 0;

  lcd.clear();

  //BU: DISABLED AS OF 3/3/2026 DUE TO BEHAVIOR OF READING NEW KEYBOARD BUTTONS
  // read the state of the pushbutton value:
    // playButtonState = digitalRead(playButtonPin);
    // creditsPlayedState = digitalRead(creditsPlayedButtonPin);
    // opButtonState = digitalRead(opButtonPin);

    //BU (3/3/2026): DUE TO BEHAVIOR OF KEYBOARD, FLIP BOOLEAN CHECK WHEN READING BUTTONS
    // read the state of the pushbutton value:
    playButtonState = (! digitalRead(playButtonPin));
    creditsPlayedState = (! digitalRead(creditsPlayedButtonPin));
    opButtonState = (! digitalRead(opButtonPin));
    cashOutButtonState = (! digitalRead(cashOutButtonPin));


  if (bIdle == true) {
      displayIdleOnLCD(displayState);
  }

  // More Info: Trigger if more info button is pressed
  if (creditsPlayedState == HIGH && opButtonState == LOW && displayState != 5 && opButtonState == LOW) { 
    creditsPlayed++;
    if (creditsPlayed > maxCredits) {
      creditsPlayed = 1;
    }
    addButtonDelay();
  }
  else if (creditsPlayedState == HIGH && opButtonState == LOW && displayState == 5 && opButtonState == LOW) { 
    playerPosition++;
    if (playerPosition > 8) { 
      playerPosition = 0;
    }
    moreInfoName = playerList[playerPosition];
    delay(100);
  }
  else if (creditsPlayedState == HIGH && opButtonState == HIGH) { 
    resetCreditsCheck = 0;           // Add this reset here so it doesn't reset credits while user tries to navigate
    displayState++;
    if (displayState > maxDisplayStates) {
      displayState = 1;
    }
    addButtonDelay();
  }
  else if (creditsPlayedState == LOW && opButtonState == HIGH && hasAdminPrivileges()) { 
    resetCreditsCheck++;
    if (resetCreditsCheck > resetCreditsCount) {
      resetCreditsCheck = 0;
      numCredits = startingCredits;
    }
    addButtonDelay();
  }

  if (cashOutButtonState == HIGH && displayState != 5 && name.compareTo("") != 0) { 
    lcd.clear();
    printToLCD(0,0, "Cashing out..");
    playSound("cash out");
    delay(1500);
    writePlayerInfo(fullFilePath);
    //PUT THESE INTO FUNCTION IN FUTURE (setDefaultUser?)
    name = "";
    moreInfoName = "";
    numCredits = 0;
    playerPosition = -1;
    themeID = 1;
    defaultTheme = "IGTLegacy";
    lcd.clear();
    printToLCD(0,0, "Thank you");
    printToLCD(0,1, "for playing!");
    delay(2500);
    addButtonDelay();
  }
  else if (cashOutButtonState == HIGH && displayState == 5) {   //5 s/b player name display
    if (name.compareTo(moreInfoName) != 0) {
      if(name.compareTo("") != 0) {
        writePlayerInfo(fullFilePath);
      }
      lcd.clear();
      printToLCD(0,0, "Switching players...");
      delay(1500);
      fullFilePath = moreInfoName + filePathSuffix;
      getPlayerInfo(fullFilePath, 1, 10);
    }
  }

  if (playButtonState == HIGH && bIdle == true && numCredits >= creditsPlayed) {
    displayState = 1;
    if(opButtonState == HIGH && hasAdminPrivileges()) {
      debug = 1;
    }

    setCurrentState(bIdle, false);
    setCurrentState(bInGame, true);
    playCount++;

    numCredits = numCredits - creditsPlayed;
    winLoss = winLoss - creditsPlayed;
    if (creditsPlayed == 1) {
      playSound("1 credit", 500);
    }
    else if (creditsPlayed == 2) {
      playSound("2 credit", 500);
    }
    else if (creditsPlayed == 3) {
      playSound("3 credit", 500);
    }
    else {
      playSound("1 credit", 500);
    }
    
    if(bProgressive) {
      progressiveDDAmt += (double) creditsPlayed / progGainGrand;
      progressive7sAmt += (double) creditsPlayed / progGainMajor;
    }
    updateCreditsOnLCD(displayState);

    if ((debug == 0 && bLHA == true) || (debug == 1 && forceLHA)) {
      LHABonus(bLHACheck, reelStops);
      if (bLHACheck) {
        debug = 3; //Force spinReels at this point to do the LHA logic
      }
    }

    if (reelSpinState > 4) {
      reelSpinState = 1;
    }
    spinReelString = spinReelPrefix + (String) reelSpinState;
    Serial.println(spinReelString);
    playSound(spinReelString);
    reelSpinState++;
    spinReels(debug);
    resultLoc = getResult(reel1, reel2, reel3, creditsPlayed);

    lastResult = resultLoc;
    displayResultOnLCD(resultLoc);
    
    numCredits += resultLoc;
    winLoss += resultLoc;

    if (resultLoc > highScore) {
      highScore = resultLoc;
    }

    bLHACheck = false;

    //Update stats on SD before ending game
    writePlayerInfo(fullFilePath);
    setCurrentState(bInGame, false);
    setCurrentState(bIdle, true);
  }
  else if (playButtonState == HIGH && bIdle == true && numCredits < creditsPlayed) {
    lcd.clear();
    printToLCD(0,0, "Not enough");
    printToLCD(0,1, "credits.");
    delay(1000);

  }

  delay(200);

} // END LOOP()

/***************** ******************************** ******************************** ******************************** ******************************** **********************/

void addButtonDelay(){
  //Button inputs are twitchy if inputs aren't buffered with delays
  delay(250);
}

template <typename T>
void printToLCD(int lcdRowPos, int lcdColPos, T outputString) {
    lcd.setCursor(lcdRowPos, lcdColPos);
    lcd.print(outputString);
}

void setCurrentState(bool& stateType, bool state) {
  stateType = state;
}


void displayIdleOnLCD(int dState){
  if (dState == 1) {
        printToLCD(0,0,"Press play");

        //Write credits on bottom right corner
        updateCreditsOnLCD(dState);
        updateBetOnLCD(dState);
        
        printToLCD(0, 1, reel1);
        printToLCD(2, 1, reel2);
        printToLCD(4, 1, reel3);
  }
  else if (dState == 2) {
        printToLCD(0,0,"Credits");
        updateCreditsOnLCD(dState);
  }
  else if (dState == 3) {
        printToLCD(0,0,"Bet Amount");
        updateBetOnLCD(dState);
  }
  else if (dState == 4)  { 
        printToLCD(0,0,"Last Spin");
        printToLCD(0,1,lastResult);
  }
  else if (dState == 5)  { 
        printToLCD(0,0,"Player Name");
        printToLCD(0,1,moreInfoName);
  }
  else if (dState == 6)  { 
        printToLCD(0,0,"Play Count");
        printToLCD(0,1,playCount);
  }
  else if (dState == 7)  { 
        printToLCD(0,0,"Win / Loss");
        printToLCD(0,1,winLoss);
  }
  else if (dState == 8)  { 
        printToLCD(0,0,"Haywire Count");
        printToLCD(0,1,haywireCount);
  }
  else if (dState == 9)  { 
        printToLCD(0,0,"LHA Count");
        printToLCD(0,1,lhaCount);
  }
  else if (dState == 10)  { 
        printToLCD(0,0,"Biggest Win");
        printToLCD(0,1,highScore);
  }
  else if (dState == 11)  { 
        printToLCD(0,0,"Prog-Grand");
        printToLCD(0,1,(int) floor(progressiveDDAmt));
  }
  else if (dState == 12)  { 
        printToLCD(0,0,"Prog-Major");
        printToLCD(0,1,(int) floor(progressive7sAmt));
  }
}

void updateCreditsOnLCD(int dState) {
  //Write credits on bottom right corner
  if (dState == 1) {
    if (numCredits >= 10000) {
        printToLCD(11,1,numCredits);
    }
    else if (numCredits >= 1000 && numCredits <= 9999) {
        printToLCD(12,1,numCredits);
    }
    else if (numCredits >= 100 && numCredits <= 999) {
        printToLCD(12,1," ");
        printToLCD(13,1,numCredits);
    }
    else if (numCredits >= 10 && numCredits <= 99) {
        printToLCD(13,1," ");
        printToLCD(14,1,numCredits);
    }
    //credits between 0-9
    else  { 
        printToLCD(14,1," ");
        printToLCD(15,1,numCredits);
    }
  }
  //Write credits on bottom left corner
  else {
    printToLCD(0,1,numCredits);
  }
  
}

void updateBetOnLCD(int dState) {
  //Write bet amount on top right corner
  if (dState == 1) {
    printToLCD(15,0,creditsPlayed);
  }
  //Write bet amount on bottom left corner
  else {
    printToLCD(0,1,creditsPlayed);
  }
  
}

void spinReels(int debug) {
  const int reelStops = 50; // s/b 22 physical symbols, but reelStops is virtual and can go from 22-100

  /* Debug values 
    1 - Operator / Force result
    2 - Haywire mode
    3 - LHA
  */
  int reel1Loc = reel1, reel2Loc = reel2, reel3Loc = reel3;

  lcd.clear();
  if (debug == 0 || debug == 3) {
    printToLCD(0,0,"Spinning..");
  }
  else if (debug == 1) {
    printToLCD(0,0,"Testing..");
  }
  else if (debug == 2) {
    printToLCD(0,0,"HAYWIRE!!");
    playSound("haywire");
    
  }

  for (int i = 0; i < reelSpeed; i++) {
    reel1 = random(0, reelStops);
    reel2 = random(0, reelStops);
    reel3 = random(0, reelStops);

    reel1 = reel1Arr[reel1];
    reel2 = reel2Arr[reel2];
    reel3 = reel3Arr[reel3];

    printToLCD(0, 1, reel1);
    printToLCD(2, 1, reel2);
    printToLCD(4, 1, reel3);
  }
  if (debug == 1) {
    reel1 = reel1Arr[devReel1];
    printToLCD(0, 1, reel1);
  }
  //For Haywire
  else if (debug == 2) {
    reel1 = reel1Loc;
    printToLCD(0, 1, reel1);
  }
  //For LHA
  else if (debug == 3) {
    reel1 = lhaReel1;
    printToLCD(0, 1, reel1);
  }

  for (int i = 0; i < reelSpeed; i++) {
    reel2 = random(0, reelStops);
    reel3 = random(0, reelStops);

    reel2 = reel2Arr[reel2];
    reel3 = reel3Arr[reel3];

    printToLCD(2, 1, reel2);
    printToLCD(4, 1, reel3);
  }

  if (debug == 1) {
    reel2 = reel2Arr[devReel2];
    printToLCD(2, 1, reel2);
  }
  //For Haywire
  else if (debug == 2) {
    reel2 = reel2Loc;
    printToLCD(2, 1, reel2);
  }
  //For LHA
  else if (debug == 3) {
    reel2 = lhaReel2;
    printToLCD(2, 1, reel2);
  }

  for (int i = 0; i < reelSpeed; i++) {
    reel3 = random(0, reelStops);

    reel3 = reel3Arr[reel3];

    printToLCD(4, 1, reel3);
  }

  if (debug == 1) {
    reel3 = reel3Arr[devReel3];
    printToLCD(4, 1, reel3);
  }
  //For Haywire
  else if (debug == 2) {
    reel3 = reel3Loc;
    printToLCD(4, 1, reel3);
  }
  //For LHA
  else if (debug == 3) {
    reel3 = lhaReel3;
    printToLCD(4, 1, reel3);
  }

  delay(500);
}

int getResult(int reel1, int reel2, int reel3, int creditsPlayedLoc)  {
  const int wildMulti = 2;
  
  int resultPosBase = 0, resultNegBase = 0, resultPosFinal = 0, resultNegFinal = 0, finalMulti = 1;
  int haywireCheck = 1, haywireLoops = 0;
  int totalResultLoc = 0;

  bool hw7sEligible = true; //USE ONLY FOR 7s DUE TO IT HAVING A PROGRESSIVE BONUS
  bool haywireCherries = false; //Special bool for cherries and/or DCs

  /* RESULT ARRAY ORDER - Values in positions will hold # of symbols or symbol types (e.g "Any Bar")
    0 - Blank
    1 - Single Bar
    2 - Double Bar
    3 - Triple Bar
    4 - Cherry
    5 - NOT USED
    6 - NOT USED
    7 - Red Seven
    8 - Double Diamond (WILD)
    9 - DC

    --Aggregates
    10 - Any Bar
  */
  int resultArray[11] = {0,0,0,0,0,0,0,0,0,0,0};
  
  resultArray[reel1]++;
  resultArray[reel2]++;
  resultArray[reel3]++;

  
  // Serial.print("Reel1: ");
  // Serial.println(reel1);
  // Serial.print("Reel2: ");
  // Serial.println(reel2);
  // Serial.print("Reel3: ");
  // Serial.println(reel3);

  // Serial.print("resultArray (Before): ");
  // Serial.println(resultArray[10]);

  

  //Any bar counts, account for all bars + double diamond IF there are NOT 3 Double Diamonds
  if ((reel1 == 1) || (reel1 == 2) || (reel1 == 3) || (reel1 == 8 && resultArray[8] != 3)) {
    resultArray[10]++;
  }
  if ((reel2 == 1) || (reel2 == 2) || (reel2 == 3) || (reel2 == 8 && resultArray[8] != 3)) {
    resultArray[10]++;
  }
  if ((reel3 == 1) || (reel3 == 2) || (reel3 == 3) || (reel3 == 8 && resultArray[8] != 3)) {
    resultArray[10]++;
  }


  // Serial.print("resultArray (After): ");
  // Serial.println(resultArray[10]);


  //Add additional counts to non-cherry symbols per wild, & calculate Multiplier if Double Diamond count between 1 & 2
  if(resultArray[8] > 0 && resultArray[8] < 3) {
    resultArray[1] += resultArray[8];
    resultArray[2] += resultArray[8];
    resultArray[3] += resultArray[8];
    resultArray[7] += resultArray[8];
    
    finalMulti = wildMulti * resultArray[8];
  }

  
  // Serial.print("resultArray[8]: ");
  // Serial.println(resultArray[8]);
  // Serial.print("finalMulti: ");
  // Serial.println(finalMulti);

  // Serial.print("resultArray[7]: ");
  // Serial.println(resultArray[7]);
  

  //Now figure out the final payout
  //3x Double Diamond
  if (resultArray[8] == 3) {
    if (bProgressive && creditsPlayed == maxCredits) { 
      resultPosBase = floor(progressiveDDAmt);
      progressiveDDAmt = startingProgDD;
    }
    else {
      resultPosBase = creditsPlayedLoc * 800;
      if (creditsPlayedLoc == maxCredits && bProgressive == false) {
        resultPosBase += bonusJackpotDD;
      }
    }

    resultPosFinal = resultPosBase;
  }
  //3x red sevens
  else if (resultArray[7] == 3) {
    if (bProgressive && creditsPlayed == maxCredits && resultArray[8] == 2) { 
      resultPosBase = floor(progressive7sAmt);
      progressive7sAmt = startingProg7s;
      hw7sEligible = false;
    }
    else {
      resultPosBase = creditsPlayedLoc * finalMulti * 80;
      if (creditsPlayedLoc == maxCredits && resultArray[8] == 2) {
        resultPosBase += bonusJackpot7s;
      }
      hw7sEligible = true;
    }

    resultPosFinal = resultPosBase;

    if (bHaywire && hw7sEligible) {
      haywireBonus(resultPosFinal, resultPosBase, hw7sChance, hw7sMaxAmount);
    }
  }
  //3x triple bar
  else if (resultArray[3] == 3) {
    resultPosBase = creditsPlayedLoc * finalMulti * 40;
    resultPosFinal = resultPosBase;

    if (bHaywire) {
      haywireBonus(resultPosFinal, resultPosBase, hwTripleBarChance, hwTripleBarMaxAmount);
    }
  }
  //3x double bar
  else if (resultArray[2] == 3) {
    resultPosBase = creditsPlayedLoc * finalMulti * 25;
    resultPosFinal = resultPosBase;

    if (bHaywire) {
      haywireBonus(resultPosFinal, resultPosBase, hwDoubleBarChance, hwDoubleBarMaxAmount);
    }
  }
  //3x single bar
  else if (resultArray[1] == 3) {
    resultPosBase = creditsPlayedLoc * finalMulti * 10;
    resultPosFinal = resultPosBase;

    if (bHaywire) {
      haywireBonus(resultPosFinal, resultPosBase, hwSingleBarChance, hwSingleBarMaxAmount);
    }
  }
  //Any bar
  else if (resultArray[10] == 3) {
    resultPosBase = creditsPlayedLoc * finalMulti * 5;
    resultPosFinal = resultPosBase;

    if (bHaywire) {
      haywireBonus(resultPosFinal, resultPosBase, hwAnyBarChance, hwAnyBarMaxAmount);
    }
  }

  //Any cherry
  if (resultArray[4] > 0) {
    //Any double diamonds with the cherries?
    if(resultArray[8] > 0 && resultArray[8] < 2) {
      resultArray[4] += resultArray[8];
    }

    switch(resultArray[4]) {
      case 1:
        resultPosBase = creditsPlayedLoc * finalMulti * 2;
        break;
      case 2:
        resultPosBase = creditsPlayedLoc * finalMulti * 5;
        break;
      case 3:
        resultPosBase = creditsPlayedLoc * finalMulti * 10;
        break;
      default: // THIS SHOULD NEVER BE POSSIBLE
        resultPosBase = 0;
        bTilt = true;
    }
    resultPosFinal = resultPosBase;
    
    if (bHaywire && resultArray[9] < 1) {
      haywireCherries = true;
      //haywireBonus(resultPosFinal, resultPosBase, hwCherryChance, hwCherryMaxAmount);
    }
  } //END Any cherry

  //Any DC
  if (resultArray[9] > 0) {
    //Any double diamonds with the DC?
    if(resultArray[8] > 0 && resultArray[8] < 2) {
      resultArray[9] += resultArray[8];
    }
    switch(resultArray[9]) {
      case 1:
        resultNegBase = creditsPlayedLoc * finalMulti * -2;
        break;
      case 2:
        resultNegBase = creditsPlayedLoc * finalMulti * -5;
        break;
      case 3: //Yeah..They're screwed. But give them 1 last chance.
        resultNegBase = numCredits - 1;
        break;
      default: // THIS SHOULD NEVER BE POSSIBLE
        resultNegBase = 0;
    }

    resultNegFinal = resultNegBase;

    //Allow Evil Haywire if worst case scenario hasn't been hit
    if (bHaywire && resultArray[9] > 0 && resultArray[9] < 3) {
      haywireCherries = true;
      //haywireBonus(resultNegFinal, resultNegBase, hwDCChance, hwDCMaxAmount);
    }
  }

  totalResultLoc = resultPosFinal + resultNegFinal;

  //Haywire check if either cherries are involved
  if (bHaywire && haywireCherries && totalResultLoc != 0) {
    if (totalResultLoc > 0) { //Overall a good result? Give them the normal cherry case
      haywireBonus(totalResultLoc, resultPosFinal + resultNegFinal, hwCherryChance, hwCherryMaxAmount);
    }
    else if (totalResultLoc < 0) { //Overall a bad result? Give them the DC case
      haywireBonus(totalResultLoc, resultPosFinal + resultNegFinal, hwDCChance, hwDCMaxAmount);
    }
    //And if overall result = 0, it means both cherries cancel each other out (e.g. cherry, DD, DC), so don't grant Haywire
  }
  
  return totalResultLoc;
}

void displayResultOnLCD(int resultLoc) {
    if (resultLoc > 0 && resultLoc < 960)
    {
      playSound("winner");
    }
    else if (resultLoc >= 960 && resultLoc < 2500)
    {
      playSound("prog-major");
    }
    else if (resultLoc >= 2500)
    {
      playSound("prog-grand");
    }
    else
    {
      playSound("winner");
    }
    if (resultLoc > 0) {
      lcd.clear();
      printToLCD(0, 0, "You won: ");
      printToLCD(0, 1, resultLoc);
      delay(1500);
    }
    else if (resultLoc < 0) {
      lcd.clear();
      printToLCD(0, 0, "You lost: ");
      printToLCD(0, 1, resultLoc);
      delay(1500);
    }
}

void haywireBonus(int& result, int basePay, int hwChance, int hwMaxAmount) {
  int haywireCheck = 1, haywireLoops = 0;
  if ((debug == 1 && forceHaywire) || (bLHACheck)) {
    haywireCheck = hwChance;
  }
  else {
    haywireCheck = random(1, hwChance + 1); //Include the endpoint for the check
  }
  
  if(haywireCheck == hwChance) {
    haywireCount++;
    displayResultOnLCD(result);
    lcd.clear();
    printToLCD(0, 0, "Haywire");
    printToLCD(0, 1, "triggered!");
    delay(2000);
    
    if (debug == 1 && forceHaywire && forceHaywireLoops > 0) {
      haywireLoops = forceHaywireLoops;
    }
    else {
      haywireLoops = random(1, hwMaxAmount + 1);
    }
    for (int i = 1; i <= haywireLoops; i++) {
      spinReels(2);
      result += basePay;
      displayResultOnLCD(result);
    }
    
  } //END haywireCheck
}

void LHABonus(bool& result, int reelStops) {
  int lhaCheck = 0;
  if (debug == 1 && forceHaywire) {
    lhaCheck = lhaChance;
  }
  else {
    lhaCheck = random(1, lhaChance + 1); //Include the endpoint for the check
  }
  
  if(lhaCheck == lhaChance) {
    result = true;
    lhaCount++;
    lcd.clear();
    printToLCD(0, 0, "Spinning..?");
    for (int i = 0; i < 3000; i++) {
      reel1 = random(0, reelStops);
      reel2 = random(0, reelStops);
      reel3 = random(0, reelStops);

      reel1 = reel1Arr[reel1];
      reel2 = reel2Arr[reel2];
      reel3 = reel3Arr[reel3];

      printToLCD(0, 1, reel1);
      printToLCD(2, 1, reel2);
      printToLCD(4, 1, reel3);
    }
    lcd.clear();
    printToLCD(0, 0, "LUCK HAS");
    printToLCD(0, 1, "ARRIVED!");
    playSound("lha");
    delay(3500);

  forceReelResult();
  }
    
} //END lhaCheck


void forceReelResult(){

  /*
  48, 48, 48 - 3 Double Diamonds (DO NOT USE IN LHA, USE WILD CHANCE INSTEAD)
  44, 45, 47 - 3 7s
  40, 39, 43 - 3 Triple Bars
  31, 35, 40 - 3 Double Bars
  22, 22, 22 - 3 Single Bars
  21, 21, 21 - 3 Cherry
  29, 35, 32 - Any Bar
  0, 21, 21 - 2 Cherry
  0, 0, 21 - 1 Cherry
  */

  int outcomeID = -1, rngWildCount = -1, rngWildLimit = 3;
  int fixedSlotOutcome[6];

  //Init wilds into array
  fixedSlotOutcome[1] = 48;
  fixedSlotOutcome[3] = 48;
  fixedSlotOutcome[5] = 48;

  if (debug = 1 && forceLHA && forceLHACondition != 0) {
    outcomeID = forceLHACondition;
  }
  else {
    outcomeID = random(1, 9); // 1-8, 9 is excluded
  }

  //Force 7s
  if (outcomeID == 1) {
    fixedSlotOutcome[0] = 44;
    fixedSlotOutcome[2] = 45;
    fixedSlotOutcome[4] = 47;
  }
  //Force Triple Bars
  else if (outcomeID == 2) {
    fixedSlotOutcome[0] = 40;
    fixedSlotOutcome[2] = 39;
    fixedSlotOutcome[4] = 43;
  }
  //Force Double Bars
  else if (outcomeID == 3) {
    fixedSlotOutcome[0] = 31;
    fixedSlotOutcome[2] = 35;
    fixedSlotOutcome[4] = 40;
  }
  //Force Single Bars
  else if (outcomeID == 4) {
    fixedSlotOutcome[0] = 22;
    fixedSlotOutcome[2] = 22;
    fixedSlotOutcome[4] = 22;
  }
  //Force 3 Cherries
  else if (outcomeID == 5) {
    fixedSlotOutcome[0] = 21;
    fixedSlotOutcome[2] = 21;
    fixedSlotOutcome[4] = 21;
  }
  //Force Any Bar
  //BU: UNSURE IF I SHOULD FORCE WILD COUNT LIMIT HERE....LEAVE OUT FOR NOW
  else if (outcomeID == 6) {
    fixedSlotOutcome[0] = 29;
    fixedSlotOutcome[2] = 35;
    fixedSlotOutcome[4] = 32;
    //
  }
  //Force 2 Cherries
  else if (outcomeID == 7) {
    fixedSlotOutcome[0] = 0;
    fixedSlotOutcome[2] = 21;
    fixedSlotOutcome[4] = 21;
  }
  //Force 1 Cherry
  else if (outcomeID == 8) {
    fixedSlotOutcome[0] = 0;
    fixedSlotOutcome[2] = 0;
    fixedSlotOutcome[4] = 21;
  }
  //Also should be case that should NEVER HAPPEN
  else {
    bTilt = true;
  }
  //if no special override has been done above, determine DD count here
  if (rngWildCount == -1) {
    rngWildCount = lhaSpread[random(0,100)];
  }

  // Serial.println("rngWildCount: ");
  // Serial.println(rngWildCount);

  //Now that we've determined wild counts, compile final result
  if (rngWildCount == 1) {
    lhaReel1 = reel1Arr[fixedSlotOutcome[1]];
  }
  else {
    lhaReel1 = reel1Arr[fixedSlotOutcome[0]];
  }
  if (rngWildCount == 2) {
    lhaReel2 = reel2Arr[fixedSlotOutcome[3]];
  }
  else {
    lhaReel2 = reel2Arr[fixedSlotOutcome[2]];
  }
  if (rngWildCount == 3) {           // !!
    lhaReel3 = reel3Arr[fixedSlotOutcome[5]];
  }
  else {
    lhaReel3 = reel3Arr[fixedSlotOutcome[4]];
  }
  // Serial.println("lhaReel1: ");
  // Serial.println(lhaReel1);
  // Serial.println("lhaReel2: ");
  // Serial.println(lhaReel2);
  // Serial.println("lhaReel3: ");
  // Serial.println(lhaReel3);

} // end forceReelResult()

void initLHAChanceArray() {

  int lhaSpreadInc = 0; // for array traversing

  for (int i = lhaSpreadInc; i < lha3sAmt; i++) {
      lhaSpread[i] = 3;
      lhaSpreadInc++;
      
  }
  for (int i = lhaSpreadInc; i < lha3sAmt+lha2sAmt; i++) {
      lhaSpread[i] = 2;
      lhaSpreadInc++;
      
  }
  for (int i = lhaSpreadInc; i < lha3sAmt+lha2sAmt+lha1sAmt; i++) {
      lhaSpread[i] = 1;
      lhaSpreadInc++;
      
  }
  for (int i = lhaSpreadInc; i < lha3sAmt+lha2sAmt+lha1sAmt+lha0sAmt; i++) {
      lhaSpread[i] = 0;
      lhaSpreadInc++;
      
  }
} // end initLHAChanceArray()

void getPlayerInfo(String path, int from, int to) {
  String list[10];
  String data = "";
  int counter = 0;
  if(from > to) return list;
  //Serial.println(path);
  File file = SD.open(path);
  if(!file) {
    Serial.println("ERROR: Could not open file for reading.");
    return;
  }
  
  while(file.available() && counter <= to) {
    counter++;
    if(counter < from) {
      file.readStringUntil('\n');
    } else {
      data = file.readStringUntil('\n');
      data.trim();
      //Serial.println(data);
      assignPlayerInfoToGlobals(data, counter);
    }
  }
  file.close();
  displayWelcomeOnLCD();
  displayState = 1;
  return;
}

void assignPlayerInfoToGlobals(String playerInfo, int position) { 
  if(position == 1) {
    PID = playerInfo.toInt();
    playerPosition = PID - 1;
  }
  else if(position == 2) {
    name = playerInfo;
    moreInfoName = playerInfo;
  }
  else if(position == 3) {
    numCredits = playerInfo.toInt();
  }
  else if(position == 4) {
    playCount = playerInfo.toInt();
  }
  else if(position == 5) {
    winLoss = playerInfo.toInt();
  }
  else if(position == 6) {
    haywireCount = playerInfo.toInt();
  }
  else if(position == 7) {
    lhaCount = playerInfo.toInt();
  }
  else if(position == 8) {
    highScore = playerInfo.toInt();
  }
  else if(position == 9) {
    playerType = playerInfo;
  }
  else if(position == 10) {
    defaultTheme = playerInfo;
    themeID = getThemeFolder(defaultTheme);
  }
}

void writePlayerInfo(String path) {
  String data = convertPlayerInfoToDataString (PID, name, numCredits, playCount, winLoss, haywireCount, lhaCount, highScore, playerType, defaultTheme);
  int counter = 0;

  File file = SD.open(path, O_WRITE);
  if(!file) {
    Serial.println("ERROR: Could not open file for writing.");
    return;
  }
  else {
    file.print(data);
    file.close();
  }
}

String convertPlayerInfoToDataString (int PID, String name, int playerCredits, int playCount, int winLoss, int haywireCount, int lhaCount, int highScore, String playerType, String defaultTheme) {
  String lineDelimiter = "\n";

  return String(PID) + lineDelimiter +
         name + lineDelimiter +
         String(playerCredits) + lineDelimiter +
         String(playCount) + lineDelimiter +
         String(winLoss) + lineDelimiter +
         String(haywireCount) + lineDelimiter +
         String(lhaCount) + lineDelimiter +
         String(highScore) + lineDelimiter +
         playerType + lineDelimiter +
         defaultTheme + lineDelimiter;
}

void displayWelcomeOnLCD() {
    if (playCount > 0) {
      lcd.clear();
      printToLCD(0, 0, "Welcome back ");
      printToLCD(0, 1, name);
      delay(1500);
    }
    else {
      lcd.clear();
      printToLCD(0, 0, "Hello there ");
      printToLCD(0, 1, name);
      delay(1500);
    }
    reelSpinState = 1;
    playSound("welcome"); 
}

bool hasAdminPrivileges() {
  if (playerType.compareTo("Admin") == 0 || playerType.compareTo("Instructor") == 0 || playerType.compareTo("Tech") == 0 || playerType.compareTo("Security") == 0) {
    return true;
  }
  else {
    return false;
  }
}

int getSoundFile(String eventName) {
  if (eventName.compareTo("1 credit") == 0) {
    return 1;
  }
  else if (eventName.compareTo("2 credit") == 0) {
    return 2;
  }
  else if (eventName.compareTo("3 credit") == 0) {
    return 3;
  }
  else if (eventName.compareTo("spinReel 1") == 0) {
    return 4;
  }
  else if (eventName.compareTo("spinReel 2") == 0) {
    return 5;
  }
  else if (eventName.compareTo("spinReel 3") == 0) {
    return 6;
  }
  else if (eventName.compareTo("spinReel 4") == 0) {
    return 7;
  }
  else if (eventName.compareTo("winner") == 0) {
    return 10;
  }
  else if (eventName.compareTo("lha") == 0) {
    return 11;
  }
  else if (eventName.compareTo("cash out") == 0) {
    return 12;
  }
  else if (eventName.compareTo("welcome") == 0) {
    return 13;
  }
  else if (eventName.compareTo("haywire") == 0) {
    return 20;
  }
  else if (eventName.compareTo("prog-major") == 0) {
    return 98;
  }
  else if (eventName.compareTo("prog-grand") == 0) {
    return 99;
  }
  else {
    return 0;
  }
}

int getThemeFolder(String themeName) {
  if (themeName.compareTo("IGTLegacy") == 0) {
    return 1;
  }
  else if (themeName.compareTo("IGTEnhanced") == 0) {
    return 2;
  }
  else if (themeName.compareTo("TwinWin") == 0) {
    return 3;
  }
  else if (themeName.compareTo("GameKing") == 0) {
    return 4;
  }
  else if (themeName.compareTo("Aristocrat") == 0) {
    return 5;
  }
  else if (themeName.compareTo("Kendrick") == 0) {
    return 6;
  }
  else {  // DEFAULT TO 1 - IGTLegacy
    return 1;
  }
}

void playSound(String eventName, int delayTime = 0) {
  //player.playFolder(3, 98);
  Serial.println(getSoundFile(eventName));
  Serial.println(themeID);
  player.playFolder(themeID, getSoundFile(eventName));
  if (delayTime != 0) {
    delay(delayTime);
  }
}

/* Developer Notes

Parts list:

Arduino Mega (The original starter Uno doesn't have enough pins for the inputs needed below)
https://www.amazon.com/ELEGOO-Compatible-Arduino-Projects-Compliant/dp/B01H4ZLZLQ

LCD1502 Module (with pin header) - Should come w/ Arduino uno starter kit, otherwise try below?
https://www.amazon.com/GeeekPi-Character-Backlight-Raspberry-Electrical/dp/B07S7PJYM6

SD Card Reader Adapter
https://www.amazon.com/HiLetgo-Adater-Interface-Conversion-Arduino/dp/B07BJ2P6X6

Universal 4 key push module
https://www.amazon.com/Universal-Button-Switch-Keyboard-Arduino/dp/B07RLDTJT7

DFPlayer Mini
https://www.amazon.com/DFPlayer-A-Mini-MP3-Player/dp/B089D5NLW1

Micro SD Card (32 GB MAX DUE TO ARDUINO LIMITATION) //BU: Knowing this project, you might need extras...
https://www.amazon.com/PNY-Elite-microSDHC-Memory-5-Pack/dp/B09WW69YRD

Male-to-Male pin jumper wires
Male-to-Female pin jumper wires
Female-to-Female pin jumper wires
https://www.amazon.com/Elegoo-EL-CP-004-Multicolored-Breadboard-arduino/dp/B01EV70C78

USB-B to USB cable (for arduino code upload / debug ; should come with starter kit, otherwise:)
https://www.amazon.com/Amazon-Basics-External-Gold-Plated-Connectors/dp/B00NH11KIK

  Symbol Legend:

  0 - Blank            
  1 - Single Bar       (3 - 10x)
  2 - Double Bar       (3 - 25x)
  3 - Triple Bar       (3 - 40x)
  4 - Cherry           (1 - 2x, 2 - 5x, 3 - 10x)
  7 - Red Seven        (3 - 80x)
  8 - "Double Diamond" (Wild, 2x multi, 3 - 800x)
  9 - "DC"

  //FOR FUTURE DEVELOPMENT - Reserve for possible bonus symbols
  5 - Top Dollar
  6 - Pinball

//For those who inherit this code...
1) Good luck.
2) Yes, it's 1000+ lines, need to find time to re-sort code and modularize into .h/.cpp libraries

//Possible Bugs:
03/09/2027: Found a case where LHA -> DD,2Bar,2Bar -> Haywire, # of Haywires exceeded intended amount (s/b 2 at most)
03/11/2027: File output leaves trailing data not originally found in file (weird file append going on?)
          : Maybe fix by wiping file completely, then re-writing file from scratch?
03/11/2027: Found erratic error where file i/o would lock up with player tracking SD card
          : Fixed by restarting entire test setup (FM?)



//Docs:

//SD Card Tutorial
https://www.youtube.com/watch?v=1jUXlPP_xnI
https://github.com/HuHamster/sd-card-archive

--BU: For player tracking SD card, File system as follows:
root
 - [PLAYER NAME 1] - Folder
  - user.txt  - File
 - [PLAYER NAME 1]
  - user.txt
 .....
 - [PLAYER NAME #]
  - user.txt

User.txt format (\r\n delimited): 
int PID
String name
int playerCredits
int playCount
int winLoss
int haywireCount
int lhaCount
int highScore
String playerType
String defaultTheme

//DFPlayer Mini
https://wiki.dfrobot.com/dfr0299
https://www.youtube.com/watch?v=P42ICrgAtS4

--BU: For mp3 player SD card, File system as follows:
root
 - 01 - FOLDER
  - 001.mp3 - FILE
  - 002.mp3
  - 003.mp3
  - ....
  - 999.mp3
 - 02
 - ...
 - 99

--------------------------------------------------

// Test scenarios (devReel1, devReel2, devReel3)
48, 48, 48 - 3 Double Diamonds
49, 49, 49 - 3 DCs
44, 45, 47 - 3 7s
22, 49, 0  - 1 DC
48, 48, 47 - 2x Double Diamond, 1 7 (4x multi test)
0, 21, 0   - 1 Cherry
29, 35, 32 - Any Bar
22, 22, 22 - 3 Single Bars
31, 35, 40 - 3 Double Bars
40, 39, 43 - 3 Triple Bars
49, 21, 49 - 2 DCs, 1 Cherry (Insane haywire condition)
21, 21, 49 - 1 DCs, 2 Cherry (Another insane haywire condition)


44, 45, 47 - 3 7s
0, 21, 0   - 1 Cherry
21, 21, 0 - 1 DCs, 2 Cherry (Another insane haywire condition)

48, 48, 47 - 2x Double Diamond, 1 7 (4x multi test)

//ARDUINO PIN ASSIGNMENT (UNO / MEGA)

4-BUTTON KEY PINS:
GRD - Gnd
K1 - Pin INPUT_PULLUP 3 / 40
K2 - Pin INPUT_PULLUP 2 / 41
K3 - Pin INPUT_PULLUP 0 / 42
K4 - Pin INPUT_PULLUP 1 / 43

LCD PINS:
1 - Gnd
2 - 5v
3 - Potentiometer
4 - rs (pin in 4 / 48)
5 - Gnd
6 - en (pin in 5 / 49)
7 - 
8 - 
9 - 
10 - 
11 - d4 (pin in 6 / 44)
12 - d5 (pin in 7 / 45)
13 - d6 (pin in 8 / 46)
14 - d7 (pin in 9 / 47)
15 - 5v
16 - Gnd

SD CARD PINS
Gnd - Gnd
VCC - 5v
MISO - pin in 12 / 50 (FIXED)
MOSI - pin in 11 / 51 (FIXED)
SCK - pin in 13 / 52 (FIXED)
CS - pin in 10  / 53

DFPLAYER MINI PINS (ORIENTATION WHERE SD CARD FACES TOWARD YOU)
LEFT (TOP TO BOTTOM)
1 - VCC (5v) ; REQUIRED
  - Input Voltage
2 - RX ; REQUIRED        //BU NEED TO USE 1K RESISTOR BEFORE FEEDING INTO DFPLAYER PIN
  - UART serial in
  - PIN IN - / 10
3 - TX ; REQUIRED
  - UART serial out
  - PIN IN - / 11
4 - DAC_R
  - Audio output R
5 - DAC_L
  - Audio output L
6 - SPK2 / Speaker-
  - Drive speaker < 3W ; REQUIRED
    - Red wire of speaker (I know, polarities are switched, but it worked in testing...)
7 - Gnd ; REQUIRED
8 - SPK1 / Speaker+ ; REQUIRED
  - Drive speaker < 3W
    - Black wire of speaker (I know, polarities are switched, but it worked in testing...)

RIGHT (TOP TO BOTTOM)
1 - BUSY
  - Playing status (INPUT_PULLUP, 0=Active, 1=Inactive)
2 - USB-
  - USB- DM
3 - USB+
  - USB+ DP
4 - ADKEY2 / AD Port 2
  - Trigger play fifth segment
5 - ADKEY1 / AD Port 1
  - Trigger play first segment
6 - IO2
  - Trigger port (Short press = play previous, Long press = decrease vol.)
7 - Gnd
8 - IO1
  - Trigger port (Short press = play next, Long press = increase vol.)


//Sound mapping info (how files are organized in SD card for DFPlayer Mini)
//FILE LIST (001.mp3, 002.mp3, 003.mp3, etc..)
1 - 1 credit
2 - 2 credit
3 - 3 credit
4 - spinReel 1
5 - spinReel 2
6 - spinReel 3
7 - spinReel 4
10 - Winner
11 - LHA
12 - Cash Out
13 - Welcome
20 - Haywire
98 - Progressive (Major)
99 - Progressive (Grand)

//FOLDER LIST (01, 02, 03...)
1 - IGTLegacy
2 - IGTEnhanced
3 - TwinWin
4 - GameKing
5 - Aristocrat
6 - Kendrick

*/