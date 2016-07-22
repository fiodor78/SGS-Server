/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

class MersenneTwister
{
public:
	MersenneTwister(int n, int r, int m, DWORD32 a);

	virtual DWORD32		Rnd() = 0;
	void				InitGenrand(DWORD32 seed);
	void				InitByArray(DWORD32 * init_key, int key_length);

protected:
	int			n, m;
	DWORD32		upper_mask, lower_mask;
	DWORD32		a;

	DWORD32		mt[630];			// MT19937 uzywa 624 slowa, MT11213B uzywa 351 slow
	int			mti;

	void		GenerateRandomWords();		// funkcja wypelnia tablice mt[]
};

class MT19937 : public MersenneTwister
{
public:
	MT19937() : MersenneTwister(624, 1, 397, 0x9908b0dfU) {};
	DWORD32		Rnd()
	{
		DWORD32 y;

		if (mti >= n)
			GenerateRandomWords();
		y = mt[mti++];

		/* Tempering */
		y ^= (y >> 11);
		y ^= (y << 7) & 0x9d2c5680U;
		y ^= (y << 15) & 0xefc60000U;
		y ^= (y >> 18);

		return y;
	};
};

class MT11213B : public MersenneTwister
{
public:
	MT11213B() : MersenneTwister(351, 13, 175, 0xccab8ee7U) {};
	DWORD32		Rnd()
	{
		DWORD32 y;

		if (mti >= n)
			GenerateRandomWords();
		y = mt[mti++];

		/* Tempering */
		y ^= (y >> 11);
		y ^= (y << 7) & 0x31b6ab00U;
		y ^= (y << 15) & 0xffe50000U;
		y ^= (y >> 17);

		return y;
	};
};

/***************************************************************************/

enum EEntropyType
{
	EENTROPY_UNKNOWN,
	EENTROPY_SG100,
	EENTROPY_NET_EVENT,
	EENTROPY_CACHE,
	EENTROPY_LAST,
};

// Rozmiary seedow dla Mersenne Twisterow w 32-bitowych DWORDach(!)
#define MAIN_MT_SEED_SIZE			56
#define SECONDARY_MT_SEED_SIZE		8

class GTrueRNG
{
public:
	GTrueRNG();

	DWORD32			Rnd(DWORD32 n);

	void			ReseedingTimeout();
	void			AddEntropy(char * entropy_bits, int bits_count, EEntropyType type);

private:
	bool			force_reseeding;
	int				added_entropy_bits;

	DWORD32			seed[MAIN_MT_SEED_SIZE + SECONDARY_MT_SEED_SIZE];

	DWORD32			Marsaglia_buffer32[256];
	USHORT16		Marsaglia_buffer16[256];

	void			Init();
	void			Reseed();

	DWORD32			MTX();

	class MT19937	mt_main;
	class MT11213B	mt_2nd;
};

/***************************************************************************/

class GEntropyCache
{
public:
	GEntropyCache();

	void Init(const char * data_dir, GMemoryManager * mgr_p, int buffer_size = K128);
	void Destroy();
	void FeedEntropy(GTrueRNG * rng, int bytes_count, bool reseed = false);

private:
	GMemory			entropy_buffer;
	string			data_dir;
};

/***************************************************************************/

/*class GHardwareRNG
{
public:
	GHardwareRNG();
	~GHardwareRNG() { Destroy(); };
	void            Init();
	bool			IsSG100Running();

	DWORD32         Rnd(DWORD32 n);

private:
	void			Destroy();

	bool			sg100_initialized;
};
*/
/***************************************************************************/

extern class GTrueRNG		truerng;
extern class GEntropyCache	entropy_cache;
