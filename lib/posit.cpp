#include "posit.h"
#include "util.h"
#include "pack.h"

#include <cstdio>
#include <cmath>

using namespace std;

void Posit::fromIeee(uint64_t fbits, int fes, int ffs)
{
    int fexpbias = POW2(fes - 1) - 1;
    int16_t fexp = (fbits >> ffs) & ((1 << fes) - 1);
    uint64_t ffrac = fbits & ((1ULL << ffs) - 1);

    // clip exponent
    int rminfexp = POW2(mEs) * (-mNbits + 2);
    int rmaxfexp = POW2(mEs) * (mNbits - 2);
    int rfexp = MIN(MAX(fexp - fexpbias, rminfexp), rmaxfexp);

    unpkd_posit_t up;

    up.neg = fbits >> (fes + ffs),
    up.reg = rfexp >> mEs; // floor(rfexp / 2^mEs),
    up.exp = rfexp - POW2(mEs) * up.reg;
    if (ffs <= POSIT_SIZE) {
        up.frac = ffrac << (POSIT_SIZE - ffs);
    } else {
        up.frac = ffrac >> (ffs - POSIT_SIZE);
    }

    mBits = pack_posit(up, mNbits, mEs);
}

Posit::Posit(POSIT_UTYPE bits, int nbits, int es, bool nan) :
    mBits(bits),
    mNbits(nbits),
    mEs(es),
    mNan(nan)
{
}

Posit::Posit(int nbits, int es, bool nan) :
    Posit(0, nbits, es, nan)
{
}

Posit::Posit(int nbits, int es) :
    Posit(nbits, es, false)
{
}

bool Posit::isZero()
{
    return util_is_zero(mBits);
}

bool Posit::isOne()
{
    return util_is_one(mBits);
}

bool Posit::isInf()
{
    return util_is_inf(mBits);
}

bool Posit::isNeg()
{
    return util_is_neg(mBits);
}

bool Posit::isNan()
{
    return mNan;
}

int Posit::nbits()
{
    return mNbits;
}

int Posit::ss()
{
    return util_ss();
}

int Posit::rs()
{
    return util_rs(mBits, mNbits);
}

int Posit::es()
{
    return util_es(mBits, mNbits, mEs);
}

int Posit::fs()
{
    return util_fs(mBits, mNbits, mEs);
}

int Posit::useed()
{
    return POW2(POW2(mEs));
}

Posit Posit::zero()
{
    return Posit(POSIT_ZERO, mNbits, mEs, false);
}

Posit Posit::one()
{
    return Posit(POSIT_ONE, mNbits, mEs, false);
}

Posit Posit::inf()
{
    return Posit(POSIT_INF, mNbits, mEs, false);
}

Posit Posit::nan()
{
    return Posit(mNbits, mEs, true);
}

Posit Posit::neg()
{
    return Posit(util_neg(mBits, mNbits), mNbits, mEs, false);
}

Posit Posit::rec()
{
    return Posit(util_rec(mBits, mNbits, mEs), mNbits, mEs, false);
}

Posit Posit::add(Posit& p)
{
    // fast exit
    if (isZero()) {
        return p;
    } else if (p.isZero()) {
        return *this;
    } else if (isInf() && p.isInf()) {
        return nan();
    } else if (isInf() || p.isInf()) {
        return inf();
    } else if (neg().eq(p)) {
        return zero();
    }

    // TODO implement
    return *this;
}

Posit Posit::sub(Posit& p)
{
    // no loss on negation
    Posit np = p.neg();

    return add(np);
}

Posit Posit::mul(Posit& p)
{
    // fast exit
    if (isZero()) {
        return (p.isInf() ? nan() : zero());
    } else if (p.isZero()) {
        return (isInf() ? nan() : zero());
    } else if (isOne()) {
        return (isNeg() ? p.neg() : p);
    } else if (p.isOne()) {
        return (p.isNeg() ? neg() : *this);
    } else if (isInf() || p.isInf()) {
        return inf();
    } else if (rec().eq(p)) {
        return one();
    } else if (rec().neg().eq(p)) {
        return one().neg();
    }

    unpkd_posit_t up;
    unpkd_posit_t xup = unpack_posit(mBits, mNbits, mEs);
    unpkd_posit_t pup = unpack_posit(p.mBits, p.mNbits, p.mEs);

    int xfexp = POW2(mEs) * xup.reg + xup.exp;
    int pfexp = POW2(p.mEs) * pup.reg + pup.exp;

    // fractions have a hidden bit
    POSIT_LUTYPE xfrac = POSIT_MSB | (xup.frac >> 1);
    POSIT_LUTYPE pfrac = POSIT_MSB | (pup.frac >> 1);
    POSIT_UTYPE mfrac = (xfrac * pfrac) >> POSIT_SIZE;

    // shift is either 0 or 1
    int shift = CLZ(mfrac);

    // clip exponent to avoid underflow and overflow
    int rminfexp = POW2(mEs) * (-mNbits + 2);
    int rmaxfexp = POW2(mEs) * (mNbits - 2);
    int rfexp = MIN(MAX(xfexp + pfexp - shift + 1, rminfexp), rmaxfexp);

    up.neg = isNeg() ^ p.isNeg();
    up.reg = rfexp >> mEs; // floor(rfexp / 2^mEs)
    up.exp = rfexp - POW2(mEs) * up.reg;
    up.frac = mfrac << (shift + 1);

    return Posit(pack_posit(up, mNbits, mEs), mNbits, mEs, false);
}

Posit Posit::div(Posit& p)
{
    // no loss on reciprocation!
    Posit rp = p.rec();

    return mul(rp);
}

bool Posit::eq(Posit& p)
{
    return mBits == p.mBits;
}

bool Posit::gt(Posit& p)
{
    if (isInf() || p.isInf()) {
        return false;
    }

    return mBits > p.mBits;
}

bool Posit::ge(Posit& p)
{
    return gt(p) || eq(p);
}

bool Posit::lt(Posit& p)
{
    return !gt(p) && !eq(p);
}

bool Posit::le(Posit& p)
{
    return !gt(p);
}

void Posit::set(float n)
{
    union {
        float f;
        uint32_t bits;
    };

    switch (fpclassify(n)) {
    case FP_INFINITE:
        mBits = POSIT_INF;
        mNan = false;
        break;
    case FP_NAN:
        mNan = true;
        break;
    case FP_ZERO:
        mBits = POSIT_ZERO;
        mNan = false;
        break;
    case FP_SUBNORMAL:
        // TODO: support subnormals
        mBits = POSIT_ZERO;
        mNan = false;
        break;
    case FP_NORMAL:
        f = n;
        fromIeee(bits, 8, 23);
        mNan = false;
        break;
    }
}

void Posit::set(double n)
{
    union {
        double f;
        uint64_t bits;
    };

    switch (fpclassify(n)) {
    case FP_INFINITE:
        mBits = POSIT_INF;
        mNan = false;
        break;
    case FP_NAN:
        mNan = true;
        break;
    case FP_ZERO:
        mBits = POSIT_ZERO;
        mNan = false;
        break;
    case FP_SUBNORMAL:
        // TODO: support subnormals
        mBits = POSIT_ZERO;
        mNan = false;
        break;
    case FP_NORMAL:
        f = n;
        fromIeee(bits, 11, 52);
        mNan = false;
        break;
    }
}

float Posit::getFloat()
{
    if (isZero()) {
        return 0.f;
    } else if (isInf()) {
        return 1.f / 0.f;
    } else if (isNan()) {
        return 0.f / 0.f;
    }

    return pack_float(unpack_posit(mBits, mNbits, mEs), mEs);
}

double Posit::getDouble()
{
    if (isZero()) {
        return 0.0;
    } else if (isInf()) {
        return 1.0 / 0.0;
    } else if (isNan()) {
        return 0.0 / 0.0;
    }

    return pack_double(unpack_posit(mBits, mNbits, mEs), mEs);
}

void Posit::setBits(POSIT_UTYPE bits)
{
    mBits = bits << (POSIT_SIZE - mNbits);
}

POSIT_UTYPE Posit::getBits()
{
    return mBits >> (POSIT_SIZE - mNbits);
}

void Posit::print()
{
    Posit p = isNeg() || isInf() ? neg() : *this;

    printf("{%d, %d} ", mNbits, mEs);

    for (int i = POSIT_SIZE - 1; i >= POSIT_SIZE - mNbits; i--) {
        printf("%d", (mBits >> i) & 1);
    }

    printf(" -> ");
    printf(isNeg() || isInf() ? "-" : "+");

    for (int i = POSIT_SIZE - ss() - 1; i >= POSIT_SIZE - mNbits; i--) {
        printf("%d", (p.mBits >> i) & 1);

        if (i != POSIT_SIZE - mNbits &&
            ((i == POSIT_SIZE - ss() - p.rs()) ||
             (i == POSIT_SIZE - ss() - p.rs() - mEs))) {
            printf(" ");
        }
    }

    printf(" = %lg\n", getDouble());
}
