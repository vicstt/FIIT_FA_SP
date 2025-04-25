#include "../include/big_int.h"
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>

void normalize(std::vector<unsigned int, pp_allocator<unsigned int>> &digits)
{
    while (!digits.empty() && digits.back() == 0) {
        digits.pop_back();
    }
    if (digits.empty()) {
        digits.push_back(0);
    }
}

std::strong_ordering big_int::operator<=>(const big_int &other) const noexcept
{
    if (_digits.empty() && other._digits.empty())
        return std::strong_ordering::equal;

    if (_sign != other._sign)
        return _sign ? std::strong_ordering::greater : std::strong_ordering::less;

    bool both_sign = _sign;
    auto& lhs = _digits;
    auto& rhs = other._digits;

    std::strong_ordering res;

    if (lhs.size() != rhs.size()) {
        res = lhs.size() <=> rhs.size();
    } else {
        res = std::strong_ordering::equal;
        for (auto lit = lhs.rbegin(), rit = rhs.rbegin(); lit != lhs.rend(); ++lit, ++rit) {
            if (*lit != *rit) {
                res = *lit <=> *rit;
                break;
            }
        }
    }

    if (both_sign) {
        return res;
    } else {
        return (res == std::strong_ordering::equal) ? res :
               (res == std::strong_ordering::less) ? std::strong_ordering::greater : std::strong_ordering::less;
    }
}

big_int::operator bool() const noexcept
{
    return !(_digits.size() == 1 && _digits[0] == 0);
}

big_int &big_int::operator++() &
{
    return *this += 1;
}


big_int big_int::operator++(int)
{
    big_int tmp = *this;
    ++(*this);
    return tmp;
}

big_int &big_int::operator--() &
{
    return *this -= 1;

}

big_int big_int::operator--(int)
{
    big_int tmp = *this;
    --(*this);
    return tmp;
}

big_int &big_int::operator+=(const big_int &other) &
{
    return plus_assign(other, 0);
}

big_int &big_int::operator-=(const big_int &other) &
{
    return minus_assign(other, 0);
}

big_int big_int::operator+(const big_int &other) const
{
    big_int tmp = *this;
    return tmp += other;
}

big_int big_int::operator-(const big_int &other) const
{
    big_int tmp = *this;
    return tmp -= other;
}

big_int big_int::operator*(const big_int &other) const
{
    big_int tmp = *this;
    return tmp *= other;
}

big_int big_int::operator/(const big_int &other) const {
    big_int tmp = *this;
    return tmp /= other;
}

big_int big_int::operator%(const big_int &other) const
{
    big_int tmp = *this;
    return tmp %= other;
}

big_int big_int::operator&(const big_int &other) const
{
    big_int tmp = *this;
    return tmp &= other;
}

big_int big_int::operator|(const big_int &other) const
{
    big_int tmp = *this;
    return tmp |= other;
}

big_int big_int::operator^(const big_int &other) const
{
    big_int tmp = *this;
    return tmp ^= other;
}

big_int big_int::operator<<(size_t shift) const
{
    big_int tmp = *this;
    return tmp <<= shift;
}

big_int big_int::operator>>(size_t shift) const
{
    big_int tmp = *this;
    return tmp >>= shift;
}

big_int &big_int::operator%=(const big_int &other) &
{
    return modulo_assign(other, decide_div(other._digits.size()));
}

big_int big_int::operator~() const
{
    big_int result(*this);
    for (auto &digit: result._digits) {
        digit = ~digit;
    }

    normalize(result._digits);
    return result;
}

big_int &big_int::operator&=(const big_int &other) &
{
    const size_t min_size = std::min(_digits.size(), other._digits.size());

    for (size_t i = 0; i < min_size; ++i) {
        _digits[i] &= other._digits[i];
    }

    if (_digits.size() > other._digits.size()) {
        for (size_t i = min_size; i < _digits.size(); ++i) {
            _digits[i] = 0;
        }
    }

    normalize(_digits);
    return *this;
}

big_int &big_int::operator|=(const big_int &other) &
{
    if (other._digits.size() > _digits.size()) {
        _digits.resize(other._digits.size(), 0);
    }

    for (size_t i = 0; i < other._digits.size(); ++i) {
        _digits[i] |= other._digits[i];
    }

    normalize(_digits);
    return *this;
}

big_int &big_int::operator^=(const big_int &other) &
{
    if (other._digits.size() > _digits.size()) {
        _digits.resize(other._digits.size(), 0);
    }

    for (size_t i = 0; i < other._digits.size(); ++i) {
        _digits[i] ^= other._digits[i];
    }

    normalize(_digits);
    return *this;
}

big_int &big_int::operator<<=(size_t shift) &
{
    if (!*this || shift == 0) {
        return *this;
    }

    size_t digit_shift = shift / (sizeof(unsigned int) * 8);
    size_t bit_shift = shift % (sizeof(unsigned int) * 8);

    _digits.insert(_digits.begin(), digit_shift, 0);

    if (bit_shift > 0) {
        unsigned int carry = 0;
        for (auto it = _digits.begin() + digit_shift; it != _digits.end(); ++it) {
            unsigned int new_carry = *it >> (32 - bit_shift);
            *it = (*it << bit_shift) | carry;
            carry = new_carry;
        }
        if (carry != 0) {
            _digits.push_back(carry);
        }
    }

    normalize(_digits);
    return *this;
}

big_int &big_int::operator>>=(size_t shift) &
{
    if (!*this || shift == 0) {
        return *this;
    }

    size_t digit_shift = shift / (sizeof(uint32_t) * 8);
    size_t bit_shift = shift % (sizeof(uint32_t) * 8);

    if (digit_shift >= _digits.size()) {
        _digits.clear();
        return *this;
    }
    _digits.erase(_digits.begin(), _digits.begin() + digit_shift);

    if (bit_shift > 0) {
        unsigned int carry = 0;
        for (auto it = _digits.rbegin(); it != _digits.rend(); ++it) {
            unsigned int new_carry = *it << (32 - bit_shift);
            *it = (*it >> bit_shift) | carry;
            carry = new_carry;
        }
    }
    normalize(_digits);
    return *this;
}

big_int &big_int::plus_assign(const big_int &other, size_t shift) &
{
    if (_digits.size() < other._digits.size() + shift) {
        _digits.resize(other._digits.size() + shift, 0);
    }

    unsigned int carry = 0;
    for (size_t i = 0; i < other._digits.size() || carry; ++i) {
        uint32_t other_digit = (i < other._digits.size()) ? other._digits[i] : 0;
        uint32_t& target = _digits[i + shift];

        uint64_t sum = (uint64_t)target + other_digit + carry;
        target = (uint32_t)sum;
        carry = sum >> 32;
    }

    normalize(_digits);
    return *this;
}

big_int &big_int::minus_assign(const big_int &other, size_t shift) &
{
    if (!other) {
        return *this;
    }

    if (_sign != other._sign) {
        big_int temp(other);
        temp._sign = _sign;
        return plus_assign(temp, shift);
    }

    big_int abs_this(*this);
    abs_this._sign = true;
    big_int abs_other(other);
    abs_other._sign = true;

    abs_other <<= shift;

    bool result_sign = _sign;
    if ((abs_this <=> abs_other) == std::strong_ordering::less) {
        result_sign = !result_sign;
        std::swap(abs_this, abs_other);
    }

    size_t max_size = abs_this._digits.size();
    abs_other._digits.resize(max_size, 0);

    unsigned int borrow = 0;
    for (size_t i = 0; i < max_size; ++i) {
        uint64_t diff = static_cast<uint64_t>(abs_this._digits[i]) - abs_other._digits[i] - borrow;
        abs_this._digits[i] = static_cast<uint32_t>(diff);
        borrow = (diff >> 63) ? 1 : 0;
    }

    _digits = std::move(abs_this._digits);
    _sign = result_sign;
    normalize(_digits);

    if (_digits.empty()) {
        _sign = true;
    }

    return *this;
}

big_int &big_int::operator*=(const big_int &other) &
{
    return multiply_assign(other, decide_mult(other._digits.size()));
}

big_int &big_int::operator/=(const big_int &other) &
{
    return divide_assign(other, decide_div(other._digits.size()));
}

std::string big_int::to_string() const
{
    if (!*this) {
        return "0";
    }

    std::string result;
    big_int num = *this;
    num._sign = true;

    while (num > 0_bi) {
        big_int digit = num % 10;
        result += '0' + digit._digits[0];
        num /= 10;
    }

    if (!_sign && result != "0") {
        result += '-';
    }

    std::reverse(result.begin(), result.end());

    return result;
}

std::ostream &operator<<(std::ostream &stream, const big_int &value)
{
    stream << value.to_string();
    return stream;
}

std::istream &operator>>(std::istream &stream, big_int &value)
{
    std::string val;
    stream >> val;
    value = big_int(val);
    return stream;
}

bool big_int::operator==(const big_int &other) const noexcept
{
    if (_sign != other._sign) {
        return false;
    }

    if (_digits.size() != other._digits.size()) {
        return false;
    }

    for (size_t i = 0; i < _digits.size(); ++i) {
        if (_digits[i] != other._digits[i]) {
            return false;
        }
    }

    return true;
}

big_int::big_int(const std::vector<unsigned int, pp_allocator<unsigned int>> &digits, bool sign)
        : _digits(digits), _sign(sign) {
    normalize(_digits);

    if (_digits.empty() || (_digits.size() == 1 && _digits[0] == 0)) {
        _sign = true;
    }
}

big_int::big_int(std::vector<unsigned int, pp_allocator<unsigned int>> &&digits, bool sign) noexcept
        : _digits(std::move(digits)), _sign(sign) {
    normalize(_digits);

    if (_digits.empty() || (_digits.size() == 1 && _digits[0] == 0)) {
        _sign = true;
    }
}

big_int::big_int(const std::string &num, unsigned int radix, pp_allocator<unsigned int> alloc)
        : _digits(alloc), _sign(true)  {
    if (radix < 2 || radix > 36) {
        throw std::invalid_argument("Radix must be between 2 and 36");
    }

    size_t start = 0;
    if (!num.empty()) {
        if (num[0] == '-') {
            _sign = false;
            start = 1;
        }
        else if (num[0] == '+') {
            start = 1;
        }
    }

    if (start >= num.size()) {
        throw std::invalid_argument("Invalid number string");
    }

    for (size_t i = start; i < num.size(); ++i) {
        char c = num[i];
        unsigned int digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        }
        else if (c >= 'A' && c <= 'Z') {
            digit = 10 + (c - 'A');
        }
        else if (c >= 'a' && c <= 'z') {
            digit = 10 + (c - 'a');
        }
        else {
            throw std::invalid_argument("Invalid character in number string");
        }

        if (digit >= radix) {
            throw std::invalid_argument("Digit exceeds radix");
        }

        *this *= radix;
        *this += digit;
    }

    normalize(_digits);

    if (_digits.empty()) {
        _digits.push_back(0);
        _sign = true;
    }
}

big_int::big_int(pp_allocator<unsigned int> alloc)
        : _digits(alloc), _sign(true)
{
    _digits.push_back(0);
}

big_int &big_int::multiply_assign(const big_int &other, big_int::multiplication_rule rule) &
{
    if (!*this) {
        return *this;
    }

    if (!other) {
        _digits.clear();
        _digits.push_back(0);
        _sign = true;
        return *this;
    }

    if (rule == big_int::multiplication_rule::Karatsuba) {
        big_int result = karatsuba_multiply(*this, other);
        _digits = std::move(result._digits);
        _sign = (_sign == other._sign);
        normalize(_digits);
        return *this;
    }

    if (rule == big_int::multiplication_rule::trivial) {
        trivial_multiply(other);
        _sign = (_sign == other._sign);
        return *this;
    }

    return *this;
}

big_int &big_int::divide_assign(const big_int &other, big_int::division_rule rule) &
{
    if (!*this) return *this;
    if (!other) throw std::logic_error("Division by zero");

    bool result_sign = (_sign == other._sign);

    if (other._digits.size() == 1) {
        uint32_t divisor = other._digits[0];
        if (divisor == 0) throw std::runtime_error("Division by zero");

        uint64_t remainder = 0;
        for (auto it = _digits.rbegin(); it != _digits.rend(); ++it) {
            uint64_t value = (remainder << 32) + *it;
            *it = static_cast<uint32_t>(value / divisor);
            remainder = value % divisor;
        }
        _sign = result_sign;
        normalize(_digits);
        return *this;
    }

    big_int dividend(*this);
    dividend._sign = true;
    big_int divisor(other);
    divisor._sign = true;

    if (dividend < divisor) {
        _digits = {0};
        _sign = true;
        return *this;
    }

    big_int quotient;
    quotient._digits.resize(dividend._digits.size(), 0);
    quotient._sign = true;

    big_int remainder;
    remainder._sign = true;

    for (int i = static_cast<int>(dividend._digits.size()) - 1; i >= 0; --i) {
        remainder <<= 32;
        remainder += dividend._digits[i];

        if (remainder >= divisor) {
            uint32_t l = 0;
            uint32_t r = UINT32_MAX;
            uint32_t digit = 0;

            while (l <= r) {
                uint32_t mid = l + (r - l) / 2;
                big_int product = divisor;
                product *= mid;

                if (product <= remainder) {
                    digit = mid;
                    l = mid + 1;
                } else {
                    r = mid - 1;
                }
            }

            quotient._digits[i] = digit;
            big_int temp = divisor;
            temp *= digit;
            remainder -= temp;
        }
    }

    quotient._sign = result_sign;
    normalize(quotient._digits);
    *this = std::move(quotient);
    return *this;
}

big_int &big_int::modulo_assign(const big_int &other, big_int::division_rule rule) &
{
    if (!*this) return *this;
    if (!other) throw std::logic_error("Division by zero");

    big_int quotient = *this;
    quotient.divide_assign(other, rule);
    quotient *= other;
    *this -= quotient;
    return *this;
}

big_int::multiplication_rule big_int::decide_mult(size_t rhs) const noexcept {
    return rhs > 1024 ? big_int::multiplication_rule::Karatsuba : big_int::multiplication_rule::trivial;
}

big_int::division_rule big_int::decide_div(size_t rhs) const noexcept {
    return big_int::division_rule::trivial;
}

big_int operator""_bi(unsigned long long n)
{
    return n;
}

void big_int::trivial_multiply(const big_int& other) {
    std::vector<unsigned int, pp_allocator<unsigned int>>result(_digits.size() + other._digits.size(),
                0, _digits.get_allocator());

    uint64_t BASE = 1ULL << 32;

    for (size_t i = 0; i < _digits.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < other._digits.size() || carry; ++j) {
            uint64_t other_digit = (j < other._digits.size()) ? other._digits[j] : 0;
            uint64_t product = static_cast<uint64_t>(_digits[i]) * other_digit + result[i + j] + carry;
            result[i + j] = static_cast<uint32_t>(product % BASE);
            carry = product / BASE;
        }
    }

    _digits = std::move(result);
    normalize(_digits);
}

void big_int::split_at(size_t half, big_int& high, big_int& low) const {
    if (half >= _digits.size()) {
        low = *this;
        high._digits = {0};
        high._sign = true;
        return;
    }

    low._digits.assign(_digits.begin(), _digits.begin() + half);
    high._digits.assign(_digits.begin() + half, _digits.end());
    low._sign = high._sign = _sign;
}

big_int big_int::karatsuba_multiply(const big_int& a, const big_int& b) {
    if (a._digits.size() < 2 || b._digits.size() < 2) {
        big_int result(a);
        result.trivial_multiply(b);
        return result;
    }

    size_t half = std::max(a._digits.size(), b._digits.size()) / 2;

    big_int a_high, a_low;
    a.split_at(half, a_high, a_low);

    big_int b_high, b_low;
    b.split_at(half, b_high, b_low);

    big_int z0 = karatsuba_multiply(a_high, b_high);
    big_int z1 = karatsuba_multiply(a_low, b_low);
    big_int z2 = karatsuba_multiply(a_low + a_high, b_low + b_high) - z0 - z1;

    z0 <<= (half * 2 * 32);
    z2 <<= (half * 32);

    return z0 + z2 + z1;
}
