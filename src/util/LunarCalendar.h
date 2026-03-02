#pragma once

/**
 * Vietnamese Lunar Calendar conversion utility.
 * Based on Ho Ngoc Duc's algorithm (https://www.informatik.uni-leipzig.de/~duc/amlich/)
 */
struct LunarDate {
  int day;
  int month;
  int year;
  bool isLeapMonth;
};

// Convert a Gregorian date to Vietnamese lunar date.
// timeZone: UTC offset in hours (Vietnam = 7.0)
LunarDate solarToLunar(int day, int month, int year, double timeZone = 7.0);
