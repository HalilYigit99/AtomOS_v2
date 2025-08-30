// Number to string conversion utilities for bare-metal environment
// No dynamic allocation: uses caller buffer or internal static buffer (NOT thread-safe)

#include <util/convert.h>

// Dahili statik bufferlar. Her çağrıda üzerine yazılır.
static char s_int_buffer[66];      // 64 bit + sign + null (her base için yeterli)
static char s_double_buffer[128];  // double dönüşümü için

// Yardımcı: verilen base aralığını doğrula 2..36
static int validate_base(int base) { return (base >= 2 && base <= 36); }

// Yardımcı: işaretsiz sayıyı belirtilen tabana çevirir (reverse yazıp sonra çevirir)
// 64-bit (unsigned) / 32-bit base bölme: (quotient, remainder) döner.
static unsigned long long udivmod_u64(unsigned long long value, unsigned int base, unsigned int* rem_out) {
	unsigned long long q = 0ULL;
	unsigned long long r = 0ULL;
	for (int i = 63; i >= 0; --i) {
		r = (r << 1) | ((value >> i) & 1ULL);
		if (r >= base) {
			r -= base;
			q |= (1ULL << i);
		}
	}
	if (rem_out) *rem_out = (unsigned int)r;
	return q;
}

static char* utoa_impl(unsigned long long value, char* buffer, int base, bool uppercase) {
	static const char* digits_low = "0123456789abcdefghijklmnopqrstuvwxyz";
	static const char* digits_up  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char* digits = uppercase ? digits_up : digits_low;

	if (value == 0) {
		buffer[0] = '0';
		buffer[1] = '\0';
		return buffer;
	}

	int i = 0;
	while (value > 0) {
		unsigned rem;
		value = udivmod_u64(value, (unsigned)base, &rem);
		buffer[i++] = digits[rem];
	}
	// reverse
	for (int j = 0; j < i / 2; ++j) {
		char tmp = buffer[j];
		buffer[j] = buffer[i - 1 - j];
		buffer[i - 1 - j] = tmp;
	}
	buffer[i] = '\0';
	return buffer;
}

char* ulltoa(unsigned long long value, char* buffer, int base) {
	if (!validate_base(base)) return 0;
	if (!buffer) buffer = s_int_buffer;
	return utoa_impl(value, buffer, base, false);
}

char* ultoa(unsigned long value, char* buffer, int base) {
	return ulltoa((unsigned long long)value, buffer, base);
}

char* utoa(unsigned value, char* buffer, int base) {
	return ulltoa((unsigned long long)value, buffer, base);
}

// Signed temel: işareti ayıkla sonra utoa_impl
static char* sign_wrap(long long v, char* buffer, int base) {
	if (!buffer) buffer = s_int_buffer;
	if (!validate_base(base)) return 0;
	unsigned long long mag;
	bool neg = false;
	if (v < 0) { neg = true; mag = (unsigned long long)(-v); }
	else { mag = (unsigned long long)v; }
	char* start = buffer;
	if (neg) {
		*start++ = '-';
	}
	utoa_impl(mag, start, base, false);
	return buffer;
}

char* lltoa(long long value, char* buffer, int base) { return sign_wrap(value, buffer, base); }
char* ltoa(long value, char* buffer, int base) { return sign_wrap((long long)value, buffer, base); }
char* itoa(int value, char* buffer, int base) { return sign_wrap((long long)value, buffer, base); }

// Double -> string (basit fixed format). precision: 0..18 clamp.
char* dtoa(double value, char* buffer, int precision) {
	if (!buffer) buffer = s_double_buffer;
	if (precision < 0) precision = 0; else if (precision > 18) precision = 18;

	// Özel durumlar
	// NaN tespiti: value != value
	if (value != value) { buffer[0]='n';buffer[1]='a';buffer[2]='n';buffer[3]='\0'; return buffer; }
	// Sonsuz
	if (value > 1.0/0.0) { buffer[0]='i';buffer[1]='n';buffer[2]='f';buffer[3]='\0'; return buffer; }
	if (value < -1.0/0.0) { buffer[0]='-';buffer[1]='i';buffer[2]='n';buffer[3]='f';buffer[4]='\0'; return buffer; }

	// İşaret
	int pos = 0;
	bool negative = false;
	if (value < 0) { buffer[pos++]='-'; value = -value; negative = true; }

	// Tamsayı kısmı
	unsigned long long int_part = (unsigned long long)value;
	double frac = value - (double)int_part;

	// Tamsayı kısmını yaz
	char tmp[32];
	utoa_impl(int_part, tmp, 10, false);
	for (int i=0; tmp[i]; ++i) buffer[pos++] = tmp[i];

	if (precision == 0) {
		buffer[pos] = '\0';
		return buffer;
	}

	buffer[pos++] = '.';

	// Round için ölçek
	double scale = 1.0;
	for (int i=0;i<precision;i++) scale *= 10.0;
	double scaled = frac * scale + 0.5; // round
	unsigned long long frac_int = (unsigned long long)scaled;
	if (frac_int >= (unsigned long long)scale) {
		// rounding overflow (ör: 0.999 -> 1.000)
		frac_int = 0;
		int_part += 1ULL;
		// integer kısmını yeniden yazmak için buffer'ı en baştan dolduralım
		pos = 0;
		if (negative) buffer[pos++]='-';
		utoa_impl(int_part, tmp, 10, false);
		for (int i=0; tmp[i]; ++i) buffer[pos++] = tmp[i];
		buffer[pos++]='.';
	}

	// Frac kısmını zeros ile doldurarak sabit uzunlukta yaz
	char frac_buf[24];
	// Daha net uygulama: frac_int'i base10'da precision basamakla yaz (leading zero dahil)
	for (int i=precision-1; i>=0; --i) {
		unsigned digit = (unsigned)(frac_int % 10ULL);
		frac_buf[i] = (char)('0'+digit);
		frac_int /= 10ULL;
	}
	for (int i=0;i<precision;i++) buffer[pos++]=frac_buf[i];
	buffer[pos]='\0';
	return buffer;
}

