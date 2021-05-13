
#include "alport.h"

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

