/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/*
   Concepts and parts of this file have been contributed by Fabio Utzig,
   chvprintf() added by Brent Roman.
 */

/**
 * @file    chprintf.c
 * @brief   Mini printf-like functionality.
 *
 * @addtogroup HAL_CHPRINTF
 * @details Mini printf-like functionality.
 * @{
 */

#include "hal.h"
#include "chprintf.h"
#include "memstreams.h"
#include <math.h>

#define MAX_FILLER 11
#define FLOAT_PRECISION 9

/**
 * @brief Convert @p num in @p radix scheme to a string,
 * the @p divisor determines both the max value that can
 * be printed into the string and the input upper bound
 * 
 * @param p Pointer to string buffer
 * @param num Value to be converted
 * @param radix aka "base"
 * @param divisor Max range
 * @return char* pointer to the character right after the end of the printed number
 */
static char *long_to_string_with_divisor(char *p,
                                         long num,
                                         unsigned radix,
                                         long divisor)
{
    int i;
    char *q;
    long l, ll;

    l = num;
    if (divisor == 0)
    {
        ll = num;
    }
    else
    {
        ll = divisor;
    }

    q = p + MAX_FILLER;
    do
    {
        i = (int)(l % radix);
        i += '0';
        if (i > '9')
            i += 'A' - '0' - 10;
        *--q = i;
        l /= radix;
    } while ((ll /= radix) != 0);

    i = (int)(p + MAX_FILLER - q);
    do
        *p++ = *q++;
    while (--i);

    return p;
}

static char *ch_ltoa(char *p, long num, unsigned radix)
{

    return long_to_string_with_divisor(p, num, radix, 0);
}

#if CHPRINTF_USE_FLOAT

static const long pow10[FLOAT_PRECISION] = {
    10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

/*
static char *ftoa(char *p, double num, unsigned long precision) {
  long l;

  if ((precision == 0) || (precision > FLOAT_PRECISION))
    precision = FLOAT_PRECISION;
  precision = pow10[precision - 1];
  long l;
  l = (long)num;
  p = long_to_string_with_divisor(p, l, 10, 0);
  *p++ = '.';
  l = (long)((num - l) * precision);
  return long_to_string_with_divisor(p, l, 10, precision / 10);
}
**/

/**
 * @brief Convert floating point number to string showing @p precision most significant digits
 * 
 * @param p Pointer to string buffer
 * @param num Value to be converted
 * @param precision Number of most significant digits
 * @return char* 
 */
char *ftoa(char *p, double num, unsigned long precision)
{
    if (isnan(num))
    {
        *p++ = 'N';
        *p++ = 'A';
        *p++ = 'N';
        return p;
    }
    if (isinf(num))
    {
        if (num < 0)
            *p++ = '-';
        *p++ = 'I';
        *p++ = 'N';
        *p++ = 'F';
        return p;
    }
    //find exponent and make num have most significant digit at ones place
    int E = 0; //exponent of MS digit
    if (num != 0)
    {
        while (num >= 10.0)
        {
            num /= 10.0;
            E++;
        }
        while (num < 1.0)
        {
            num *= 10.0;
            E--;
        }
    }

    if ((precision == 0) || (precision > FLOAT_PRECISION))
        precision = FLOAT_PRECISION;

    if (E < (int)precision && E >= 0)
    {
        //number can be presented without exponent nicely
        for (; precision > 0; E--, precision--)
        {
            *p++ = '0' + (int)num;
            if (E == 0 && precision > 1)
                *p++ = '.';
            num -= (int)num;
            num *= 10;
        }
    }
    else
    {
        //show number with exponent
        long l;
        l = (long)num;
        p = long_to_string_with_divisor(p, l, 10, 0);

        if (precision >= 1)
        {

            precision = pow10[precision - 2];
            *p++ = '.';
            l = (long)((num - l) * precision);
            p = long_to_string_with_divisor(p, l, 10, precision / 10);
        }

        *p++ = 'E';
        if (E >= 0)
        {
            *p++ = '+';
            p = long_to_string_with_divisor(p, E, 10, 0);
        }
        else
        {
            *p++ = '-';
            E = -E;
            p = long_to_string_with_divisor(p, E, 10, 0);
        }
    }
    return p;
};

#endif

/**
 * @brief   System formatted output function.
 * @details This function implements a minimal @p vprintf()-like functionality
 *          with output on a @p BaseSequentialStream.
 *          The general parameters format is: %[-][width|*][.precision|*][l|L]p.
 *          The following parameter types (p) are supported:
 *          - <b>x</b> hexadecimal integer.
 *          - <b>X</b> hexadecimal long.
 *          - <b>o</b> octal integer.
 *          - <b>O</b> octal long.
 *          - <b>d</b> decimal signed integer.
 *          - <b>D</b> decimal signed long.
 *          - <b>u</b> decimal unsigned integer.
 *          - <b>U</b> decimal unsigned long.
 *          - <b>c</b> character.
 *          - <b>s</b> string.
 *          .
 *
 * @param[in] chp       pointer to a @p BaseSequentialStream implementing object
 * @param[in] fmt       formatting string
 * @param[in] ap        list of parameters
 * @return              The number of bytes that would have been
 *                      written to @p chp if no stream error occurs
 *
 * @api
 */
int chvprintf(BaseSequentialStream *chp, const char *fmt, va_list ap)
{
    char *p, *s, c, filler;
    int i, precision, width;
    int n = 0;
    bool is_long, left_align;
    long l;
#if CHPRINTF_USE_FLOAT
    char tmpbuf[2 * MAX_FILLER + 1];
#else
    char tmpbuf[MAX_FILLER + 1];
#endif

    while (true)
    {
        c = *fmt++;
        if (c == 0)
            return n;
        if (c != '%')
        {
            streamPut(chp, (uint8_t)c);
            n++;
            continue;
        }
        p = tmpbuf;
        s = tmpbuf;
        left_align = FALSE;
        if (*fmt == '-')
        {
            fmt++;
            left_align = TRUE;
        }
        filler = ' ';
        if (*fmt == '0')
        {
            fmt++;
            filler = '0';
        }
        width = 0;
        while (TRUE)
        {
            c = *fmt++;
            if (c >= '0' && c <= '9')
                c -= '0';
            else if (c == '*')
                c = va_arg(ap, int);
            else
                break;
            width = width * 10 + c;
        }
        precision = 0;
        if (c == '.')
        {
            while (TRUE)
            {
                c = *fmt++;
                if (c >= '0' && c <= '9')
                    c -= '0';
                else if (c == '*')
                    c = va_arg(ap, int);
                else
                    break;
                precision *= 10;
                precision += c;
            }
        }
        /* Long modifier.*/
        if (c == 'l' || c == 'L')
        {
            is_long = TRUE;
            if (*fmt)
                c = *fmt++;
        }
        else
            is_long = (c >= 'A') && (c <= 'Z');

        /* Command decoding.*/
        switch (c)
        {
        case 'c':
            filler = ' ';
            *p++ = va_arg(ap, int);
            break;
        case 's':
            filler = ' ';
            if ((s = va_arg(ap, char *)) == 0)
                s = "(null)";
            if (precision == 0)
                precision = 32767;
            for (p = s; *p && (--precision >= 0); p++)
                ;
            break;
        case 'D':
        case 'd':
        case 'I':
        case 'i':
            if (is_long)
                l = va_arg(ap, long);
            else
                l = va_arg(ap, int);
            if (l < 0)
            {
                *p++ = '-';
                l = -l;
            }
            p = ch_ltoa(p, l, 10);
            break;
#if CHPRINTF_USE_FLOAT
            static volatile double f;
        case 'f':
            f = va_arg(ap, double);
            if (f < 0)
            {
                *p++ = '-';
                f = -f;
            }
            p = ftoa(p, f, precision);
            break;
#endif
        case 'X':
        case 'x':
            c = 16;
            goto unsigned_common;
        case 'U':
        case 'u':
            c = 10;
            goto unsigned_common;
        case 'O':
        case 'o':
            c = 8;
        unsigned_common:
            if (is_long)
                l = va_arg(ap, unsigned long);
            else
                l = va_arg(ap, unsigned int);
            p = ch_ltoa(p, l, c);
            break;
        default:
            *p++ = c;
            break;
        }
        i = (int)(p - s);
        if ((width -= i) < 0)
            width = 0;
        if (left_align == FALSE)
            width = -width;
        if (width < 0)
        {
            if (*s == '-' && filler == '0')
            {
                streamPut(chp, (uint8_t)*s++);
                n++;
                i--;
            }
            do
            {
                streamPut(chp, (uint8_t)filler);
                n++;
            } while (++width != 0);
        }
        while (--i >= 0)
        {
            streamPut(chp, (uint8_t)*s++);
            n++;
        }

        while (width)
        {
            streamPut(chp, (uint8_t)filler);
            n++;
            width--;
        }
    }
}

/**
 * @brief   System formatted output function.
 * @details This function implements a minimal @p printf() like functionality
 *          with output on a @p BaseSequentialStream.
 *          The general parameters format is: %[-][width|*][.precision|*][l|L]p.
 *          The following parameter types (p) are supported:
 *          - <b>x</b> hexadecimal integer.
 *          - <b>X</b> hexadecimal long.
 *          - <b>o</b> octal integer.
 *          - <b>O</b> octal long.
 *          - <b>d</b> decimal signed integer.
 *          - <b>D</b> decimal signed long.
 *          - <b>u</b> decimal unsigned integer.
 *          - <b>U</b> decimal unsigned long.
 *          - <b>c</b> character.
 *          - <b>s</b> string.
 *          .
 *
 * @param[in] chp       pointer to a @p BaseSequentialStream implementing object
 * @param[in] fmt       formatting string
 * @return              The number of bytes that would have been
 *                      written to @p chp if no stream error occurs
 *
 * @api
 */
int chprintf(BaseSequentialStream *chp, const char *fmt, ...)
{
    va_list ap;
    int formatted_bytes;

    va_start(ap, fmt);
    formatted_bytes = chvprintf(chp, fmt, ap);
    va_end(ap);

    return formatted_bytes;
}

/**
 * @brief   System formatted output function.
 * @details This function implements a minimal @p snprintf()-like functionality.
 *          The general parameters format is: %[-][width|*][.precision|*][l|L]p.
 *          The following parameter types (p) are supported:
 *          - <b>x</b> hexadecimal integer.
 *          - <b>X</b> hexadecimal long.
 *          - <b>o</b> octal integer.
 *          - <b>O</b> octal long.
 *          - <b>d</b> decimal signed integer.
 *          - <b>D</b> decimal signed long.
 *          - <b>u</b> decimal unsigned integer.
 *          - <b>U</b> decimal unsigned long.
 *          - <b>c</b> character.
 *          - <b>s</b> string.
 *          .
 * @post    @p str is NUL-terminated, unless @p size is 0.
 *
 * @param[in] str       pointer to a buffer
 * @param[in] size      maximum size of the buffer
 * @param[in] fmt       formatting string
 * @return              The number of characters (excluding the
 *                      terminating NUL byte) that would have been
 *                      stored in @p str if there was room.
 *
 * @api
 */
int chsnprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    int retval;

    /* Performing the print operation.*/
    va_start(ap, fmt);
    retval = chvsnprintf(str, size, fmt, ap);
    va_end(ap);

    /* Return number of bytes that would have been written.*/
    return retval;
}

/**
 * @brief   System formatted output function.
 * @details This function implements a minimal @p vsnprintf()-like functionality.
 *          The general parameters format is: %[-][width|*][.precision|*][l|L]p.
 *          The following parameter types (p) are supported:
 *          - <b>x</b> hexadecimal integer.
 *          - <b>X</b> hexadecimal long.
 *          - <b>o</b> octal integer.
 *          - <b>O</b> octal long.
 *          - <b>d</b> decimal signed integer.
 *          - <b>D</b> decimal signed long.
 *          - <b>u</b> decimal unsigned integer.
 *          - <b>U</b> decimal unsigned long.
 *          - <b>c</b> character.
 *          - <b>s</b> string.
 *          .
 * @post    @p str is NUL-terminated, unless @p size is 0.
 *
 * @param[in] str       pointer to a buffer
 * @param[in] size      maximum size of the buffer
 * @param[in] fmt       formatting string
 * @param[in] ap        list of parameters
 * @return              The number of characters (excluding the
 *                      terminating NUL byte) that would have been
 *                      stored in @p str if there was room.
 *
 * @api
 */
int chvsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    MemoryStream ms;
    BaseSequentialStream *chp;
    size_t size_wo_nul;
    int retval;

    if (size > 0)
        size_wo_nul = size - 1;
    else
        size_wo_nul = 0;

    /* Memory stream object to be used as a string writer, reserving one
     byte for the final zero.*/
    msObjectInit(&ms, (uint8_t *)str, size_wo_nul, 0);

    /* Performing the print operation using the common code.*/
    chp = (BaseSequentialStream *)(void *)&ms;
    retval = chvprintf(chp, fmt, ap);

    /* Terminate with a zero, unless size==0.*/
    if (ms.eos < size)
    {
        str[ms.eos] = 0;
    }

    /* Return number of bytes that would have been written.*/
    return retval;
}

/** @} */
