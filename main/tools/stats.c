/* Functions to calculate Average, Min, Max, and Std Dev */
/* Taken from Arduino lag test code by @jorge_: https://pastebin.com/ktqn7izx */

#include <stdint.h>
#include <math.h>

float getAverage(uint32_t newNum){
  static uint32_t numSamples = 1;
  static float curAvg;

  curAvg = curAvg + (newNum - curAvg) / numSamples;
  numSamples++;

  return curAvg;
}

float getMax(uint32_t newNum){
  static uint32_t maxVal = 0;

  if(newNum > maxVal){
    maxVal = newNum;
  }

  return maxVal;
}

float getMin(uint32_t newNum){
  static uint32_t minVal = 0xFFFFFFFF;

  if(newNum < minVal){
    minVal = newNum;
  }

  return minVal;
}

float getStdDev(uint32_t newNum){
    static float M = 0.0;
    static float S = 0.0;
    static uint32_t i = 1;

    float tmpM = M;
    M += (newNum - tmpM) / i;
    S += (newNum - tmpM) * (newNum - M);
    i++;

    return sqrt(S/(i-2));
}
