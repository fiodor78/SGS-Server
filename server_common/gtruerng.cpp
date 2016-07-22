/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

#include "headers_all.h"
#include "gtruerng.h"

/* Oryginalny kod zrodlowy Mersenne Twistera */

/*
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

//------------------------------------------------------------------------

MersenneTwister::MersenneTwister(int n, int r, int m, DWORD32 a)
{
	this->n = n;
	this->m = m;
	this->a = a;

	lower_mask = (1 << (32 - r)) - 1;
	upper_mask = ~lower_mask;

	mti = n + 1;
}

void MersenneTwister::InitGenrand(DWORD32 seed)
{
	mt[0]= seed & 0xffffffffU;
	for (mti = 1; mti < n; mti++)
	{
		mt[mti] = (1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
		/* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
		/* In the previous versions, MSBs of the seed affect   */
		/* only MSBs of the array mt[].                        */
		/* 2002/01/09 modified by Makoto Matsumoto             */
		mt[mti] &= 0xffffffffU;
		/* for >32 bit machines */
	}
}

void MersenneTwister::InitByArray(DWORD32 * init_key, int key_length)
{
	int i, j, k;
	InitGenrand(19650218U);
	i = 1; j = 0;
	k = max(n, key_length);
	for (; k; k--)
	{
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525U)) + init_key[j] + j; /* non linear */
		mt[i] &= 0xffffffffU; /* for WORDSIZE > 32 machines */
		i++; j++;
		if (i >= n)
		{
			mt[0] = mt[n - 1];
			i = 1;
		}
		if (j >= key_length)
			j = 0;
	}
	for (k = n - 1; k; k--)
	{
		mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941U)) - i; /* non linear */
		mt[i] &= 0xffffffffU; /* for WORDSIZE > 32 machines */
		i++;
		if (i >= n)
		{
			mt[0] = mt[n - 1];
			i = 1;
		}
	}

	mt[0] = 0x80000000U; /* MSB is 1; assuring non-zero initial array */
}

void MersenneTwister::GenerateRandomWords()
{
	DWORD32 y, mag01[2];
	int kk;

	mag01[0] = 0;
	mag01[1] = a;
	/* mag01[x] = x * MATRIX_A  for x=0,1 */

	if (mti == n + 1)   /* if init_genrand() has not been called, */
		InitGenrand(5489U); /* a default initial seed is used */

	for (kk = 0; kk < n - m; kk++)
	{
		y = (mt[kk] & upper_mask) | (mt[kk + 1] & lower_mask);
		mt[kk] = mt[kk + m] ^ (y >> 1) ^ mag01[y & 0x1U];
	}
	for (; kk < n-1; kk++) 
	{
		y = (mt[kk] & upper_mask) | (mt[kk + 1] & lower_mask);
		mt[kk] = mt[kk + (m - n)] ^ (y >> 1) ^ mag01[y & 0x1U];
	}
	y = (mt[n-1] & upper_mask) | (mt[0] & lower_mask);
	mt[n - 1] = mt[m - 1] ^ (y >> 1) ^ mag01[y & 0x1U];

	mti = 0;
}

/******************************************************************************************/

/*
	Zasada dzialania generatora liczb losowych:

		1. Korzystamy z dwoch Mersenne Twisterow:
			a) glowny: MT19937
			b) pomocniczy: MT11213B

		2. Funkcja MTX() zwraca nam 32-bitowa liczbe losowa wyliczona na podstawie
			wartosci zwroconych przez oba MT i zmiksowanych m.in. przy uzyciu techniki
			MacLaren-Marsaglia.

		3. Wlasciwa funkcja Rnd(n) zwraca liczbe losowa calkowita z przedzialu [0..n-1].
			Korzysta ona z funkcji MTX() i bierze pod uwage tyle bitow z konca jej wyniku ile jest
			potrzebnych do zapisania liczby n-1. Jesli otrzymana liczba jest <= n-1 to zwracamy
			ja jako szukana liczbe losowa. W przeciwnym wypadku powtarzamy procedure do skutku.
			W ten sposob uniezalezniamy sie od wszelkich zaburzen, ktore by wystepowaly gdybysmy
			uzywali funkcji modulo.

		4. Oba MT musza byc w miare czesto reseedowane (np. co 5 sekund). Co ten czas powinna
			byc wywolywana funkcja ReseedingTimeout().
			MT sa aktualnie seedowane przy uzyciu seedow odpowiednio 1792 i 256 bitowych.

		5. Seed musi byc stale modyfikowany poprzez dodawanie do niego entropii.
			Sluzy do tego funkcja AddEntropy().
			Jesli liczba bitow entropii dodanych do seeda od czasu ostatniego reseedowania
			jest mniejsza niz 3/4 jego rozmiaru to odwlekamy reseedowanie do czasu az
			wymagana liczbe bitow entropii dostaniemy.

		6. Oba MT sa poczatkowo inicjalizowane przez dane pochodzace z /dev/random i getpid()
			pod Linuxem lub QueryPerformanceCounter() i GetCurrentProcessID() pod Windows.
			Przy czym zakladamy, ze pierwsze reseedowanie nastapi na tyle szybko, ze funkcja Rnd()
			nie bedzie do tego czasu uzyta ani razu, ew. bardzo sporadycznie.

		7. Z generatora korzystamy poprzez funkcje Rnd(). Mozemy ja traktowac jakby	byla threadsafe.
			Wszelkie ew. konflikty moga zaburzyc stan generatora co nie przeszkadza nam w jego
			losowosci.

*/

/***************************************************************************/

GTrueRNG::GTrueRNG()
{
	Init();
}

void GTrueRNG::Init()
{
	DWORD32 initial_seed;

	force_reseeding = true;
	added_entropy_bits = 0;

	initial_seed = (DWORD32)time(NULL);

#ifndef LINUX

	LARGE_INTEGER highres_counter;

	if (QueryPerformanceCounter(&highres_counter))
		initial_seed ^= (DWORD32)highres_counter.LowPart;

	initial_seed ^= (GetCurrentProcessId() << 16);

#else

	int rand_fd;
	DWORD32 tmp;

	if ((rand_fd = open("/dev/random", O_RDONLY)) >= 0)
	{
		if (read(rand_fd, &tmp, 4) > 0)
			initial_seed ^= tmp;
		close(rand_fd);
	}

	if ((rand_fd = open("/dev/urandom", O_RDONLY)) >= 0)
	{
		if (read(rand_fd, &tmp, 4) > 0)
			initial_seed ^= tmp;
		close(rand_fd);
	}

	initial_seed ^= (getpid() << 16);

#endif

	//------------------------------------------------------------------------

	int i, j;
	unsigned char * p;
	DWORD32 v1, v2;

	mt_main.InitGenrand(initial_seed);
	mt_2nd.InitGenrand(initial_seed ^ 0x55555555); 

	for (i = 0; i < (int)sizeof(Marsaglia_buffer32) + (int)sizeof(Marsaglia_buffer16); i++)
	{
		if (i < (int)sizeof(Marsaglia_buffer32))
			p = ((unsigned char*)&Marsaglia_buffer32) + i;
		else
			p = ((unsigned char*)&Marsaglia_buffer16) + (i - (int)sizeof(Marsaglia_buffer32));

		v1 = 0;
		for (j = 0; j < 16; j++)
		{
			v1 <<= 1;
			v1 |= (mt_main.Rnd() >> 31);
		}
		v2 = mt_2nd.Rnd() >> 24;

		*p = (unsigned char)(((v1 >> 8) & ~v2) | ((v1 & 0xff) & v2));
	}

	for (i = 0; i < MAIN_MT_SEED_SIZE + SECONDARY_MT_SEED_SIZE; i++)
	{
		seed[(i + (initial_seed >> 3)) % (MAIN_MT_SEED_SIZE + SECONDARY_MT_SEED_SIZE)] = MTX();
	}
}

/***************************************************************************/

DWORD32 GTrueRNG::Rnd(DWORD32 n)
{
	DWORD32 mask, result;

	if (force_reseeding && added_entropy_bits > ((MAIN_MT_SEED_SIZE + SECONDARY_MT_SEED_SIZE) * 32 * 3) / 4)
	{
		Reseed();
		force_reseeding = false;
	}

	mask = 1;
	while (mask < n - 1)
		mask = (mask << 1) + 1;

	result = n;
	while (result >= n)
		result = MTX() & mask;

	return result;
}

void GTrueRNG::ReseedingTimeout()
{
	force_reseeding = true;
}

void GTrueRNG::Reseed()
{
	mt_main.InitByArray(&seed[0], MAIN_MT_SEED_SIZE);
	mt_2nd.InitByArray(&seed[MAIN_MT_SEED_SIZE], SECONDARY_MT_SEED_SIZE);
	added_entropy_bits = 0;
}

void GTrueRNG::AddEntropy(char * entropy_bits, int bits_count, EEntropyType type)
{
	int i, pos;

	for (i = 0; i < bits_count; i++)
	{
		pos = Rnd((MAIN_MT_SEED_SIZE + SECONDARY_MT_SEED_SIZE) * 32);

		if (((unsigned char)entropy_bits[i / 8] >> (i % 8)) & 1)
		{
			seed[pos / 32] ^= 1 << (pos % 32);
		}
	}

	added_entropy_bits += bits_count;
}

DWORD32 GTrueRNG::MTX()
{
	DWORD32 result, v1, v2, va, vb, vmask;
	int i;

	result = 0;
	for (i = 0; i < 4; i++)
	{
		v1 = mt_main.Rnd();
		v2 = mt_2nd.Rnd() & 0x1f;
		va = (v1 << v2) | (v1 >> (32 - v2));

		v1 = mt_main.Rnd();
		v2 = mt_2nd.Rnd() & 0x1f;
		vb = (v1 << v2) | (v1 >> (32 - v2));

		//----

		vmask = mt_2nd.Rnd();
		v2 = (va & ~vmask) | (vb & vmask);

		//----

		va = mt_2nd.Rnd();
		va >>= 24;
		v1 = Marsaglia_buffer32[va];
		Marsaglia_buffer32[va] = v2;

		//----

		vmask = mt_2nd.Rnd();
		vmask >>= 16;
		vb = ((v1 >> 16) & ~vmask) | ((v1 & 0xffff) & vmask);

		//----

		va = mt_2nd.Rnd();
		va >>= 24;
		v1 = (DWORD32)Marsaglia_buffer16[va];
		Marsaglia_buffer16[va] = (USHORT16)vb;

		//----

		vmask = mt_2nd.Rnd();
		vmask >>= 24;
		vb = ((v1 >> 8) & ~vmask) | ((v1 & 0xff) & vmask);

		//----

		result <<= 8;
		result |= (vb & 0xff);
	}

	return result;
}

/***************************************************************************/

GEntropyCache::GEntropyCache()
{
	data_dir = "";
}

void GEntropyCache::Destroy()
{
	entropy_buffer.Deallocate();
}

void GEntropyCache::Init(const char * data_dir, GMemoryManager * mgr_p, int buffer_size)
{
	this->data_dir = data_dir;
	entropy_buffer.Init(mgr_p);
	entropy_buffer.Allocate(buffer_size);
}

void GEntropyCache::FeedEntropy(GTrueRNG * rng, int bytes_count, bool reseed)
{
	char entropy_filename[256], new_filename[256];
	int base, i, num_read;
	FILE * plik;

	if ((int)entropy_buffer.ToParse() < bytes_count)
	{
		rng->AddEntropy(entropy_buffer.Parse(), entropy_buffer.ToParse() * 8, EENTROPY_CACHE);
		bytes_count -= entropy_buffer.ToParse();

		//--- Czyscimy bufor i wczytujemy nowe dane ---
		entropy_buffer.IncreaseParsed(entropy_buffer.ToParse());
		entropy_buffer.RemoveParsed();

		base = rng->Rnd(100);
		for (i = 0; i < 100; i++)
		{
			sprintf(entropy_filename, "%s/entropy%02d.bin", data_dir.c_str(), (base + i * 7) % 100);
#ifdef LINUX
			if (access(entropy_filename, 0) == 0)
#else
			if (_access(entropy_filename, 0) == 0)
#endif
			{
				break;
			}
		}

		if (i < 100)
		{
#ifdef LINUX
			sprintf(new_filename, "%s/entropy.bin.%d", data_dir.c_str(), getpid());
#else
			sprintf(new_filename, "%s/entropy.bin.%d", data_dir.c_str(), GetCurrentProcessId());
#endif
			if (rename(entropy_filename, new_filename) == 0)
			{
				if ((plik = fopen(new_filename, "rb")) != NULL)
				{
					num_read = fread(entropy_buffer.End(), 1, entropy_buffer.Free(), plik);
					entropy_buffer.IncreaseUsed(num_read);
					fclose(plik);
				}
#ifdef LINUX
				unlink(new_filename);
#else
				_unlink(new_filename);
#endif
			}
		}
	}

	rng->AddEntropy(entropy_buffer.Parse(), min((int)entropy_buffer.ToParse(), bytes_count) * 8, EENTROPY_CACHE);
	i = bytes_count + rng->Rnd(bytes_count / 3);
	entropy_buffer.IncreaseParsed(min((int)entropy_buffer.ToParse(), i));
	 
	if (reseed)
		rng->ReseedingTimeout();
}

/***************************************************************************/
// Funkcje zwiazane z osbsluga sprzetowego generatora liczb losowych

/*GHardwareRNG::GHardwareRNG()
{
	sg100_initialized = false;
}

void GHardwareRNG::Init()
{
#ifdef SERVICES
#ifdef LINUX
#endif
#endif
}

void GHardwareRNG::Destroy()
{
#ifdef SERVICES
#ifdef LINUX
	Global_Settings.generator_enable = false;
#endif
#endif
	sg100_initialized = false;
}

bool GHardwareRNG::IsSG100Running()
{
#ifdef SERVICES
#ifdef LINUX
	if (sg100_initialized && Global_Settings.Generator_Running)
	{
		return true;
	}
#endif
#endif
	return false;
}

DWORD32 GHardwareRNG::Rnd(DWORD32 n)
{
#ifdef SERVICES
#ifdef LINUX
	if (IsSG100Running())
	{
		return SG100_Random(n);
	}
#endif
#endif
	return truerng.Rnd(n);
}*/
/***************************************************************************/
