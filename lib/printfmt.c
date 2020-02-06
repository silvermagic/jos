// Stripped-down primitive printf-style formatting routines,
// used in common by printf, sprintf, fprintf, etc.
// This code is also used by both the kernel and user programs.

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/stdarg.h>
#include <inc/error.h>

/*
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 *
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * The integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 */

static const char * const error_string[MAXERROR] =
{
	[E_UNSPECIFIED]	= "unspecified error",
	[E_BAD_ENV]	= "bad environment",
	[E_INVAL]	= "invalid parameter",
	[E_NO_MEM]	= "out of memory",
	[E_NO_FREE_ENV]	= "out of environments",
	[E_FAULT]	= "segmentation fault",
};

static int style = 0x0;

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static void
printnum(void (*putch)(int, void*), void *putdat,
	 unsigned long long num, unsigned base, int width, int padc)
{
	// first recursively print all preceding (more significant) digits
	if (num >= base) {
		printnum(putch, putdat, num / base, base, width - 1, padc);
	} else {
		// print any needed pad characters before first digit
		while (--width > 0)
			putch(padc | style, putdat);
	}

	// then print this (the least significant) digit
	putch("0123456789abcdef"[num % base] | style, putdat);
}

// Get an unsigned int of various possible sizes from a varargs list,
// depending on the lflag parameter.
static unsigned long long
getuint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, unsigned long long);
	else if (lflag)
		return va_arg(*ap, unsigned long);
	else
		return va_arg(*ap, unsigned int);
}

// Same as getuint but signed - can't use getuint
// because of sign extension
static long long
getint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, long long);
	else if (lflag)
		return va_arg(*ap, long);
	else
		return va_arg(*ap, int);
}


// Main function to format and print a string.
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);

void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
	register const char *p;
	register int ch, err;
	unsigned long long num;
	int base, lflag, width, precision, altflag;
	char padc;

	while (1) {
		while ((ch = *(unsigned char *) fmt++) != '%') {
			if (ch == '\0')
				return;
			// 文本样式控制
			if (ch == '\e') {
				ch = *(unsigned char *) fmt++;
				if (ch == '[') {
					ch = *(unsigned char *) fmt++;
					// 颜色编码解析
					while ('0' <= ch && ch <= '9') {
						int color = 0;
						while (1) {
							color = color * 10 + ch - '0';
							ch = *fmt++;
							if (ch < '0' || ch > '9')
								break;
						}

						switch (color) {
							case 5: // 闪烁
								style ^= 0x8000;
								break;
								// 前景
							case 30: // 黑色
								style &= 0xF0FF;
								style |= 0x0000;
								break;
							case 31: // 红色
								style &= 0xF0FF;
								style |= 0x0400;
								break;
							case 32: // 绿色
								style &= 0xF0FF;
								style |= 0x0200;
								break;
							case 33: // 黄色
								style &= 0xF0FF;
								style |= 0x0600;
								break;
							case 34: // 蓝色
								style &= 0xF0FF;
								style |= 0x0100;
								break;
							case 35: // 品红色
								style &= 0xF0FF;
								style |= 0x0500;
								break;
							case 36: // 青色
								style &= 0xF0FF;
								style |= 0x0300;
							case 37: // 白色（灰）
								style &= 0xF0FF;
								style |= 0x0700;
								break;
								// 背景
							case 40: // 黑色
								style &= 0x7FFF;
								style |= 0x0000;
								break;
							case 41: // 红色
								style &= 0x7FFF;
								style |= 0x4000;
								break;
							case 42: // 绿色
								style &= 0x7FFF;
								style |= 0x2000;
								break;
							case 43: // 黄色
								style &= 0x7FFF;
								style |= 0x6000;
								break;
							case 44: // 蓝色
								style &= 0x7FFF;
								style |= 0x1000;
								break;
							case 45: // 品红色
								style &= 0x7FFF;
								style |= 0x5000;
								break;
							case 46: // 青色
								style &= 0x7FFF;
								style |= 0x3000;
								break;
							case 47: // 白色（灰）
								style &= 0x7FFF;
								style |= 0x7000;
								break;
								// 亮色前景
							case 90: // 亮黑色（灰）
								style &= 0xF0FF;
								style |= 0x0800;
								break;
							case 91: // 亮红色
								style &= 0xF0FF;
								style |= 0x0c00;
								break;
							case 92: // 亮绿色
								style &= 0xF0FF;
								style |= 0x0a00;
								break;
							case 93: // 亮黄色
								style &= 0xF0FF;
								style |= 0x0e00;
								break;
							case 94: // 亮蓝色
								style &= 0xF0FF;
								style |= 0x0900;
								break;
							case 95: // 亮品红色
								style &= 0xF0FF;
								style |= 0x0d00;
								break;
							case 96: // 亮青色
								style &= 0xF0FF;
								style |= 0x0b00;
							case 97: // 亮白色
								style &= 0xF0FF;
								style |= 0x0f00;
								break;
							default: // 重置
								style = 0x0;
								break;
						}

						if (ch == ';')
							ch = *(unsigned char *) fmt++;
					}
					// 文本样式控制结束符
					if (ch == 'm')
						continue;
					else if (ch == '%') // 如果不是结束符，默认自动结束，但是需要打印当前字符
						break;
				}
			}
			putch(ch | style, putdat);
		}

		// Process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;
	reswitch:
		switch (ch = *(unsigned char *) fmt++) {

		// flag to pad on the right
		case '-':
			padc = '-';
			goto reswitch;

		// flag to pad with 0's instead of spaces
		case '0':
			padc = '0';
			goto reswitch;

		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (precision = 0; ; ++fmt) {
				precision = precision * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto process_precision;

		case '*':
			precision = va_arg(ap, int);
			goto process_precision;

		case '.':
			if (width < 0)
				width = 0;
			goto reswitch;

		case '#':
			altflag = 1;
			goto reswitch;

		process_precision:
			if (width < 0)
				width = precision, precision = -1;
			goto reswitch;

		// long flag (doubled for long long)
		case 'l':
			lflag++;
			goto reswitch;

		// character
		case 'c':
			putch(va_arg(ap, int) | style, putdat);
			break;

		// error message
		case 'e':
			err = va_arg(ap, int);
			if (err < 0)
				err = -err;
			if (err >= MAXERROR || (p = error_string[err]) == NULL)
				printfmt(putch, putdat, "error %d", err);
			else
				printfmt(putch, putdat, "%s", p);
			break;

		// string
		case 's':
			if ((p = va_arg(ap, char *)) == NULL)
				p = "(null)";
			if (width > 0 && padc != '-')
				for (width -= strnlen(p, precision); width > 0; width--)
					putch(padc | style, putdat);
			for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
				if (altflag && (ch < ' ' || ch > '~'))
					putch('?' | style, putdat);
				else
					putch(ch, putdat);
			for (; width > 0; width--)
				putch(' ' | style, putdat);
			break;

		// (signed) decimal
		case 'd':
			num = getint(&ap, lflag);
			if ((long long) num < 0) {
				putch('-' | style, putdat);
				num = -(long long) num;
			}
			base = 10;
			goto number;

		// unsigned decimal
		case 'u':
			num = getuint(&ap, lflag);
			base = 10;
			goto number;

		// (unsigned) octal
		case 'o':
			// Replace this with your code.
			num = getuint(&ap, lflag);
			base = 8;
			goto number;

		// pointer
		case 'p':
			putch('0', putdat);
			putch('x', putdat);
			num = (unsigned long long)
				(uintptr_t) va_arg(ap, void *);
			base = 16;
			goto number;

		// (unsigned) hexadecimal
		case 'x':
			num = getuint(&ap, lflag);
			base = 16;
		number:
			printnum(putch, putdat, num, base, width, padc);
			break;

		// escaped '%' character
		case '%':
			putch(ch | style, putdat);
			break;

		// unrecognized escape sequence - just print it literally
		default:
			putch('%' | style, putdat);
			for (fmt--; fmt[-1] != '%'; fmt--)
				/* do nothing */;
			break;
		}
	}
}

void
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintfmt(putch, putdat, fmt, ap);
	va_end(ap);
}

struct sprintbuf {
	char *buf;
	char *ebuf;
	int cnt;
};

static void
sprintputch(int ch, struct sprintbuf *b)
{
	b->cnt++;
	if (b->buf < b->ebuf)
		*b->buf++ = ch;
}

int
vsnprintf(char *buf, int n, const char *fmt, va_list ap)
{
	struct sprintbuf b = {buf, buf+n-1, 0};

	if (buf == NULL || n < 1)
		return -E_INVAL;

	// print the string to the buffer
	vprintfmt((void*)sprintputch, &b, fmt, ap);

	// null terminate the buffer
	*b.buf = '\0';

	return b.cnt;
}

int
snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return rc;
}


