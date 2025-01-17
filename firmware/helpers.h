#include "config.h"

void printHallValues(bool hallValues[NUMSTEPS*4]) {
  String firstRow = "";
  for (auto i = 0; i < NUMSTEPS; i++) {
    firstRow += " ";
    firstRow += String(hallValues[i * 4]);
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
  }

  String secondRow = "";
  for (auto i = 0; i < 8; i++) {
    secondRow += String(hallValues[i * 4 + 3]);
    secondRow += " ";
    secondRow += String(hallValues[i * 4 + 1]);
    secondRow += " ";
    secondRow += " ";
    secondRow += " ";
    secondRow += " ";
    secondRow += " ";
  }

  String thirdRow = "";
  for (auto i = 0; i < 8; i++) {
    thirdRow += " ";
    thirdRow += String(hallValues[i * 4 + 2]);
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
  }

  Serial.println(firstRow);
  Serial.println(secondRow);
  Serial.println(thirdRow);
  Serial.println();
}