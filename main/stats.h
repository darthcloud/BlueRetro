/* Functions to calculate Average, Min, Max, and Std Dev */
/* Taken from Arduino lag test code by @jorge_: https://pastebin.com/ktqn7izx */

#ifndef _STATS_H_
#define _STATS_H_

float getAverage(uint32_t newNum);
float getMax(uint32_t newNum);
float getMin(uint32_t newNum);
float getStdDev(uint32_t newNum);

#endif /* _STATS_H_ */
