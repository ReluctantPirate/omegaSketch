byte teamHues[8] = {0, 0, 21, 42, 85, 110, 170, 210};

#define RESET 7
#define FIELD 0

byte team = FIELD;

bool isPuck = false;

enum comStates {INERT, GO, RESOLVE, LASER};
byte comState[6] = {INERT, INERT, INERT, INERT, INERT, INERT};

Timer swarmTimer;

void setup() {
  // put your setup code here, to run once:
  randomize();
}

void loop() {

  if (isPuck) {
    puckLoop();
  } else {
    fieldLoop();
  }

  //listen for reset
  if (buttonMultiClicked() && buttonClickCount > 2) {
    setFullComState(GO);
    team = RESET;
    isPuck = false;
  }

  //visuals
  if (isPuck) {
    puckDisplay();
  } else {
    fieldDisplay();
  }

  //begin communication
  FOREACH_FACE(f) {
    byte sendVal = (isPuck << 5) | (team << 2) | (comState[f]);
    setValueSentOnFace(sendVal, f);
  }
}

void puckLoop() {
  if (buttonSingleClicked() && isAlone()) {
    team++;
    if (team == 7) {
      team = 1;
    }
  }

  if (isAlone()) {
    comState[0] = LASER;
    comState[1] = LASER;
    comState[2] = LASER;
    comState[3] = LASER;
    comState[4] = LASER;
    comState[5] = LASER;
  }

  FOREACH_FACE(f) {
    switch (comState[f]) {
      case INERT:
        inertFaceLoop(f);
        break;
      case GO:
        goFaceLoop(f);
        break;
      case RESOLVE:
        resolveFaceLoop(f);
        break;
      case LASER:
        laserFaceLoop(f);
        break;
    }
  }
}

void fieldLoop() {
  if (buttonSingleClicked() && isAlone()) {
    //become a puck
    isPuck = true;
    team = random(5) + 1;
    setFullComState(INERT);
    comState[0] = LASER;
    comState[3] = LASER;
  }



  byte teamCount[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  FOREACH_FACE(f) {

    //first, run signal loops
    switch (comState[f]) {
      case INERT:
        inertFaceLoop(f);
        break;
      case GO:
        goFaceLoop(f);
        break;
      case RESOLVE:
        resolveFaceLoop(f);
        break;
      case LASER:
        laserFaceLoop(f);
        break;
    }

    //second, let's do the game of life stuff
    if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getIsPuck(neighborData) == false) {
        //log their color
        teamCount[getTeam(neighborData)]++;
      }
    }
  }

  if (team != RESET) {
    //now we'll resolve game of life stuff
    //search for 3s within the team count
    byte threesFound = 0;
    byte threesTeam = 0;
    FOREACH_FACE(f) {
      if (teamCount[f + 1] > 4) {
        threesFound++;
        threesTeam = f + 1;
      }
    }

    if (threesFound == 1 && threesTeam != team) {//this is the special swarm transition
      team = threesTeam;
      if (swarmTimer.isExpired()) {
        swarmTimer.set(1000);
      }
    }
  }
}

void inertFaceLoop (byte face) {
  //check if I have anyone on that face
  if (!isValueReceivedOnFaceExpired(face)) {//ooh, a neighbor!
    byte neighborData = getLastValueReceivedOnFace(face);
    if (getComState(neighborData) == GO) {
      //ooh, they want me to DO something
      if (getTeam(neighborData) == 7) {
        //in this case, I need to go into FULL reset mode
        setFullComState(GO);
        team = RESET;
        isPuck = false;
      } else if (getTeam(neighborData) != FIELD) {
        //this is spreading a color on a specific face
        //so change to that color and set some stuff on the opposite face
        team = getTeam(neighborData);
        comState[face] = GO;
        comState[(face + 3) % 6] = GO;
      }
    }
  }
}

void setFullComState(byte setting) {//convenience function
  FOREACH_FACE(f) {
    comState[f] = setting;
  }
}

void goFaceLoop(byte face) {
  //so here all I care about is: has my neighbor heard the news?
  comState[face] = RESOLVE;//do this by default, assume it's happening

  //check to see if this neighbor is still in INERT just in case
  if (!isValueReceivedOnFaceExpired(face)) {//a neighbor!
    byte neighborData = getLastValueReceivedOnFace(face);
    if (getComState(neighborData) == INERT) {
      //nope, not time to go to RESOLVE
      comState[face] = GO;
    }
  }

}

void resolveFaceLoop(byte face) {
  //this is very similar to the one above
  //so here all I care about is: has my neighbor heard the news?
  comState[face] = INERT;//do this by default, assume it's happening

  //check to see if this neighbor is still in INERT just in case
  if (!isValueReceivedOnFaceExpired(face)) {//a neighbor!
    byte neighborData = getLastValueReceivedOnFace(face);
    if (getComState(neighborData) == GO) {
      //nope, not time to go to INERT
      comState[face] = RESOLVE;
    }
  }

  //deal with the RESET >> FIELD transition
  if (team == RESET) {
    team = FIELD;
  }
}

void laserFaceLoop(byte face) {
  //so all I do here is check to see if I'm touching something so I can change to GO
  if (!isValueReceivedOnFaceExpired(face)) { //ooh, a neighbor!
    comState[face] = GO;
  }
}

void fieldDisplay() {

  //determine swarm saturation
  byte sat = 255;
  if (swarmTimer.isExpired() == false) {
    sat = 255 - map(swarmTimer.getRemaining(), 0, 1000, 0, 255);
  }

  FOREACH_FACE(f) {
    switch (comState[f]) {
      case INERT:
        if (team == FIELD) {
          setColorOnFace(dim(WHITE, 155), f);
        } else {
          setColorOnFace(makeColorHSB(teamHues[team], sat, 255), f);
        }
        break;
      case GO:
      case RESOLVE:
        setColorOnFace(WHITE, f);
        break;
    }
  }

}

void puckDisplay() {
  setColor(makeColorHSB(teamHues[team], (millis() / 3) % 255, 255));
}

bool getIsPuck(byte val) {
  return (val >> 5);
}

byte getTeam(byte val) {
  return ((val >> 2) & 7);
}

byte getComState(byte val) {
  return (val & 3);
}
