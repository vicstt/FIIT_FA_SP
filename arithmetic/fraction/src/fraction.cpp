#include "../include/fraction.h"
#include <cmath>
#include <numeric>
#include <sstream>
#include <regex>

void fraction::optimise()
{
    if (_denominator == 0_bi) {
        throw std::invalid_argument("Denominator cannot be zero");
    }

    big_int a = _numerator;
    big_int b = _denominator;

    if (a < 0_bi) {
        a = a * big_int("-1");
    }

    if (b < 0_bi) {
        b = b * big_int("-1");
    }

    while (b != 0_bi) {
        big_int temp = b;
        b = a % b;
        a = temp;
    }
    big_int divisor = a;

    _numerator /= divisor;
    _denominator /= divisor;

    if (_numerator < 0_bi) {
        _numerator = _numerator * big_int("-1");
        _denominator = _denominator * big_int("-1");
    }
}

template<std::convertible_to<big_int> f, std::convertible_to<big_int> s>
fraction::fraction(f &&numerator, s &&denominator) : _numerator(std::forward<f>(numerator)),
                                                    _denominator(std::forward<s>(denominator)) {
    if (_denominator == 0_bi) {
        throw std::invalid_argument("Denominator cannot be zero");
    }
    optimise();
}

fraction::fraction(pp_allocator<big_int::value_type>)
        : _numerator(0_bi), _denominator(1_bi) {}

fraction &fraction::operator+=(fraction const &other) &
{
    _numerator = _numerator * other._denominator + _denominator * other._numerator;
    _denominator = _denominator * other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator+(fraction const &other) const
{
    fraction result = *this;
    result += other;
    return result;
}

fraction &fraction::operator-=(fraction const &other) &
{
    _numerator = _numerator * other._denominator - _denominator * other._numerator;
    _denominator = _denominator * other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator-(fraction const &other) const
{
    fraction result = *this;
    result -= other;
    return result;
}

fraction fraction::operator-() const
{
    fraction result(*this);
    result._numerator = result._numerator * big_int("-1");
    result.optimise();
    return result;
}

fraction &fraction::operator*=(fraction const &other) &
{
    _numerator *= other._numerator;
    _denominator *= other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator*(fraction const &other) const
{
    fraction result = *this;
    result *= other;
    return result;
}

fraction &fraction::operator/=(fraction const &other) &
{
    if (other._numerator == 0_bi) {
        throw std::invalid_argument("Division by zero");
    }
    _numerator *= other._denominator;
    _denominator *= other._numerator;
    optimise();
    return *this;
}

fraction fraction::operator/(fraction const &other) const
{
    fraction result = *this;
    result /= other;
    return result;
}

bool fraction::operator==(fraction const &other) const noexcept
{
    return _numerator == other._numerator && _denominator == other._denominator;
}

std::partial_ordering fraction::operator<=>(const fraction& other) const noexcept
{
    big_int lhs = _numerator * other._denominator;
    big_int rhs = _denominator * other._numerator;
    if (lhs < rhs) return std::partial_ordering::less;
    if (lhs > rhs) return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
}

std::ostream &operator<<(std::ostream &stream, fraction const &obj)
{
    stream << obj._numerator << "/" << obj._denominator;
    return stream;
}

std::istream &operator>>(std::istream &stream, fraction &obj)
{
    std::string input;
    if (!(stream >> input)) {
        return stream;
    }

    size_t slash_pos = input.find('/');

    std::string num_str = (slash_pos == std::string::npos) ? input : input.substr(0, slash_pos);
    std::string denom_str = (slash_pos == std::string::npos) ? "1" : input.substr(slash_pos + 1);

    if (num_str.empty() || denom_str.empty()) {
        stream.setstate(std::ios::failbit);
        return stream;
    }

    big_int numerator(num_str);
    big_int denominator(denom_str);

    if (denominator == 0_bi) {
        stream.setstate(std::ios::failbit);
        return stream;
    }

    obj = fraction(numerator, denominator);
    return stream;
}

std::string fraction::to_string() const
{
    std::stringstream ss;
    ss << _numerator << "/" << _denominator;
    return ss.str();
}

fraction fraction::sin(fraction const &epsilon) const
{
    fraction result(0_bi, 1_bi);
    fraction temp = *this;
    fraction x_squared = *this * *this;

    big_int factorial = 1_bi;
    int n = 1;
    while (temp > epsilon || temp < -epsilon) {
        result += temp;
        n += 2;
        factorial *= n * (n - 1);
        temp *= x_squared;
        temp /= fraction(factorial, 1_bi);
        temp = -temp;
    }
    return result;
}

fraction fraction::cos(fraction const &epsilon) const
{
    fraction x_squared = *this * *this;
    fraction result(0_bi, 1_bi);
    fraction temp(1_bi, 1_bi);
    big_int factorial = 1_bi;
    int n = 0;
    while (temp > epsilon || temp < -epsilon) {
        result += temp;
        n += 2;
        factorial *= n * (n - 1);
        temp *= x_squared;
        temp /= fraction(factorial, 1_bi);
        temp = -temp;
    }
    return result;
}

fraction fraction::tg(fraction const &epsilon) const
{
    fraction sin_val = this->sin(epsilon);
    fraction cos_val = this->cos(epsilon);

    if (cos_val == fraction(0_bi, 1_bi)) {
        throw std::runtime_error("Tangent is undefined (cos(x) = 0)");
    }

    return sin_val / cos_val;
}

fraction fraction::ctg(fraction const &epsilon) const
{
    fraction sin_val = this->sin(epsilon);
    fraction cos_val = this->cos(epsilon);

    if (sin_val == fraction(0_bi, 1_bi)) {
        throw std::runtime_error("Cotangent is undefined (sin(x) = 0)");
    }

    return cos_val / sin_val;
}

fraction fraction::sec(fraction const &epsilon) const
{
    fraction cos_val = this->cos(epsilon);

    if (cos_val == fraction(0_bi, 1_bi)) {
        throw std::runtime_error("Secant is undefined (cos(x) = 0)");
    }
    return fraction(1, 1) / cos_val;
}

fraction fraction::cosec(fraction const &epsilon) const
{
    fraction sin_val = this->sin(epsilon);

    if (sin_val == fraction(0_bi, 1_bi)) {
        throw std::runtime_error("Cosecant is undefined (sin(x) = 0)");
    }
    return fraction(1, 1) / sin_val;
}

fraction fraction::arcsin(const fraction& epsilon) const {
    if (*this > fraction(1_bi, 1_bi) || *this < fraction(big_int("-1"), 1_bi)) {
        throw std::domain_error("arcsin(x) is defined only for x in [-1, 1]");
    }

    fraction result = *this;
    fraction temp = *this;
    fraction x_squared = *this * *this;

    int n = 1;         // Счётчик для числителя коэффициента
    int k = 0;         // Счётчик для знаменателя коэффициента
    int m = 1;         // Числитель коэффициента
    int p = 1;         // Знаменатель коэффициента

    while (temp > epsilon || temp < -epsilon) {
        n += 2;
        k += 2;
        m *= (n - 2);
        p *= k;

        temp *= x_squared;
        temp = temp * fraction(m, p * n);

        result += temp;
    }

    return result;
}

fraction fraction::arccos(const fraction& epsilon) const {
    if (*this > fraction(1_bi, 1_bi) || *this < fraction(big_int("-1"), 1_bi)) {
        throw std::domain_error("arccos(x) is defined only for x in [-1, 1]");
    }

    fraction pi(355_bi, 113_bi);
    fraction pi_half = pi / fraction(2_bi, 1_bi);

    return pi_half - this->arcsin(epsilon);
}

fraction fraction::arctg(const fraction& epsilon) const {
    if (*this == fraction(0_bi, 1_bi)) {
        return fraction(0_bi, 1_bi);
    }

    fraction x_squared = *this * *this;
    fraction denominator = (fraction(1_bi, 1_bi) + x_squared).root(2, epsilon);
    fraction sin_arg = *this / denominator;

    return sin_arg.arcsin(epsilon);
}

fraction fraction::arcctg(fraction const &epsilon) const
{
    fraction pi(355_bi, 113_bi);
    fraction pi_half = pi / fraction(2_bi, 1_bi);
    return pi_half - this->arctg(epsilon);
}

fraction fraction::arcsec(fraction const &epsilon) const
{
    if (*this == fraction(0_bi, 1_bi)) {
        throw std::domain_error("arcsec(0) is undefined");
    }
    fraction temp = fraction(1_bi, 1_bi) / *this;
    return temp.arccos(epsilon);
}

fraction fraction::arccosec(fraction const &epsilon) const
{
    if (*this == fraction(0_bi, 1_bi)) {
        throw std::domain_error("arccosec(0) is undefined");
    }
    fraction temp = fraction(1_bi, 1_bi) / *this;
    return temp.arcsin(epsilon);
}

fraction fraction::pow(size_t degree) const
{
    if (degree == 0) return fraction(1_bi, 1_bi);
    if (degree == 1) return *this;
    if (degree % 2 == 0) {
        fraction temp = *this;
        temp = temp.pow(degree / 2);
        temp *= temp;
        return temp;
    }
    else {
        fraction temp = *this;
        temp = temp.pow(degree-1);
        temp *= (*this);
        return temp;
    }
}

fraction fraction::root(size_t degree, fraction const &epsilon) const
{
    if (degree == 0) {
        throw std::invalid_argument("Degree cannot be zero");
    }
    if (degree == 1) {
        return *this;
    }
    if (_numerator < 0_bi && degree % 2 == 0) {
        throw std::domain_error("Even root of negative number is not real");
    }

    bool is_negative = (_numerator < 0_bi) && (degree % 2 == 1);
    fraction x = is_negative ? -(*this) : *this;

    fraction guess = x / fraction(degree, 1);
    fraction prev_guess;
    fraction delta;

    do {
        prev_guess = guess;
        fraction power = guess.pow(degree - 1);

        if (power == fraction(0_bi, 1_bi)) {
            break;
        }

        guess = (prev_guess * fraction(degree - 1, 1) + x / power) /
                fraction(degree, 1);
        delta = guess > prev_guess ? guess - prev_guess : prev_guess - guess;

    } while (delta >= epsilon);

    return is_negative ? -guess : guess;
}

fraction fraction::log2(fraction const &epsilon) const
{
    if (_numerator <= 0_bi || _denominator <= 0_bi) {
        throw std::domain_error("Logarithm of non-positive number is undefined");
    }
    fraction ln2 = fraction(2_bi, 1_bi).ln(epsilon);
    return this->ln(epsilon) / ln2;
}

fraction fraction::ln(fraction const &epsilon) const
{
    if (_numerator <= 0_bi || _denominator <= 0_bi) {
        throw std::domain_error("Natural logarithm of non-positive number is undefined");
    }
    fraction x = *this;
    if (x > fraction(2_bi, 1_bi)) {
        return fraction(1_bi, 1_bi).ln(epsilon) + (x / fraction(2_bi, 1_bi)).ln(epsilon);
    }
    if (x < fraction(1_bi, 2_bi)) {
        return -(fraction(1_bi, 1_bi) / x).ln(epsilon);
    }
    fraction y = x - fraction(1_bi, 1_bi);
    fraction result(0_bi, 1_bi);
    fraction temp = y;
    int n = 1;
    while (temp > epsilon || temp < -epsilon) {
        result += fraction((n % 2 == 0 ? big_int("-1") : 1_bi), n) * temp;
        n++;
        temp *= y;
    }
    return result;
}

fraction fraction::lg(fraction const &epsilon) const
{
    if (_numerator <= 0_bi || _denominator <= 0_bi) {
        throw std::domain_error("Base-10 logarithm of non-positive number is undefined");
    }
    fraction ln10 = fraction(10_bi, 1_bi).ln(epsilon);
    return this->ln(epsilon) / ln10;
}