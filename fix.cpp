
#include "alport.h"

fixed _cos_tbl[512] =
{
   /* precalculated fixed point (16.16) cosines for a full circle (0-255) */

   65536L,  65531L,  65516L,  65492L,  65457L,  65413L,  65358L,  65294L,
   65220L,  65137L,  65043L,  64940L,  64827L,  64704L,  64571L,  64429L,
   64277L,  64115L,  63944L,  63763L,  63572L,  63372L,  63162L,  62943L,
   62714L,  62476L,  62228L,  61971L,  61705L,  61429L,  61145L,  60851L,
   60547L,  60235L,  59914L,  59583L,  59244L,  58896L,  58538L,  58172L,
   57798L,  57414L,  57022L,  56621L,  56212L,  55794L,  55368L,  54934L,
   54491L,  54040L,  53581L,  53114L,  52639L,  52156L,  51665L,  51166L,
   50660L,  50146L,  49624L,  49095L,  48559L,  48015L,  47464L,  46906L,
   46341L,  45769L,  45190L,  44604L,  44011L,  43412L,  42806L,  42194L,
   41576L,  40951L,  40320L,  39683L,  39040L,  38391L,  37736L,  37076L,
   36410L,  35738L,  35062L,  34380L,  33692L,  33000L,  32303L,  31600L,
   30893L,  30182L,  29466L,  28745L,  28020L,  27291L,  26558L,  25821L,
   25080L,  24335L,  23586L,  22834L,  22078L,  21320L,  20557L,  19792L,
   19024L,  18253L,  17479L,  16703L,  15924L,  15143L,  14359L,  13573L,
   12785L,  11996L,  11204L,  10411L,  9616L,   8820L,   8022L,   7224L,
   6424L,   5623L,   4821L,   4019L,   3216L,   2412L,   1608L,   804L,
   0L,      -804L,   -1608L,  -2412L,  -3216L,  -4019L,  -4821L,  -5623L,
   -6424L,  -7224L,  -8022L,  -8820L,  -9616L,  -10411L, -11204L, -11996L,
   -12785L, -13573L, -14359L, -15143L, -15924L, -16703L, -17479L, -18253L,
   -19024L, -19792L, -20557L, -21320L, -22078L, -22834L, -23586L, -24335L,
   -25080L, -25821L, -26558L, -27291L, -28020L, -28745L, -29466L, -30182L,
   -30893L, -31600L, -32303L, -33000L, -33692L, -34380L, -35062L, -35738L,
   -36410L, -37076L, -37736L, -38391L, -39040L, -39683L, -40320L, -40951L,
   -41576L, -42194L, -42806L, -43412L, -44011L, -44604L, -45190L, -45769L,
   -46341L, -46906L, -47464L, -48015L, -48559L, -49095L, -49624L, -50146L,
   -50660L, -51166L, -51665L, -52156L, -52639L, -53114L, -53581L, -54040L,
   -54491L, -54934L, -55368L, -55794L, -56212L, -56621L, -57022L, -57414L,
   -57798L, -58172L, -58538L, -58896L, -59244L, -59583L, -59914L, -60235L,
   -60547L, -60851L, -61145L, -61429L, -61705L, -61971L, -62228L, -62476L,
   -62714L, -62943L, -63162L, -63372L, -63572L, -63763L, -63944L, -64115L,
   -64277L, -64429L, -64571L, -64704L, -64827L, -64940L, -65043L, -65137L,
   -65220L, -65294L, -65358L, -65413L, -65457L, -65492L, -65516L, -65531L,
   -65536L, -65531L, -65516L, -65492L, -65457L, -65413L, -65358L, -65294L,
   -65220L, -65137L, -65043L, -64940L, -64827L, -64704L, -64571L, -64429L,
   -64277L, -64115L, -63944L, -63763L, -63572L, -63372L, -63162L, -62943L,
   -62714L, -62476L, -62228L, -61971L, -61705L, -61429L, -61145L, -60851L,
   -60547L, -60235L, -59914L, -59583L, -59244L, -58896L, -58538L, -58172L,
   -57798L, -57414L, -57022L, -56621L, -56212L, -55794L, -55368L, -54934L,
   -54491L, -54040L, -53581L, -53114L, -52639L, -52156L, -51665L, -51166L,
   -50660L, -50146L, -49624L, -49095L, -48559L, -48015L, -47464L, -46906L,
   -46341L, -45769L, -45190L, -44604L, -44011L, -43412L, -42806L, -42194L,
   -41576L, -40951L, -40320L, -39683L, -39040L, -38391L, -37736L, -37076L,
   -36410L, -35738L, -35062L, -34380L, -33692L, -33000L, -32303L, -31600L,
   -30893L, -30182L, -29466L, -28745L, -28020L, -27291L, -26558L, -25821L,
   -25080L, -24335L, -23586L, -22834L, -22078L, -21320L, -20557L, -19792L,
   -19024L, -18253L, -17479L, -16703L, -15924L, -15143L, -14359L, -13573L,
   -12785L, -11996L, -11204L, -10411L, -9616L,  -8820L,  -8022L,  -7224L,
   -6424L,  -5623L,  -4821L,  -4019L,  -3216L,  -2412L,  -1608L,  -804L,
   0L,      804L,    1608L,   2412L,   3216L,   4019L,   4821L,   5623L,
   6424L,   7224L,   8022L,   8820L,   9616L,   10411L,  11204L,  11996L,
   12785L,  13573L,  14359L,  15143L,  15924L,  16703L,  17479L,  18253L,
   19024L,  19792L,  20557L,  21320L,  22078L,  22834L,  23586L,  24335L,
   25080L,  25821L,  26558L,  27291L,  28020L,  28745L,  29466L,  30182L,
   30893L,  31600L,  32303L,  33000L,  33692L,  34380L,  35062L,  35738L,
   36410L,  37076L,  37736L,  38391L,  39040L,  39683L,  40320L,  40951L,
   41576L,  42194L,  42806L,  43412L,  44011L,  44604L,  45190L,  45769L,
   46341L,  46906L,  47464L,  48015L,  48559L,  49095L,  49624L,  50146L,
   50660L,  51166L,  51665L,  52156L,  52639L,  53114L,  53581L,  54040L,
   54491L,  54934L,  55368L,  55794L,  56212L,  56621L,  57022L,  57414L,
   57798L,  58172L,  58538L,  58896L,  59244L,  59583L,  59914L,  60235L,
   60547L,  60851L,  61145L,  61429L,  61705L,  61971L,  62228L,  62476L,
   62714L,  62943L,  63162L,  63372L,  63572L,  63763L,  63944L,  64115L,
   64277L,  64429L,  64571L,  64704L,  64827L,  64940L,  65043L,  65137L,
   65220L,  65294L,  65358L,  65413L,  65457L,  65492L,  65516L,  65531L
};


fix::fix() : v(0)                              {}
fix::fix(const fix &x) : v(x.v)                {}
fix::fix(const int x) : v(itofix(x))           {}
fix::fix(const long x) : v(itofix(x))          {}
fix::fix(const unsigned int x) : v(itofix(x))  {}
fix::fix(const unsigned long x) : v(itofix(x)) {}
fix::fix(const float x) : v(ftofix(x))         {}
fix::fix(const double x) : v(ftofix(x))        {}

fix::operator int() const
{
   return fixtoi(v);
}

fix::operator long() const
{
   return fixtoi(v);
}

fix::operator unsigned int() const
{
   return fixtoi(v);
}

fix::operator unsigned long() const
{
   return fixtoi(v);
}

fix::operator float() const
{
   return fixtof(v);
}

fix::operator double() const
{
   return fixtof(v);
}

fix &fix::operator = (const fix &x)
{
   v = x.v;
   return *this;
}

fix &fix::operator = (const int x)
{
   v = itofix(x);
   return *this;
}

fix &fix::operator = (const long x)
{
   v = itofix(x);
   return *this;
}

fix &fix::operator = (const unsigned int x)
{
   v = itofix(x);
   return *this;
}

fix &fix::operator = (const unsigned long x)
{
   v = itofix(x);
   return *this;
}

fix &fix::operator = (const float x)
{
   v = ftofix(x);
   return *this;
}

fix &fix::operator = (const double x)
{
   v = ftofix(x);
   return *this;
}

fix &fix::operator += (const fix x)
{
   v += x.v;
   return *this;
}

fix &fix::operator += (const int x)
{
   v += itofix(x);
   return *this;
}

fix &fix::operator += (const long x)
{
   v += itofix(x);
   return *this;
}

fix &fix::operator += (const float x)
{
   v += ftofix(x);
   return *this;
}

fix &fix::operator += (const double x)
{
   v += ftofix(x);
   return *this;
}

fix &fix::operator -= (const fix x)
{
   v -= x.v;
   return *this;
}

fix &fix::operator -= (const int x)
{
   v -= itofix(x);
   return *this;
}

fix &fix::operator -= (const long x)
{
   v -= itofix(x);
   return *this;
}

fix &fix::operator -= (const float x)
{
   v -= ftofix(x);
   return *this;
}

fix &fix::operator -= (const double x)
{
   v -= ftofix(x);
   return *this;
}

fix &fix::operator *= (const fix x)
{
   v = fixmul(v, x.v);
   return *this;
}

fix &fix::operator *= (const int x)
{
   v *= x;
   return *this;
}

fix &fix::operator *= (const long x)
{
   v *= x;
   return *this;
}

fix &fix::operator *= (const float x)
{
   v = ftofix(fixtof(v) * x);
   return *this;
}

fix &fix::operator *= (const double x)
{
   v = ftofix(fixtof(v) * x);
   return *this;
}

fix &fix::operator /= (const fix x)
{
   v = fixdiv(v, x.v);
   return *this;
}

fix &fix::operator /= (const int x)
{
   v /= x;
   return *this;
}

fix &fix::operator /= (const long x)
{
   v /= x;
   return *this;
}

fix &fix::operator /= (const float x)
{
   v = ftofix(fixtof(v) / x);
   return *this;
}

fix &fix::operator /= (const double x)
{
   v = ftofix(fixtof(v) / x);
   return *this;
}

fix &fix::operator <<= (const int x)
{
   v <<= x;
   return *this;
}

fix &fix::operator >>= (const int x)
{
   v >>= x;
   return *this;
}

fix &fix::operator ++ ()
{
   v += itofix(1);
   return *this;
}

fix &fix::operator -- ()
{
   v -= itofix(1);
   return *this;
}

fix fix::operator ++ (int)
{
   fix t;
   t.v = v;
   v += itofix(1);
   return t;
}

fix fix::operator -- (int)
{
   fix t;
   t.v = v;
   v -= itofix(1);
   return t;
}

fix fix::operator - () const
{
   fix t;
   t.v = -v;
   return t;
}

