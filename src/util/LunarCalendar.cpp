#include "LunarCalendar.h"

#include <cmath>

// ── Internal helpers (Ho Ngoc Duc algorithm) ─────────────────────────────────

static double jdFromGregorian(int d, int m, int y) {
  int a = (14 - m) / 12;
  int yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

static double newMoon(int k) {
  const double dr = M_PI / 180.0;
  double T  = k / 1236.85;
  double T2 = T * T;
  double T3 = T2 * T;

  double Jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
  Jd1 += 0.00033 * sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);

  double M   = 359.2242    + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3;
  double Mpr = 306.0253    + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3;
  double F   = 21.2964     + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3;

  double C1 = (0.1734 - 0.000393 * T) * sin(M * dr) + 0.0021 * sin(2 * dr * M);
  C1 -= 0.4068 * sin(Mpr * dr) + 0.0161 * sin(dr * 2 * Mpr);
  C1 -= 0.0004 * sin(dr * 3 * Mpr);
  C1 += 0.0104 * sin(dr * 2 * F) - 0.0051 * sin(dr * (M + Mpr));
  C1 -= 0.0074 * sin(dr * (M - Mpr)) + 0.0004 * sin(dr * (2 * F + M));
  C1 -= 0.0004 * sin(dr * (2 * F - M)) - 0.0006 * sin(dr * (2 * F + Mpr));
  C1 += 0.0010 * sin(dr * (2 * F - Mpr)) + 0.0005 * sin(dr * (M + 2 * Mpr));

  double deltat;
  if (T < -11) {
    deltat = 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3;
  } else {
    deltat = -0.000278 + 0.000265 * T + 0.000262 * T2;
  }
  return Jd1 + C1 - deltat;
}

static double sunLongitudeDeg(double jdn) {
  const double dr = M_PI / 180.0;
  double T  = (jdn - 2451545.0) / 36525.0;
  double T2 = T * T;
  double M  = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
  double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
  double DL = (1.9146 - 0.004817 * T - 0.000014 * T2) * sin(dr * M)
            + (0.019993 - 0.000101 * T) * sin(dr * 2 * M)
            + 0.00029 * sin(dr * 3 * M);
  double L = L0 + DL;
  L = L / 360.0 - floor(L / 360.0);
  return L * 360.0;
}

// Returns solar term index (0-11) for given JDN in given timezone
static int solarTermAt(double jdn, double tz) {
  return (int)(sunLongitudeDeg(jdn - 0.5 - tz / 24.0) / 30.0);
}

// Returns JDN of k-th new moon (adjusted for timezone)
static int newMoonDay(int k, double tz) {
  return (int)(newMoon(k) + 0.5 + tz / 24.0);
}

// JDN of the 11th lunar month start for given Gregorian year
static int lunarMonth11Start(int yy, double tz) {
  double off = jdFromGregorian(31, 12, yy) - 2415021.076998695;
  int k = (int)(off / 29.530588853);
  int nm = newMoonDay(k, tz);
  if (solarTermAt(nm, tz) >= 9) nm = newMoonDay(k - 1, tz);
  return nm;
}

// Offset of the leap month within the 13-month year starting at a11
static int leapMonthOffset(int a11, double tz) {
  int k = (int)((a11 - 2415021.076998695) / 29.530588853 + 0.5);
  int i = 1;
  int arc = solarTermAt(newMoonDay(k + i, tz), tz);
  int last;
  do {
    last = arc;
    i++;
    arc = solarTermAt(newMoonDay(k + i, tz), tz);
  } while (arc != last && i < 14);
  return i - 1;
}

// ── Public API ────────────────────────────────────────────────────────────────

LunarDate solarToLunar(int day, int month, int year, double timeZone) {
  int dayNumber   = (int)jdFromGregorian(day, month, year);
  int k           = (int)((dayNumber - 2415021.076998695) / 29.530588853);
  int monthStart  = newMoonDay(k + 1, timeZone);
  if (monthStart > dayNumber) monthStart = newMoonDay(k, timeZone);

  int a11 = lunarMonth11Start(year, timeZone);
  int b11 = a11;
  int lunarYear;
  if (a11 >= monthStart) {
    lunarYear = year;
    a11 = lunarMonth11Start(year - 1, timeZone);
  } else {
    lunarYear = year + 1;
    b11 = lunarMonth11Start(year + 1, timeZone);
  }

  int lunarDay   = dayNumber - monthStart + 1;
  int diff       = (int)((monthStart - a11) / 29);
  int lunarMonth = diff + 11;
  bool isLeap    = false;

  if (b11 - a11 > 365) {
    int leapOff = leapMonthOffset(a11, timeZone);
    if (diff >= leapOff) {
      lunarMonth = diff + 10;
      if (diff == leapOff) isLeap = true;
    }
  }

  if (lunarMonth > 12) lunarMonth -= 12;
  if (lunarMonth >= 11 && diff < 4) lunarYear -= 1;

  return {lunarDay, lunarMonth, lunarYear, isLeap};
}
